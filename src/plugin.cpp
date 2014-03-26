/*
 * TFB Plugin
 * (c)oded by Joseph Sutton
 * shoutz: I'm too old for this.
 *
 *********************************
 *
 * CHANGELOG:
 *	3/10/2014		1.0
 *	> First final build
 *	3/10/2014		1.1
 *	> Added icon to channel option(s)
 *	3/23/2014		1.2
 *	> Remote posting FTW.
 */

// Socket shit
//#include <unistd.h>
#include <io.h>
#include <errno.h>
#include <WinSock2.h>
#include <WinSock.h>
#include <ws2tcpip.h>
//#include <arpa/inet.h>
//#include <netinet/in.h>
#include <sys/types.h>
#include <BaseTsd.h>

#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <list>
//#include <ctime>
//#include <fstream>
#include <sstream>
#include <assert.h>
#include "public_errors.h"
#include "public_errors_rare.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin.h"

// Le constants
#define MSG_SERVER_OFFLINE      "Could not connect to the server - it seems to be offline to me. :-("
#define MSG_INVALID_ADDRINFO    "Return value for getaddrinfo() is not what's expected... Debug, plox."
#define MSG_RECV_ERROR          "Could not recv() back from server."
#define PORT                    "80"
#define MAX_DATA_SIZE           4096
#define MAXLINE                 4096

typedef SSIZE_T ssize_t;

using namespace std;

static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 19

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128

static char* pluginID = NULL;

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result) {
	int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	*result = (char*)malloc(outlen);
	if(WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
		*result = NULL;
		return -1;
	}
	return 0;
}
#endif

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
#ifdef _WIN32
	/* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
	static char* result = NULL;  /* Static variable so it's allocated only once */
	if(!result) {
		const wchar_t* name = L"Taskforce Blackjack";
		if(wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
			result = "Taskforce Blackjack";  /* Conversion failed, fallback here */
		}
	}
	return result;
#else
	return "Taskforce Blackjack";
#endif
}

/* Plugin version */
const char* ts3plugin_version() {
    return "1.3";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Joseph Sutton";
}

/* Plugin description */
const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Various functionalities for Taskforce Blackjack. I.e., attendance.";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init() {
    char appPath[PATH_BUFSIZE];
    char resourcesPath[PATH_BUFSIZE];
    char configPath[PATH_BUFSIZE];
	char pluginPath[PATH_BUFSIZE];

    /* Your plugin init code here */
    printf("PLUGIN: init\n");

    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE);

	printf("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s\n", appPath, resourcesPath, configPath, pluginPath);

    return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
	/* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
	 * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
	 * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
    printf("PLUGIN: shutdown\n");

	/*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */

	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}
}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
	printf("PLUGIN: offersConfigure\n");
	/*
	 * Return values:
	 * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	 * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	 * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	 */
	return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
    printf("PLUGIN: configure\n");
}

/*
 * If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
 * automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
 * Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
 */
void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
	printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return "test";
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
	char buf[COMMAND_BUFSIZE];
	char *s, *param1 = NULL, *param2 = NULL;
	int i = 0;
	enum { CMD_NONE = 0, CMD_JOIN, CMD_COMMAND, CMD_SERVERINFO, CMD_CHANNELINFO, CMD_AVATAR, CMD_ENABLEMENU, CMD_SUBSCRIBE, CMD_UNSUBSCRIBE, CMD_SUBSCRIBEALL, CMD_UNSUBSCRIBEALL } cmd = CMD_NONE;
#ifdef _WIN32
	char* context = NULL;
#endif

	printf("PLUGIN: process command: '%s'\n", command);

	_strcpy(buf, COMMAND_BUFSIZE, command);
#ifdef _WIN32
	s = strtok_s(buf, " ", &context);
#else
	s = strtok(buf, " ");
#endif
	while(s != NULL) {
		if(i == 0) {
			if(!strcmp(s, "join")) {
				cmd = CMD_JOIN;
			} else if(!strcmp(s, "command")) {
				cmd = CMD_COMMAND;
			} else if(!strcmp(s, "serverinfo")) {
				cmd = CMD_SERVERINFO;
			} else if(!strcmp(s, "channelinfo")) {
				cmd = CMD_CHANNELINFO;
			} else if(!strcmp(s, "avatar")) {
				cmd = CMD_AVATAR;
			} else if(!strcmp(s, "enablemenu")) {
				cmd = CMD_ENABLEMENU;
			} else if(!strcmp(s, "subscribe")) {
				cmd = CMD_SUBSCRIBE;
			} else if(!strcmp(s, "unsubscribe")) {
				cmd = CMD_UNSUBSCRIBE;
			} else if(!strcmp(s, "subscribeall")) {
				cmd = CMD_SUBSCRIBEALL;
			} else if(!strcmp(s, "unsubscribeall")) {
				cmd = CMD_UNSUBSCRIBEALL;
			}
		} else if(i == 1) {
			param1 = s;
		} else {
			param2 = s;
		}
#ifdef _WIN32
		s = strtok_s(NULL, " ", &context);
#else
		s = strtok(NULL, " ");
#endif
		i++;
	}

	switch(cmd) {
		case CMD_NONE:
			return 1;  /* Command not handled by plugin */
		case CMD_JOIN:  /* /test join <channelID> [optionalCannelPassword] */
			if(param1) {
				uint64 channelID = (uint64)atoi(param1);
				char* password = param2 ? param2 : "";
				char returnCode[RETURNCODE_BUFSIZE];
				anyID myID;

				/* Get own clientID */
				if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
					ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
					break;
				}

				/* Create return code for requestClientMove function call. If creation fails, returnCode will be NULL, which can be
				 * passed into the client functions meaning no return code is used.
				 * Note: To use return codes, the plugin needs to register a plugin ID using ts3plugin_registerPluginID */
				ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);

				/* In a real world plugin, the returnCode should be remembered (e.g. in a dynamic STL vector, if it's a C++ plugin).
				 * onServerErrorEvent can then check the received returnCode, compare with the remembered ones and thus identify
				 * which function call has triggered the event and react accordingly. */

				/* Request joining specified channel using above created return code */
				if(ts3Functions.requestClientMove(serverConnectionHandlerID, myID, channelID, password, returnCode) != ERROR_ok) {
					ts3Functions.logMessage("Error requesting client move", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
				}
			} else {
				ts3Functions.printMessageToCurrentTab("Missing channel ID parameter.");
			}
			break;
		case CMD_COMMAND:  /* /test command <command> */
			if(param1) {
				/* Send plugin command to all clients in current channel. In this case targetIds is unused and can be NULL. */
				if(pluginID) {
					/* See ts3plugin_registerPluginID for how to obtain a pluginID */
					printf("PLUGIN: Sending plugin command to current channel: %s\n", param1);
					ts3Functions.sendPluginCommand(serverConnectionHandlerID, pluginID, param1, PluginCommandTarget_CURRENT_CHANNEL, NULL, NULL);
				} else {
					printf("PLUGIN: Failed to send plugin command, was not registered.\n");
				}
			} else {
				ts3Functions.printMessageToCurrentTab("Missing command parameter.");
			}
			break;
		case CMD_SERVERINFO: {  /* /test serverinfo */
			/* Query host, port and server password of current server tab.
			 * The password parameter can be NULL if the plugin does not want to receive the server password.
			 * Note: Server password is only available if the user has actually used it when connecting. If a user has
			 * connected with the permission to ignore passwords (b_virtualserver_join_ignore_password) and the password,
			 * was not entered, it will not be available.
			 * getServerConnectInfo returns 0 on success, 1 on error or if current server tab is disconnected. */
			char host[SERVERINFO_BUFSIZE];
			/*char password[SERVERINFO_BUFSIZE];*/
			char* password = NULL;  /* Don't receive server password */
			unsigned short port;
			if(!ts3Functions.getServerConnectInfo(serverConnectionHandlerID, host, &port, password, SERVERINFO_BUFSIZE)) {
				char msg[SERVERINFO_BUFSIZE];
				snprintf(msg, sizeof(msg), "Server Connect Info: %s:%d", host, port);
				ts3Functions.printMessageToCurrentTab(msg);
			} else {
				ts3Functions.printMessageToCurrentTab("No server connect info available.");
			}
			break;
		}
		case CMD_CHANNELINFO: {  /* /test channelinfo */
			/* Query channel path and password of current server tab.
			 * The password parameter can be NULL if the plugin does not want to receive the channel password.
			 * Note: Channel password is only available if the user has actually used it when entering the channel. If a user has
			 * entered a channel with the permission to ignore passwords (b_channel_join_ignore_password) and the password,
			 * was not entered, it will not be available.
			 * getChannelConnectInfo returns 0 on success, 1 on error or if current server tab is disconnected. */
			char path[CHANNELINFO_BUFSIZE];
			/*char password[CHANNELINFO_BUFSIZE];*/
			char* password = NULL;  /* Don't receive channel password */

			/* Get own clientID and channelID */
			anyID myID;
			uint64 myChannelID;
			if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
				ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
				break;
			}
			/* Get own channel ID */
			if(ts3Functions.getChannelOfClient(serverConnectionHandlerID, myID, &myChannelID) != ERROR_ok) {
				ts3Functions.logMessage("Error querying channel ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
				break;
			}

			/* Get channel connect info of own channel */
			if(!ts3Functions.getChannelConnectInfo(serverConnectionHandlerID, myChannelID, path, password, CHANNELINFO_BUFSIZE)) {
				char msg[CHANNELINFO_BUFSIZE];
				snprintf(msg, sizeof(msg), "Channel Connect Info: %s", path);
				ts3Functions.printMessageToCurrentTab(msg);
			} else {
				ts3Functions.printMessageToCurrentTab("No channel connect info available.");
			}
			break;
		}
		case CMD_AVATAR: {  /* /test avatar <clientID> */
			char avatarPath[PATH_BUFSIZE];
			anyID clientID = (anyID)atoi(param1);
			unsigned int error;

			memset(avatarPath, 0, PATH_BUFSIZE);
			error = ts3Functions.getAvatar(serverConnectionHandlerID, clientID, avatarPath, PATH_BUFSIZE);
			if(error == ERROR_ok) {  /* ERROR_ok means the client has an avatar set. */
				if(strlen(avatarPath)) {  /* Avatar path contains the full path to the avatar image in the TS3Client cache directory */
					printf("Avatar path: %s\n", avatarPath);
				} else { /* Empty avatar path means the client has an avatar but the image has not yet been cached. The TeamSpeak
						  * client will automatically start the download and call onAvatarUpdated when done */
					printf("Avatar not yet downloaded, waiting for onAvatarUpdated...\n");
				}
			} else if(error == ERROR_database_empty_result) {  /* Not an error, the client simply has no avatar set */
				printf("Client has no avatar\n");
			} else { /* Other error occured (invalid server connection handler ID, invalid client ID, file io error etc) */
				printf("Error getting avatar: %d\n", error);
			}
			break;
		}
		case CMD_ENABLEMENU:  /* /test enablemenu <menuID> <0|1> */
			if(param1) {
				int menuID = atoi(param1);
				int enable = param2 ? atoi(param2) : 0;
				ts3Functions.setPluginMenuEnabled(pluginID, menuID, enable);
			} else {
				ts3Functions.printMessageToCurrentTab("Usage is: /test enablemenu <menuID> <0|1>");
			}
			break;
		case CMD_SUBSCRIBE:  /* /test subscribe <channelID> */
			if(param1) {
				char returnCode[RETURNCODE_BUFSIZE];
				uint64 channelIDArray[2];
				channelIDArray[0] = (uint64)atoi(param1);
				channelIDArray[1] = 0;
				ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
				if(ts3Functions.requestChannelSubscribe(serverConnectionHandlerID, channelIDArray, returnCode) != ERROR_ok) {
					ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
				}
			}
			break;
		case CMD_UNSUBSCRIBE:  /* /test unsubscribe <channelID> */
			if(param1) {
				char returnCode[RETURNCODE_BUFSIZE];
				uint64 channelIDArray[2];
				channelIDArray[0] = (uint64)atoi(param1);
				channelIDArray[1] = 0;
				ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
				if(ts3Functions.requestChannelUnsubscribe(serverConnectionHandlerID, channelIDArray, NULL) != ERROR_ok) {
					ts3Functions.logMessage("Error unsubscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
				}
			}
			break;
		case CMD_SUBSCRIBEALL: {  /* /test subscribeall */
			char returnCode[RETURNCODE_BUFSIZE];
			ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
			if(ts3Functions.requestChannelSubscribeAll(serverConnectionHandlerID, returnCode) != ERROR_ok) {
				ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
			}
			break;
		}
		case CMD_UNSUBSCRIBEALL: {  /* /test unsubscribeall */
			char returnCode[RETURNCODE_BUFSIZE];
			ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
			if(ts3Functions.requestChannelUnsubscribeAll(serverConnectionHandlerID, returnCode) != ERROR_ok) {
				ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
			}
			break;
		}
	}

	return 0;  /* Plugin handled command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
    printf("PLUGIN: currentServerConnectionChanged %llu (%llu)\n", (long long unsigned int)serverConnectionHandlerID, (long long unsigned int)ts3Functions.getCurrentServerConnectionHandlerID());
}

/*
 * Implement the following three functions when the plugin should display a line in the server/channel/client info.
 * If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
 */

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle() {
	return "";
}

/*
 * Dynamic content shown in the right column in the info frame. Memory for the data string needs to be allocated in this
 * function. The client will call ts3plugin_freeMemory once done with the string to release the allocated memory again.
 * Check the parameter "type" if you want to implement this feature only for specific item types. Set the parameter
 * "data" to NULL to have the client ignore the info data.
 */
void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data) {
	char* name;

	/* For demonstration purpose, display the name of the currently selected server, channel or client. */
	switch(type) {
		case PLUGIN_SERVER:
			if(ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &name) != ERROR_ok) {
				printf("Error getting virtual server name\n");
				return;
			}
			break;
		case PLUGIN_CHANNEL:
			if(ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, id, CHANNEL_NAME, &name) != ERROR_ok) {
				printf("Error getting channel name\n");
				return;
			}
			break;
		case PLUGIN_CLIENT:
			if(ts3Functions.getClientVariableAsString(serverConnectionHandlerID, (anyID)id, CLIENT_NICKNAME, &name) != ERROR_ok) {
				printf("Error getting client nickname\n");
				return;
			}
			break;
		default:
			printf("Invalid item type: %d\n", type);
			data = NULL;  /* Ignore */
			return;
	}

	*data = (char*)malloc(INFODATA_BUFSIZE * sizeof(char));  /* Must be allocated in the plugin! */
	//snprintf(*data, INFODATA_BUFSIZE, "The nickname is [I]\"%s\"[/I]", name);  /* bbCode is supported. HTML is not supported */
	ts3Functions.freeMemory(name);
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/*
 * Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
 * the user manually disabled it in the plugin dialog.
 * This function is optional. If missing, no autoload is assumed.
 */
int ts3plugin_requestAutoload() {
	return 0;  /* 1 = request autoloaded, 0 = do not request autoload */
}

/* Helper function to create a menu item */
static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon) {
	struct PluginMenuItem* menuItem = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
	menuItem->type = type;
	menuItem->id = id;
	_strcpy(menuItem->text, PLUGIN_MENU_BUFSZ, text);
	_strcpy(menuItem->icon, PLUGIN_MENU_BUFSZ, icon);
	return menuItem;
}

/* Some makros to make the code to create menu items a bit more readable */
#define BEGIN_CREATE_MENUS(x) const size_t sz = x + 1; size_t n = 0; *menuItems = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * sz);
#define CREATE_MENU_ITEM(a, b, c, d) (*menuItems)[n++] = createMenuItem(a, b, c, d);
#define END_CREATE_MENUS (*menuItems)[n++] = NULL; assert(n == sz);

/*
 * Menu IDs for this plugin. Pass these IDs when creating a menuitem to the TS3 client. When the menu item is triggered,
 * ts3plugin_onMenuItemEvent will be called passing the menu ID of the triggered menu item.
 * These IDs are freely choosable by the plugin author. It's not really needed to use an enum, it just looks prettier.
 */
enum {
	MENU_ID_CHANNEL_1
};

/*
 * Initialize plugin menus.
 * This function is called after ts3plugin_init and ts3plugin_registerPluginID. A pluginID is required for plugin menus to work.
 * Both ts3plugin_registerPluginID and ts3plugin_freeMemory must be implemented to use menus.
 * If plugin menus are not used by a plugin, do not implement this function or return NULL.
 */
void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
	/*
	 * Create the menus
	 * There are three types of menu items:
	 * - PLUGIN_MENU_TYPE_CLIENT:  Client context menu
	 * - PLUGIN_MENU_TYPE_CHANNEL: Channel context menu
	 * - PLUGIN_MENU_TYPE_GLOBAL:  "Plugins" menu in menu bar of main window
	 *
	 * Menu IDs are used to identify the menu item when ts3plugin_onMenuItemEvent is called
	 *
	 * The menu text is required, max length is 128 characters
	 *
	 * The icon is optional, max length is 128 characters. When not using icons, just pass an empty string.
	 * Icons are loaded from a subdirectory in the TeamSpeak client plugins folder. The subdirectory must be named like the
	 * plugin filename, without dll/so/dylib suffix
	 * e.g. for "test_plugin.dll", icon "1.png" is loaded from <TeamSpeak 3 Client install dir>\plugins\test_plugin\1.png
	 */

	BEGIN_CREATE_MENUS(1);  /* IMPORTANT: Number of menu items must be correct! */
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_1, "Gather Attendance", "tfb.png");
	END_CREATE_MENUS;  /* Includes an assert checking if the number of menu items matched */

	/*
	 * Specify an optional icon for the plugin. This icon is used for the plugins submenu within context and main menus
	 * If unused, set menuIcon to NULL
	 */
	*menuIcon = (char*)malloc(PLUGIN_MENU_BUFSZ * sizeof(char));
	_strcpy(*menuIcon, PLUGIN_MENU_BUFSZ, "tfb.png");

	/*
	 * Menus can be enabled or disabled with: ts3Functions.setPluginMenuEnabled(pluginID, menuID, 0|1);
	 * Test it with plugin command: /test enablemenu <menuID> <0|1>
	 * Menus are enabled by default. Please note that shown menus will not automatically enable or disable when calling this function to
	 * ensure Qt menus are not modified by any thread other the UI thread. The enabled or disable state will change the next time a
	 * menu is displayed.
	 */
	/* For example, this would disable MENU_ID_GLOBAL_2: */
	/* ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_GLOBAL_2, 0); */

	/* All memory allocated in this function will be automatically released by the TeamSpeak client later by calling ts3plugin_freeMemory */
}

/* Helper function to create a hotkey */
static struct PluginHotkey* createHotkey(const char* keyword, const char* description) {
	struct PluginHotkey* hotkey = (struct PluginHotkey*)malloc(sizeof(struct PluginHotkey));
	_strcpy(hotkey->keyword, PLUGIN_HOTKEY_BUFSZ, keyword);
	_strcpy(hotkey->description, PLUGIN_HOTKEY_BUFSZ, description);
	return hotkey;
}

/* Some makros to make the code to create hotkeys a bit more readable */
#define BEGIN_CREATE_HOTKEYS(x) const size_t sz = x + 1; size_t n = 0; *hotkeys = (struct PluginHotkey**)malloc(sizeof(struct PluginHotkey*) * sz);
#define CREATE_HOTKEY(a, b) (*hotkeys)[n++] = createHotkey(a, b);
#define END_CREATE_HOTKEYS (*hotkeys)[n++] = NULL; assert(n == sz);

/*
 * Initialize plugin hotkeys. If your plugin does not use this feature, this function can be omitted.
 * Hotkeys require ts3plugin_registerPluginID and ts3plugin_freeMemory to be implemented.
 * This function is automatically called by the client after ts3plugin_init.
 */
//void ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys) {
	/* Register hotkeys giving a keyword and a description.
	 * The keyword will be later passed to ts3plugin_onHotkeyEvent to identify which hotkey was triggered.
	 * The description is shown in the clients hotkey dialog. */

	// Joseph - 3/10/2014 :: Omitted function procedures.

	//BEGIN_CREATE_HOTKEYS(3);  /* Create 3 hotkeys. Size must be correct for allocating memory. */
	//CREATE_HOTKEY("keyword_1", "Test hotkey 1");
	//CREATE_HOTKEY("keyword_2", "Test hotkey 2");
	//CREATE_HOTKEY("keyword_3", "Test hotkey 3");
	//END_CREATE_HOTKEYS;

	/* The client will call ts3plugin_freeMemory to release all allocated memory */
//}

/**
 * Determine whether to use IPv4 or IPv6, depending on what socket
 * type we're using. Source: beej.us (only because I didn't want to nest type-casts...)
 */
void *get_in_addr(struct sockaddr* sa) {
	if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
 * Convert integer to std::string
 */
std::string toString(int x) {
	std::stringstream ss;
	ss << x;

	return ss.str();
}

/**
 * Do the actual work with the socket.
 */
ssize_t processHttp( int iSock, std::string sHostname, std::string sPage, std::string sPost ) {
		string          sHeaders;
		ssize_t         n;
		//char            cLineRecvd[MAXLINE + 1];

		// Post string length
		int iLength = strlen(sPost.c_str());
		string strPostLen = toString(iLength);

		// Header generation
		sHeaders += "POST " + sPage + " HTTP/1.1\r\n";
		sHeaders += "Host: " + sHostname + "\r\n";
		sHeaders += "Content-type: application/x-www-form-urlencoded\r\n";
		sHeaders += "Content-length: " + strPostLen + "\r\n";
		sHeaders += "Connection: close\r\n\r\n";
		sHeaders += sPost + "\r\n";

		send( iSock, sHeaders.c_str(), strlen(sHeaders.c_str()), 0 );

		return n;
}

/*
 * Called when a plugin menu item (see ts3plugin_initMenus) is triggered. Optional function, when not using plugin menus, do not implement this.
 * 
 * Parameters:
 * - serverConnectionHandlerID: ID of the current server tab
 * - type: Type of the menu (PLUGIN_MENU_TYPE_CHANNEL, PLUGIN_MENU_TYPE_CLIENT or PLUGIN_MENU_TYPE_GLOBAL)
 * - menuItemID: Id used when creating the menu item
 * - selectedItemID: Channel or Client ID in the case of PLUGIN_MENU_TYPE_CHANNEL and PLUGIN_MENU_TYPE_CLIENT. 0 for PLUGIN_MENU_TYPE_GLOBAL.
 */
void ts3plugin_onMenuItemEvent(uint64 scHandlerID, enum PluginMenuType type, int menuItemID, uint64 channelId) {
	//printf("PLUGIN: onMenuItemEvent: serverConnectionHandlerID=%llu, type=%d, menuItemID=%d, selectedItemID=%llu\n", (long long unsigned int)serverConnectionHandlerID, type, menuItemID, (long long unsigned int)selectedItemID);
	switch(type) {
		case PLUGIN_MENU_TYPE_CHANNEL:
			/* Channel contextmenu item was triggered. selectedItemID is the channelID of the selected channel */
			switch(menuItemID) {
				case MENU_ID_CHANNEL_1:
					anyID* cList;
					
					if( ts3Functions.getChannelClientList( scHandlerID, channelId, &cList ) == ERROR_ok ) {
						string postData = "user[]=";
						list<string> users;

						for( int i = 0; cList[i]; ++i ) {
							char *name = new char[TS3_MAX_SIZE_CLIENT_NICKNAME_NONSDK];

							if( ts3Functions.getClientDisplayName( scHandlerID, cList[i], name, TS3_MAX_SIZE_CLIENT_NICKNAME_NONSDK ) == ERROR_ok ) {
								postData.append(name);
								postData.append("&user[]=");
								users.push_back(name);
							}

						}

						WSADATA wsaData;

						if( WSAStartup(MAKEWORD(1,1), &wsaData) != 0 ) {
							fprintf( stderr, "WSAStartup failed.\n");
							exit(1);
						}

						string					sHostname = "www.tf-blackjack.com";
						string					sPage = "/misc/attendance.php";
						int                     iSock;
						int                     iBytes;
						int                     iRetVal;
						char                    cNetLen[INET6_ADDRSTRLEN];
						char                    cBuffer[MAX_DATA_SIZE];
						struct addrinfo         aiHints;
						struct addrinfo*        aiServInfo;
						struct addrinfo*        aiP;
						const char*             cHostname = sHostname.c_str();
						
						 // Allocate thine memory
						memset( &aiHints, 0, sizeof( aiHints ));

						aiHints.ai_family       = AF_UNSPEC;
						aiHints.ai_socktype     = SOCK_STREAM;

						if( (iRetVal = getaddrinfo(cHostname, PORT, &aiHints, &aiServInfo)) != 0 ) {
								cout << MSG_INVALID_ADDRINFO << endl;
								return;
						}

						// Of all the kings men, only one shall be chosen
						for( aiP = aiServInfo; aiP != NULL; aiP = aiP->ai_next ) {

								// Attempt this socket
								if( (iSock = socket( aiP->ai_family, aiP->ai_socktype, aiP->ai_protocol )) == -1 ) {
										continue;
								}

								// Test thine connection to thine socket, m8
								if( connect( iSock, aiP->ai_addr, aiP->ai_addrlen ) == -1 ) {
										closesocket( iSock );
										continue;
								}

								break;
						}

						if( aiP == NULL ) {
								cout << MSG_SERVER_OFFLINE << endl;
								return;
						}

						// Octet formatting (IP6/IP4)
						inet_ntop( aiP->ai_family, get_in_addr((struct sockaddr*)aiP->ai_addr), cNetLen, sizeof( cNetLen ));

						// Free up some space (like we need to, ha)
						freeaddrinfo( aiServInfo );

						// Woo!
						processHttp( iSock, sHostname, sPage, postData );

						// Close thine sock
						closesocket( iSock );

						// Output confirmation
						list<string>::iterator it;
						string message = "Success! The following users were submitted: ";
						int x=0, iSize = users.size();

						for( it = users.begin(); it != users.end(); ++it ) {
							message.append( *it );

							x++;

							if( x < iSize ) {
								message.append(", ");
							}
						}

						const char *cMsg = message.c_str();

						ts3Functions.printMessageToCurrentTab( cMsg );

					}

					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}
