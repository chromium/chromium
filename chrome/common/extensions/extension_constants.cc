// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/constants.h"

namespace extension_urls {

const char kWebstoreSourceField[] = "utm_source";

const char kLaunchSourceAppList[] = "chrome-app-launcher";
const char kLaunchSourceAppListSearch[] = "chrome-app-launcher-search";
const char kLaunchSourceAppListInfoDialog[] = "chrome-app-launcher-info-dialog";

}  // namespace extension_urls

namespace extension_misc {

const char kCalendarAppId[] = "ejjicmeblgpmajnghnpcppodonldlgfn";
const char kDataSaverExtensionId[] = "pfmgfdlgomnbgkofeojodiodmgpgmkac";
const char kDocsOfflineExtensionId[] = "ghbmnnjooekpmoecnnnilnnbdlolhkhi";
const char kGoogleMapsAppId[] = "lneaknkopdijkpnocmklfnjbeapigfbh";
const char kGooglePhotosAppId[] = "hcglmfcclpfgljeaiahehebeoaiicbko";
const char kGooglePlayBooksAppId[] = "mmimngoggfoobjdlefbcabngfnmieonb";
const char kGooglePlayMoviesAppId[] = "gdijeikdkaembjbdobgfkoidjkpbmlkd";
const char kGooglePlayMusicAppId[] = "icppfcnhkcmnfdhfhphakoifcfokfdhg";
const char kGooglePlusAppId[] = "dlppkpafhbajpcmmoheippocdidnckmm";
const char kTextEditorAppId[] = "mmfbcljfglbokpmkimbfghdkjmjhdgbg";
const char kInAppPaymentsSupportAppId[] = "nmmhkkegccagdldgiimedpiccmgmieda";
const char kKeyboardExtensionId[] = "mppnpdlheglhdfmldimlhpnegondlapf";

const char* const kBuiltInFirstPartyExtensionIds[] = {
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
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    kAccessibilityCommonExtensionId,
    kSelectToSpeakExtensionId,
    kSwitchAccessExtensionId,
    kFilesManagerAppId,
    kFirstRunDialogId,
    kEspeakSpeechSynthesisExtensionId,
    kGoogleSpeechSynthesisExtensionId,
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    kEmbeddedA11yHelperExtensionId,
    kChromeVoxHelperExtensionId,
#endif        // BUILDFLAG(IS_CHROMEOS_LACROS)
    nullptr,  // Null-terminated array.
};

#if BUILDFLAG(IS_CHROMEOS)
const char kAssessmentAssistantExtensionId[] =
    "gndmhdcefbhlchkhipcnnbkcmicncehk";
const char kEchoExtensionId[] = "kddnkjkcjddckihglkfcickdhbmaodcn";
const char kGnubbyAppId[] = "beknehfpfkghjoafdifaflglpjkojoco";
const char kGnubbyV3ExtensionId[] = "lfboplenmmjcmpbkeemecobbadnmpfhi";
const char kGCSEExtensionId[] = "cfmgaohenjcikllcgjpepfadgbflcjof";
const char kContactCenterInsightsExtensionId[] =
    "oebfonohdfogiaaaelfmjlkjbgdbaahf";
const char kDeskApiExtensionId[] = "kflgdebkpepnpjobkdfeeipcjdahoomc";
const char kBruSecurityKeyForwarderExtensionId[] =
    "lcooaekmckohjjnpaaokodoepajbnill";
const char kODFSExtensionId[] = "gnnndjlaomemikopnjhhnoombakkkkdg";
const char kPerfettoUIExtensionId[] = "lfmkphfpdbjijhpomgecfikhfohaoine";
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kAccessibilityCommonExtensionId[] =
    "egfdjlfmgnehecnclamagfafdccgfndp";
const char kAccessibilityCommonExtensionPath[] = "chromeos/accessibility";
const char kAccessibilityCommonManifestFilename[] =
    "accessibility_common_manifest.json";
const char kAccessibilityCommonGuestManifestFilename[] =
    "accessibility_common_manifest_guest.json";
const char kChromeVoxExtensionPath[] = "chromeos/accessibility";
const char kChromeVoxManifestFilename[] = "chromevox_manifest.json";
const char kChromeVoxGuestManifestFilename[] = "chromevox_manifest_guest.json";
const char kChromeVoxOptionsPath[] = "/chromevox/options/options.html";
const char kEnhancedNetworkTtsExtensionId[] =
    "jacnkoglebceckolkoapelihnglgaicd";
const char kEnhancedNetworkTtsExtensionPath[] = "chromeos/accessibility";
const char kEnhancedNetworkTtsManifestFilename[] =
    "enhanced_network_tts_manifest.json";
const char kEnhancedNetworkTtsGuestManifestFilename[] =
    "enhanced_network_tts_manifest_guest.json";
const char kSelectToSpeakExtensionId[] = "klbcgckkldhdhonijdbnhhaiedfkllef";
const char kSelectToSpeakExtensionPath[] = "chromeos/accessibility";
const char kSelectToSpeakManifestFilename[] = "select_to_speak_manifest.json";
const char kSelectToSpeakGuestManifestFilename[] =
    "select_to_speak_manifest_guest.json";
const char kSwitchAccessExtensionId[] = "pmehocpgjmkenlokgjfkaichfjdhpeol";
const char kSwitchAccessExtensionPath[] = "chromeos/accessibility";
const char kSwitchAccessManifestFilename[] = "switch_access_manifest.json";
const char kSwitchAccessGuestManifestFilename[] =
    "switch_access_manifest_guest.json";
const char kGuestManifestFilename[] = "manifest_guest.json";
const char kFirstRunDialogId[] = "jdgcneonijmofocbhmijhacgchbihela";
const char kEspeakSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/espeak-ng";
const char kEspeakSpeechSynthesisExtensionId[] =
    "dakbfdmgjiabojdgbiljlhgjbokobjpg";
const char kEspeakSpeechSynthesisOptionsPath[] = "/options.html";
const char kGoogleSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/patts";
const char kGoogleSpeechSynthesisExtensionId[] =
    "gjjabgpgjpampikjhjpfhneeoapjbjaf";
const char kGoogleSpeechSynthesisOptionsPath[] = "/options.html";
const char kHelpAppExtensionId[] = "honijodknafkokifofgiaalefdiedpko";
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
const char kEmbeddedA11yHelperExtensionId[] =
    "kgonammgkackdilhodbgbmodpepjocdp";
const char kEmbeddedA11yHelperExtensionPath[] = "accessibility";
const char kEmbeddedA11yHelperManifestFilename[] =
    "embedded_a11y_helper_manifest.json";
const char kChromeVoxHelperExtensionId[] = "mlkejohendkgipaomdopolhpbihbhfnf";
const char kChromeVoxHelperExtensionPath[] = "accessibility";
const char kChromeVoxHelperManifestFilename[] =
    "chromevox_helper_manifest.json";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

const char kAppStateNotInstalled[] = "not_installed";
const char kAppStateInstalled[] = "installed";
const char kAppStateDisabled[] = "disabled";
const char kAppStateRunning[] = "running";
const char kAppStateCannotRun[] = "cannot_run";
const char kAppStateReadyToRun[] = "ready_to_run";

const char kMediaFileSystemPathPart[] = "_";
const char kExtensionRequestTimestamp[] = "timestamp";
const char kExtensionWorkflowJustification[] = "justification";

}  // namespace extension_misc
