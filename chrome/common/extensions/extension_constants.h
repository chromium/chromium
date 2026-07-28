// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_

#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "extensions/common/constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_extension_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extension_urls {

// Field to use with webstore URL for tracking launch source.
inline constexpr char kWebstoreSourceField[] = "utm_source";

// Values to use with webstore URL launch source field.
inline constexpr char kLaunchSourceAppList[] = "chrome-app-launcher";
inline constexpr char kLaunchSourceAppListSearch[] =
    "chrome-app-launcher-search";
inline constexpr char kLaunchSourceAppListInfoDialog[] =
    "chrome-app-launcher-info-dialog";

}  // namespace extension_urls

namespace extension_misc {

// The extension id of the Calendar application.
inline constexpr char kCalendarAppId[] = "ejjicmeblgpmajnghnpcppodonldlgfn";

// The extension id of the Data Saver extension.
inline constexpr char kDataSaverExtensionId[] =
    "pfmgfdlgomnbgkofeojodiodmgpgmkac";

// The extension id of the Dictation Connector component extension.
inline constexpr char kDictationConnectorExtensionId[] =
    "kbglekiebdohdafflpmiejhbfdmjdbbe";

// The extension id of the Google Maps application.
inline constexpr char kGoogleMapsAppId[] = "lneaknkopdijkpnocmklfnjbeapigfbh";

// The extension id of the Google Photos application.
inline constexpr char kGooglePhotosAppId[] = "hcglmfcclpfgljeaiahehebeoaiicbko";

// The extension id of the Google Play Books application.
inline constexpr char kGooglePlayBooksAppId[] =
    "mmimngoggfoobjdlefbcabngfnmieonb";

// The extension id of the Google Play Movies application.
inline constexpr char kGooglePlayMoviesAppId[] =
    "gdijeikdkaembjbdobgfkoidjkpbmlkd";

// The extension id of the Google Play Music application.
inline constexpr char kGooglePlayMusicAppId[] =
    "icppfcnhkcmnfdhfhphakoifcfokfdhg";

// The extension id of the Google+ application.
inline constexpr char kGooglePlusAppId[] = "dlppkpafhbajpcmmoheippocdidnckmm";

// The extension id of the Text Editor application.
inline constexpr char kTextEditorAppId[] = "mmfbcljfglbokpmkimbfghdkjmjhdgbg";

// The extension id of the in-app payments support application.
inline constexpr char kInAppPaymentsSupportAppId[] =
    "nmmhkkegccagdldgiimedpiccmgmieda";

// The extension id of Perfetto UI extension.
inline constexpr char kPerfettoUIExtensionId[] =
    "lfmkphfpdbjijhpomgecfikhfohaoine";

// The extension id of virtual keyboard extension.
inline constexpr char kKeyboardExtensionId[] =
    "mppnpdlheglhdfmldimlhpnegondlapf";

// The extension id of the Gemini transition extension.
inline constexpr char kGlicExtensionId[] = "admccjkmockfdflocgggjfgdacdodkdf";

// The public key of the Gemini transition extension.
inline constexpr char kGlicPublicKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDZI0bgiASzP89xOCIcg2Ej0S4/"
    "2Xoi8htu1qh4M1o4V0Q8GiqJ2nU2xJ9dJYxpMQff9OtNJ3giNman1xodSImQEWtm+"
    "YVUWJKu8V5ZyGWRJTtyo0tQiyNOx/kf3/"
    "f43CtsAKtsbFqJy9i9ux7JmF+KoORP1fjBJs0zpOGXWHbqIQIDAQAB";

// The extension id of the Gemini testing extension.
inline constexpr char kGlicTestExtensionId[] =
    "oljbkhokcbpaencibijkoolhipplkeoc";

// The public key of the Gemini testing extension.
inline constexpr char kGlicTestPublicKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAmXEAMP+pPV/eGfsrDMku2ftK"
    "AbJ0JV4ao9jhq1+b36wHEDYShjUVdQjHw6ygAv35v8F8vu8xEKqqY/vguTD8u/Xz3g0l"
    "hytN9xCR/YUaJxLGbf8Vfz39LcQxg56pHddCTDOJqUOAw3Wuq1v5TYFpuHlC3XNBL8qj"
    "WsrwVbdrOsyIum9PBNyzKX2hxpYSWIf12TRixU+ZJoEhEnh8CWodZJ2tGbptMNl03hPp"
    "GydJeTv/Tug4U6dhOIEdWzkLhaX3znm+Nn+jj0lxvcnbN79H43e79ASt1P0IRWsVI79M"
    "Avt1oyQsAvfVp6L/qCxejjzZDD/Ps0HarahFk7LLLHBcRwIDAQAB";

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

// The extension id of the helper extension for Reading Mode to work on Google
// Docs.
inline constexpr char kReadingModeGDocsHelperExtensionId[] =
    "cjlaeehoipngghikfjogbdkpbdgebppb";
// The path to the the helper extension for Reading Mode to work on Google Docs.
inline constexpr char kReadingModeGDocsHelperExtensionPath[] = "accessibility";
// The name of the manifest file for the extension that enables Reading Mode to
// work on Google Docs.
inline constexpr base::FilePath::CharType
    kReadingModeGDocsHelperManifestFilename[] =
        FILE_PATH_LITERAL("reading_mode_gdocs_helper_manifest.json");
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
// The extension id of the google tts engine extension to use on-device natural
// Google voices.
inline constexpr char kTTSEngineExtensionId[] =
    "kfgdcmdikpmgdjhgfpbfgkomboamacbb";
inline constexpr char kComponentUpdaterTTSEngineExtensionId[] =
    "gjjabgpgjpampikjhjpfhneeoapjbjaf";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

// The states that an app can be in, as reported by chrome.app.installState
// and chrome.app.runningState.
inline constexpr char kAppStateNotInstalled[] = "not_installed";
inline constexpr char kAppStateInstalled[] = "installed";
inline constexpr char kAppStateDisabled[] = "disabled";
inline constexpr char kAppStateRunning[] = "running";
inline constexpr char kAppStateCannotRun[] = "cannot_run";
inline constexpr char kAppStateReadyToRun[] = "ready_to_run";

// The path part of the file system url used for media file systems.
inline constexpr char kMediaFileSystemPathPart[] = "_";

inline constexpr auto kBuiltInFirstPartyExtensionIds =
    std::to_array<const std::string_view>({
        kCalculatorAppId,
        kCalendarAppId,
        kDataSaverExtensionId,
        kDocsOfflineExtensionId,
        kGoogleDriveAppId,
        kGmailAppId,
        kGoogleDocsAppId,
        kGoogleMapsAppId,
        kGooglePhotosAppId,
        kGooglePlayBooksAppId,
        kGooglePlayMoviesAppId,
        kGooglePlayMusicAppId,
        kGooglePlusAppId,
        kGoogleSheetsAppId,
        kGoogleSlidesAppId,
        kTextEditorAppId,
        kInAppPaymentsSupportAppId,
#if BUILDFLAG(IS_CHROMEOS)
        kAssessmentAssistantExtensionId,
        kAccessibilityCommonExtensionId,
        kSelectToSpeakExtensionId,
        kSwitchAccessExtensionId,
        kFilesManagerAppId,
        kFirstRunDialogId,
        kEspeakSpeechSynthesisExtensionId,
        kGoogleSpeechSynthesisExtensionId,
#endif  // BUILDFLAG(IS_CHROMEOS)
        kReadingModeGDocsHelperExtensionId,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
        kTTSEngineExtensionId,
        kComponentUpdaterTTSEngineExtensionId,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    });

}  // namespace extension_misc

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
