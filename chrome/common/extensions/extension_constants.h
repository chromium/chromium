// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_

#include <stdint.h>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "url/gurl.h"

namespace extension_urls {

// Field to use with webstore URL for tracking launch source.
extern const char kWebstoreSourceField[];

// Values to use with webstore URL launch source field.
extern const char kLaunchSourceAppList[];
extern const char kLaunchSourceAppListSearch[];
extern const char kLaunchSourceAppListInfoDialog[];

}  // namespace extension_urls

namespace extension_misc {

// The extension id of the Calendar application.
extern const char kCalendarAppId[];

// The extension id of the Data Saver extension.
extern const char kDataSaverExtensionId[];

// The extension id of the Google Docs Offline extension.
extern const char kDocsOfflineExtensionId[];

// The extension id of the Google Maps application.
extern const char kGoogleMapsAppId[];

// The extension id of the Google Photos application.
extern const char kGooglePhotosAppId[];

// The extension id of the Google Play Books application.
extern const char kGooglePlayBooksAppId[];

// The extension id of the Google Play Movies application.
extern const char kGooglePlayMoviesAppId[];

// The extension id of the Google Play Music application.
extern const char kGooglePlayMusicAppId[];

// The extension id of the Google+ application.
extern const char kGooglePlusAppId[];

// The extension id of the Identity API UI application.
extern const char kIdentityApiUiAppId[];

// The extension id of the Text Editor application.
extern const char kTextEditorAppId[];

// The extension id of the in-app payments support application.
extern const char kInAppPaymentsSupportAppId[];

// The extension id of virtual keyboard extension.
extern const char kKeyboardExtensionId[];

// A list of all the first party extension IDs, last entry is null.
extern const char* const kBuiltInFirstPartyExtensionIds[];

// The buckets used for app launches.
enum AppLaunchBucket {
  // Launch from NTP apps section while maximized.
  APP_LAUNCH_NTP_APPS_MAXIMIZED,

  // Launch from NTP apps section while collapsed.
  APP_LAUNCH_NTP_APPS_COLLAPSED,

  // Launch from NTP apps section while in menu mode.
  APP_LAUNCH_NTP_APPS_MENU,

  // Launch from NTP most visited section in any mode.
  APP_LAUNCH_NTP_MOST_VISITED,

  // Launch from NTP recently closed section in any mode.
  APP_LAUNCH_NTP_RECENTLY_CLOSED,

  // App link clicked from bookmark bar.
  APP_LAUNCH_BOOKMARK_BAR,

  // Nvigated to an app from within a web page (like by clicking a link).
  APP_LAUNCH_CONTENT_NAVIGATION,

  // Launch from session restore.
  APP_LAUNCH_SESSION_RESTORE,

  // Autolaunched at startup, like for pinned tabs.
  APP_LAUNCH_AUTOLAUNCH,

  // Launched from omnibox app links.
  APP_LAUNCH_OMNIBOX_APP,

  // App URL typed directly into the omnibox (w/ instant turned off).
  APP_LAUNCH_OMNIBOX_LOCATION,

  // Navigate to an app URL via instant.
  APP_LAUNCH_OMNIBOX_INSTANT,

  // Launch via chrome.management.launchApp.
  APP_LAUNCH_EXTENSION_API,

  // Launch an app via a shortcut. This includes using the --app or --app-id
  // command line arguments, or via an app shim process on Mac.
  APP_LAUNCH_CMD_LINE_APP,

  // App launch by passing the URL on the cmd line (not using app switches).
  APP_LAUNCH_CMD_LINE_URL,

  // User clicked web store launcher on NTP.
  APP_LAUNCH_NTP_WEBSTORE,

  // App launched after the user re-enabled it on the NTP.
  APP_LAUNCH_NTP_APP_RE_ENABLE,

  // URL launched using the --app cmd line option, but the URL does not
  // correspond to an installed app. These launches are left over from a
  // feature that let you make desktop shortcuts from the file menu.
  APP_LAUNCH_CMD_LINE_APP_LEGACY,

  // User clicked web store link on the NTP footer.
  APP_LAUNCH_NTP_WEBSTORE_FOOTER,

  // User clicked [+] icon in apps page.
  APP_LAUNCH_NTP_WEBSTORE_PLUS_ICON,

  // User clicked icon in app launcher main view.
  APP_LAUNCH_APP_LIST_MAIN,

  // User clicked app launcher search result.
  APP_LAUNCH_APP_LIST_SEARCH,

  // User clicked the chrome app icon from the app launcher's main view.
  APP_LAUNCH_APP_LIST_MAIN_CHROME,

  // User clicked the webstore icon from the app launcher's main view.
  APP_LAUNCH_APP_LIST_MAIN_WEBSTORE,

  // User clicked the chrome app icon from the app launcher's search view.
  APP_LAUNCH_APP_LIST_SEARCH_CHROME,

  // User clicked the webstore icon from the app launcher's search view.
  APP_LAUNCH_APP_LIST_SEARCH_WEBSTORE,
  APP_LAUNCH_BUCKET_BOUNDARY,
  APP_LAUNCH_BUCKET_INVALID
};

#if BUILDFLAG(IS_CHROMEOS)
// The extension id of the Assessment Assistant extension.
extern const char kAssessmentAssistantExtensionId[];
// The extension id of the extension responsible for providing chromeos perks.
extern const char kEchoExtensionId[];
// The extension id of the Gnubby chrome app.
extern const char kGnubbyAppId[];
// The extension id of the new v3 Gnubby extension.
extern const char kGnubbyV3ExtensionId[];
// The extension id of the GCSE.
extern const char kGCSEExtensionId[];
// The extension id of the Contact Center Insights chrome component extension.
extern const char kContactCenterInsightsExtensionId[];
// The extension id of the Desk API chrome component extension.
extern const char kDeskApiExtensionId[];
// The extension id of the Bruschetta Security Key Forwarder extension.
extern const char kBruSecurityKeyForwarderExtensionId[];
// The extension id of the OneDrive FS external component extension.
extern const char kODFSExtensionId[];
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
// The extension id of the Accessibility Common extension.
extern const char kAccessibilityCommonExtensionId[];
// Path to preinstalled Accessibility Common extension (relative to
// |chrome::DIR_RESOURCES|).
extern const char kAccessibilityCommonExtensionPath[];
// The manifest filename of the Accessibility Common extension.
extern const char kAccessibilityCommonManifestFilename[];
// The guest manifest filename of the Accessibility Common extension.
extern const char kAccessibilityCommonGuestManifestFilename[];
// Path to preinstalled ChromeVox screen reader extension (relative to
// |chrome::DIR_RESOURCES|).
extern const char kChromeVoxExtensionPath[];
// The manifest filename of the ChromeVox extension.
extern const char kChromeVoxManifestFilename[];
// The guest manifest filename of the ChromeVox extension.
extern const char kChromeVoxGuestManifestFilename[];
// The path to the ChromeVox extension's options page.
extern const char kChromeVoxOptionsPath[];
// The extension id of the Enhanced network TTS engine extension.
extern const char kEnhancedNetworkTtsExtensionId[];
// Path to preinstalled Enhanced network TTS engine extension (relative to
// |chrome::DIR_RESOURCES|).
extern const char kEnhancedNetworkTtsExtensionPath[];
// The manifest filename of the Enhanced network TTS engine extension.
extern const char kEnhancedNetworkTtsManifestFilename[];
// The guest manifest filename of the Enhanced network TTS engine extension.
extern const char kEnhancedNetworkTtsGuestManifestFilename[];
// The extension id of the Select-to-speak extension.
extern const char kSelectToSpeakExtensionId[];
// Path to preinstalled Select-to-speak extension (relative to
// |chrome::DIR_RESOURCES|).
extern const char kSelectToSpeakExtensionPath[];
// The manifest filename of the Select to Speak extension.
extern const char kSelectToSpeakManifestFilename[];
// The guest manifest filename of the Select to Speak extension.
extern const char kSelectToSpeakGuestManifestFilename[];
// The extension id of the Switch Access extension.
extern const char kSwitchAccessExtensionId[];
// Path to preinstalled Switch Access extension (relative to
// |chrome::DIR_RESOURCES|).
extern const char kSwitchAccessExtensionPath[];
// The manifest filename of the Switch Access extension.
extern const char kSwitchAccessManifestFilename[];
// The guest manifest filename of the Switch Access extension.
extern const char kSwitchAccessGuestManifestFilename[];
// Name of the manifest file in an extension when a special manifest is used
// for guest mode.
extern const char kGuestManifestFilename[];
// The extension id of the first run dialog application.
extern const char kFirstRunDialogId[];
// Path to preinstalled Google speech synthesis extension.
extern const char kGoogleSpeechSynthesisExtensionPath[];
// The extension id of the Google speech synthesis extension.
extern const char kGoogleSpeechSynthesisExtensionId[];
// The path to the Google speech synthesis extension's options page.
extern const char kGoogleSpeechSynthesisOptionsPath[];
// Path to preinstalled eSpeak-NG speech synthesis extension.
extern const char kEspeakSpeechSynthesisExtensionPath[];
// The extension id of the eSpeak-NG speech synthesis extension.
extern const char kEspeakSpeechSynthesisExtensionId[];
// The path to the eSpeak-NG speech synthesis extension's options page.
extern const char kEspeakSpeechSynthesisOptionsPath[];
// The extension id of official HelpApp extension.
extern const char kHelpAppExtensionId[];
#endif

// What causes an extension to be installed? Used in histograms, so don't
// change existing values.
enum CrxInstallCause {
  INSTALL_CAUSE_UNSET = 0,
  INSTALL_CAUSE_USER_DOWNLOAD,
  INSTALL_CAUSE_UPDATE,
  INSTALL_CAUSE_EXTERNAL_FILE,
  INSTALL_CAUSE_AUTOMATION,
  NUM_INSTALL_CAUSES
};

// The states that an app can be in, as reported by chrome.app.installState
// and chrome.app.runningState.
extern const char kAppStateNotInstalled[];
extern const char kAppStateInstalled[];
extern const char kAppStateDisabled[];
extern const char kAppStateRunning[];
extern const char kAppStateCannotRun[];
extern const char kAppStateReadyToRun[];

// The path part of the file system url used for media file systems.
extern const char kMediaFileSystemPathPart[];

// The key name of extension request timestamp used by the
// prefs::kCloudExtensionRequestIds preference.
extern const char kExtensionRequestTimestamp[];

// The key name of the extension workflow request justification used by the
// prefs::kCloudExtensionRequestIds preference.
extern const char kExtensionWorkflowJustification[];
}  // namespace extension_misc

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
