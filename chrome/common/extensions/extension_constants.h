// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "url/gurl.h"

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

// The extension id of virtual keyboard extension.
inline constexpr char kKeyboardExtensionId[] =
    "mppnpdlheglhdfmldimlhpnegondlapf";

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
inline constexpr char kAssessmentAssistantExtensionId[] =
    "gndmhdcefbhlchkhipcnnbkcmicncehk";
// The extension id of the extension responsible for providing chromeos perks.
inline constexpr char kEchoExtensionId[] = "kddnkjkcjddckihglkfcickdhbmaodcn";
// The extension id of the Gnubby chrome app.
inline constexpr char kGnubbyAppId[] = "beknehfpfkghjoafdifaflglpjkojoco";
// The extension id of the new v3 Gnubby extension.
inline constexpr char kGnubbyV3ExtensionId[] =
    "lfboplenmmjcmpbkeemecobbadnmpfhi";
// The extension id of the GCSE.
inline constexpr char kGCSEExtensionId[] = "cfmgaohenjcikllcgjpepfadgbflcjof";
// The extension id of the Contact Center Insights chrome component extension.
inline constexpr char kContactCenterInsightsExtensionId[] =
    "oebfonohdfogiaaaelfmjlkjbgdbaahf";
// The extension id of the Desk API chrome component extension.
inline constexpr char kDeskApiExtensionId[] =
    "kflgdebkpepnpjobkdfeeipcjdahoomc";
// The extension id of the Bruschetta Security Key Forwarder extension.
inline constexpr char kBruSecurityKeyForwarderExtensionId[] =
    "lcooaekmckohjjnpaaokodoepajbnill";
// The extension id of the OneDrive FS external component extension.
inline constexpr char kODFSExtensionId[] = "gnnndjlaomemikopnjhhnoombakkkkdg";
// The extension id of Perfetto UI extension.
inline constexpr char kPerfettoUIExtensionId[] =
    "lfmkphfpdbjijhpomgecfikhfohaoine";
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
// The extension id of the Accessibility Common extension.
inline constexpr char kAccessibilityCommonExtensionId[] =
    "egfdjlfmgnehecnclamagfafdccgfndp";
// Path to preinstalled Accessibility Common extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kAccessibilityCommonExtensionPath[] =
    "chromeos/accessibility";
// The manifest filename of the Accessibility Common extension.
inline constexpr char kAccessibilityCommonManifestFilename[] =
    "accessibility_common_manifest.json";
// The manifest v3 filename of the Accessibility Common extension.
inline constexpr char kAccessibilityCommonManifestV3Filename[] =
    "accessibility_common_manifest_v3.json";
// The guest manifest filename of the Accessibility Common extension.
inline constexpr char kAccessibilityCommonGuestManifestFilename[] =
    "accessibility_common_manifest_guest.json";
// The guest manifest v3 filename of the Accessibility Common extension.
inline constexpr char kAccessibilityCommonGuestManifestV3Filename[] =
    "accessibility_common_manifest_guest_v3.json";
// Path to preinstalled ChromeVox screen reader extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kChromeVoxExtensionPath[] = "chromeos/accessibility";
// The manifest filename of the ChromeVox extension.
inline constexpr char kChromeVoxManifestFilename[] = "chromevox_manifest.json";
// The manifest v3 filename of the ChromeVox extension.
inline constexpr char kChromeVoxManifestV3Filename[] =
    "chromevox_manifest_v3.json";
// The guest manifest filename of the ChromeVox extension.
inline constexpr char kChromeVoxGuestManifestFilename[] =
    "chromevox_manifest_guest.json";
// The guest manifest v3 filename of the ChromeVox extension.
inline constexpr char kChromeVoxGuestManifestV3Filename[] =
    "chromevox_manifest_guest_v3.json";
// The path to the ChromeVox extension's options page.
inline constexpr char kChromeVoxOptionsPath[] =
    "/chromevox/options/options.html";
// The extension id of the Enhanced network TTS engine extension.
inline constexpr char kEnhancedNetworkTtsExtensionId[] =
    "jacnkoglebceckolkoapelihnglgaicd";
// Path to preinstalled Enhanced network TTS engine extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kEnhancedNetworkTtsExtensionPath[] =
    "chromeos/accessibility";
// The manifest filename of the Enhanced network TTS engine extension.
inline constexpr char kEnhancedNetworkTtsManifestFilename[] =
    "enhanced_network_tts_manifest.json";
// The manifest v3 filename of the Enhanced network TTS engine extension.
inline constexpr char kEnhancedNetworkTtsManifestV3Filename[] =
    "enhanced_network_tts_manifest_v3.json";
// The guest manifest filename of the Enhanced network TTS engine extension.
inline constexpr char kEnhancedNetworkTtsGuestManifestFilename[] =
    "enhanced_network_tts_manifest_guest.json";
// The guest manifest v3 filename of the Enhanced network TTS engine extension.
inline constexpr char kEnhancedNetworkTtsGuestManifestV3Filename[] =
    "enhanced_network_tts_manifest_guest_v3.json";
// The extension id of the Select-to-speak extension.
inline constexpr char kSelectToSpeakExtensionId[] =
    "klbcgckkldhdhonijdbnhhaiedfkllef";
// Path to preinstalled Select-to-speak extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kSelectToSpeakExtensionPath[] = "chromeos/accessibility";
// The manifest filename of the Select to Speak extension.
inline constexpr char kSelectToSpeakManifestFilename[] =
    "select_to_speak_manifest.json";
// The manifest v3 filename of the Select to Speak extension.
inline constexpr char kSelectToSpeakManifestV3Filename[] =
    "select_to_speak_manifest_v3.json";
// The guest manifest filename of the Select to Speak extension.
inline constexpr char kSelectToSpeakGuestManifestFilename[] =
    "select_to_speak_manifest_guest.json";
// The guest manifest v3 filename of the Select to Speak extension.
inline constexpr char kSelectToSpeakGuestManifestV3Filename[] =
    "select_to_speak_manifest_v3_guest.json";
// The extension id of the Switch Access extension.
inline constexpr char kSwitchAccessExtensionId[] =
    "pmehocpgjmkenlokgjfkaichfjdhpeol";
// Path to preinstalled Switch Access extension (relative to
// |chrome::DIR_RESOURCES|).
inline constexpr char kSwitchAccessExtensionPath[] = "chromeos/accessibility";
// The manifest filename of the Switch Access extension.
inline constexpr char kSwitchAccessManifestFilename[] =
    "switch_access_manifest.json";
// The manifest v3 filename of the Switch Access extension.
inline constexpr char kSwitchAccessManifestV3Filename[] =
    "switch_access_manifest_v3.json";
// The guest manifest filename of the Switch Access extension.
inline constexpr char kSwitchAccessGuestManifestFilename[] =
    "switch_access_manifest_guest.json";
// The guest manifest v3 filename of the Switch Access extension.
inline constexpr char kSwitchAccessGuestManifestV3Filename[] =
    "switch_access_manifest_guest_v3.json";
// Name of the manifest file in an extension when a special manifest is used
// for guest mode.
inline constexpr char kGuestManifestFilename[] = "manifest_guest.json";
// The extension id of the first run dialog application.
inline constexpr char kFirstRunDialogId[] = "jdgcneonijmofocbhmijhacgchbihela";
// Path to preinstalled Google speech synthesis extension.
inline constexpr char kGoogleSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/patts";
// The extension id of the Google speech synthesis extension.
inline constexpr char kGoogleSpeechSynthesisExtensionId[] =
    "gjjabgpgjpampikjhjpfhneeoapjbjaf";
// The path to the Google speech synthesis extension's options page.
inline constexpr char kGoogleSpeechSynthesisOptionsPath[] = "/options.html";
// Path to preinstalled eSpeak-NG speech synthesis extension.
inline constexpr char kEspeakSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/espeak-ng";
// The extension id of the eSpeak-NG speech synthesis extension.
inline constexpr char kEspeakSpeechSynthesisExtensionId[] =
    "dakbfdmgjiabojdgbiljlhgjbokobjpg";
// The path to the eSpeak-NG speech synthesis extension's options page.
inline constexpr char kEspeakSpeechSynthesisOptionsPath[] = "/options.html";
// The extension id of official HelpApp extension.
inline constexpr char kHelpAppExtensionId[] =
    "honijodknafkokifofgiaalefdiedpko";
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
// The extension id of the Lacros accessibility helper extension.
inline constexpr char kEmbeddedA11yHelperExtensionId[] =
    "kgonammgkackdilhodbgbmodpepjocdp";
// The path to the Lacros accessibility helper extension.
inline constexpr char kEmbeddedA11yHelperExtensionPath[] = "accessibility";
// The name of the manifest file for the Lacros accessibility helper extension.
inline constexpr char kEmbeddedA11yHelperManifestFilename[] =
    "embedded_a11y_helper_manifest.json";
// The extension id of the Lacros ChromeVox helper extension.
inline constexpr char kChromeVoxHelperExtensionId[] =
    "mlkejohendkgipaomdopolhpbihbhfnf";
// The path to the Lacros ChromeVox helper extension.
inline constexpr char kChromeVoxHelperExtensionPath[] = "accessibility";
// The name of the manifest file for the Lacros ChromeVox helper extension.
inline constexpr char kChromeVoxHelperManifestFilename[] =
    "chromevox_helper_manifest.json";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
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
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

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
inline constexpr char kAppStateNotInstalled[] = "not_installed";
inline constexpr char kAppStateInstalled[] = "installed";
inline constexpr char kAppStateDisabled[] = "disabled";
inline constexpr char kAppStateRunning[] = "running";
inline constexpr char kAppStateCannotRun[] = "cannot_run";
inline constexpr char kAppStateReadyToRun[] = "ready_to_run";

// The path part of the file system url used for media file systems.
inline constexpr char kMediaFileSystemPathPart[] = "_";

// The key name of extension request timestamp used by the
// prefs::kCloudExtensionRequestIds preference.
inline constexpr char kExtensionRequestTimestamp[] = "timestamp";

// The key name of the extension workflow request justification used by the
// prefs::kCloudExtensionRequestIds preference.
inline constexpr char kExtensionWorkflowJustification[] = "justification";
}  // namespace extension_misc

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
