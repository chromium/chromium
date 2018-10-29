// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"

#include "base/macros.h"
#include "components/nacl/common/buildflags.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "extensions/buildflags/buildflags.h"

namespace chrome {

// Please keep this file in the same order as the header.

// Note: Add hosts to |kChromePaths| in browser_about_handler.cc to be listed by
// chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.

const char kChromeUIAboutHost[] = "about";
const char kChromeUIAboutURL[] = "chrome://about/";
const char kChromeUIAccessibilityHost[] = "accessibility";
const char kChromeUIAppLauncherPageHost[] = "apps";
const char kChromeUIAppListStartPageURL[] = "chrome://app-list/";
const char kChromeUIAppsURL[] = "chrome://apps/";
const char kChromeUIBluetoothInternalsHost[] = "bluetooth-internals";
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUICertificateViewerDialogHost[] = "view-cert-dialog";
const char kChromeUICertificateViewerDialogURL[] = "chrome://view-cert-dialog/";
const char kChromeUICertificateViewerHost[] = "view-cert";
const char kChromeUICertificateViewerURL[] = "chrome://view-cert/";
const char kChromeUIChromeSigninHost[] = "chrome-signin";
const char kChromeUIChromeSigninURL[] = "chrome://chrome-signin/";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUIComponentsHost[] = "components";
const char kChromeUIConflictsHost[] = "conflicts";
const char kChromeUIConflictsURL[] = "chrome://conflicts/";
const char kChromeUIConstrainedHTMLTestURL[] = "chrome://constrained-test/";
const char kChromeUIContentSettingsURL[] = "chrome://settings/content";
const char kChromeUICrashHost[] = "crash";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIDefaultHost[] = "version";
const char kChromeUIDelayedHangUIHost[] = "delayeduithreadhang";
const char kChromeUIDevToolsBlankPath[] = "blank";
const char kChromeUIDevToolsBundledPath[] = "bundled";
const char kChromeUIDevToolsCustomPath[] = "custom";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDevToolsRemotePath[] = "remote";
const char kChromeUIDevToolsURL[] =
    "chrome-devtools://devtools/bundled/inspector.html";
const char kChromeUIDeviceLogHost[] = "device-log";
const char kChromeUIDevicesHost[] = "devices";
const char kChromeUIDevicesURL[] = "chrome://devices/";
const char kChromeUIDomainReliabilityInternalsHost[] =
    "domain-reliability-internals";
const char kChromeUIDownloadInternalsHost[] = "download-internals";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIDriveInternalsHost[] = "drive-internals";
const char kChromeUIExtensionIconHost[] = "extension-icon";
const char kChromeUIExtensionIconURL[] = "chrome://extension-icon/";
const char kChromeUIExtensionsFrameHost[] = "extensions-frame";
const char kChromeUIExtensionsFrameURL[] = "chrome://extensions-frame/";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIExtensionsInternalsHost[] = "extensions-internals";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIFaviconHost[] = "favicon";
const char kChromeUIFaviconURL[] = "chrome://favicon/";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIFlashHost[] = "flash";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHangUIHost[] = "uithreadhang";
const char kChromeUIHelpHost[] = "help";
const char kChromeUIHelpURL[] = "chrome://help/";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIIdentityInternalsHost[] = "identity-internals";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUIInterstitialHost[] = "interstitials";
const char kChromeUIInterstitialURL[] = "chrome://interstitials/";
const char kChromeUIInterventionsInternalsHost[] = "interventions-internals";
const char kChromeUIInvalidationsHost[] = "invalidations";
const char kChromeUIKillHost[] = "kill";
const char kChromeUILocalStateHost[] = "local-state";
const char kChromeUIManagementHost[] = "management";
const char kChromeUIManagementURL[] = "chrome://management";
const char kChromeUIMdUserManagerHost[] = "md-user-manager";
const char kChromeUIMdUserManagerUrl[] = "chrome://md-user-manager/";
const char kChromeUIMediaEngagementHost[] = "media-engagement";
const char kChromeUIMediaRouterHost[] = "media-router";
const char kChromeUIMediaRouterURL[] = "chrome://media-router/";
const char kChromeUIMediaRouterInternalsHost[] = "media-router-internals";
const char kChromeUIMediaRouterInternalsURL[] =
    "chrome://media-router-internals/";
const char kChromeUIMemoryInternalsHost[] = "memory-internals";
const char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
const char kChromeUINaClHost[] = "nacl";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINetInternalsURL[] = "chrome://net-internals/";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUINewTabIconHost[] = "ntpicon";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUIOmniboxHost[] = "omnibox";
const char kChromeUIOmniboxURL[] = "chrome://omnibox/";
const char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
const char kChromeUIPhysicalWebHost[] = "physical-web";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIPolicyToolHost[] = "policy-tool";
const char kChromeUIPolicyURL[] = "chrome://policy/";
const char kChromeUIPredictorsHost[] = "predictors";
const char kChromeUIPrefsInternalsHost[] = "prefs-internals";
const char kChromeUIPrintURL[] = "chrome://print/";
const char kChromeUIQuitHost[] = "quit";
const char kChromeUIQuitURL[] = "chrome://quit/";
const char kChromeUIQuotaInternalsHost[] = "quota-internals";
const char kChromeUIResetPasswordHost[] = "reset-password";
const char kChromeUIResetPasswordURL[] = "chrome://reset-password/";
const char kChromeUIRestartHost[] = "restart";
const char kChromeUIRestartURL[] = "chrome://restart/";
const char kChromeUISafetyURL[] = "https://g.co/PixelSlate/safety";
const char kChromeUISettingsHost[] = "settings";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUISigninEmailConfirmationHost[] = "signin-email-confirmation";
const char kChromeUISigninEmailConfirmationURL[] =
    "chrome://signin-email-confirmation";
const char kChromeUISigninErrorHost[] = "signin-error";
const char kChromeUISigninErrorURL[] = "chrome://signin-error/";
const char kChromeUISiteDetailsPrefixURL[] =
    "chrome://settings/content/siteDetails?site=";
const char kChromeUISiteEngagementHost[] = "site-engagement";
const char kChromeUISuggestionsHost[] = "suggestions";
const char kChromeUISuggestionsURL[] = "chrome://suggestions/";
const char kChromeUISupervisedUserInternalsHost[] = "supervised-user-internals";
const char kChromeUISupervisedUserPassphrasePageHost[] =
    "managed-user-passphrase";
const char kChromeUISyncConfirmationHost[] = "sync-confirmation";
const char kChromeUISyncConfirmationURL[] = "chrome://sync-confirmation/";
const char kChromeUISyncConsentBumpURL[] =
    "chrome://sync-confirmation/?consent-bump";
const char kChromeUISyncFileSystemInternalsHost[] = "syncfs-internals";
const char kChromeUISyncHost[] = "sync";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUISyncResourcesHost[] = "syncresources";
const char kChromeUISystemInfoHost[] = "system";
const char kChromeUITaskSchedulerInternalsHost[] = "taskscheduler-internals";
const char kChromeUITermsHost[] = "terms";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIThemeHost[] = "theme";
const char kChromeUIThemeURL[] = "chrome://theme/";
const char kChromeUIThumbnailHost2[] = "thumb2";
const char kChromeUIThumbnailHost[] = "thumb";
const char kChromeUIThumbnailListHost[] = "thumbnails";
const char kChromeUIThumbnailURL[] = "chrome://thumb/";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIUkmHost[] = "ukm";
const char kChromeUIUberHost[] = "chrome";
const char kChromeUIUsbInternalsHost[] = "usb-internals";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIVersionHost[] = "version";
const char kChromeUIVersionURL[] = "chrome://version/";
const char kChromeUIWelcomeHost[] = "welcome";
const char kChromeUIWelcomeURL[] = "chrome://welcome/";
const char kChromeUIWelcomeWin10Host[] = "welcome-win10";
const char kChromeUIWelcomeWin10URL[] = "chrome://welcome-win10/";
const char kDeprecatedChromeUIHistoryFrameHost[] = "history-frame";
const char kDeprecatedChromeUIHistoryFrameURL[] = "chrome://history-frame/";

#if defined(OS_ANDROID)
const char kChromeUIEocInternalsHost[] = "eoc-internals";
const char kChromeUIJavaCrashURL[] = "chrome://java-crash/";
const char kChromeUINativeBookmarksURL[] = "chrome-native://bookmarks/";
const char kChromeUINativeExploreURL[] = "chrome-native://explore";
const char kChromeUINativeHistoryURL[] = "chrome-native://history/";
const char kChromeUINativeNewTabURL[] = "chrome-native://newtab/";
const char kChromeUINativePhysicalWebDiagnosticsURL[] =
    "chrome-native://physical-web-diagnostics/";
const char kChromeUINativeScheme[] = "chrome-native";
const char kChromeUIOfflineInternalsHost[] = "offline-internals";
const char kChromeUIPhysicalWebDiagnosticsHost[] = "physical-web-diagnostics";
const char kChromeUISnippetsInternalsHost[] = "snippets-internals";
const char kChromeUIWebApksHost[] = "webapks";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessageHost[] = "activationmessage";
const char kChromeUIBluetoothPairingHost[] = "bluetooth-pairing";
const char kChromeUIBluetoothPairingURL[] = "chrome://bluetooth-pairing/";
const char kChromeUICertificateManagerDialogURL[] =
    "chrome://certificate-manager/";
const char kChromeUICertificateManagerHost[] = "certificate-manager";
const char kChromeUICryptohomeHost[] = "cryptohome";
const char kChromeUIDeviceEmulatorHost[] = "device-emulator";
const char kChromeUIDiscoverURL[] = "chrome://oobe/discover";
const char kChromeUIFirstRunHost[] = "first-run";
const char kChromeUIFirstRunURL[] = "chrome://first-run/";
const char kChromeUIIntenetConfigDialogURL[] =
    "chrome://internet-config-dialog/";
const char kChromeUIIntenetDetailDialogURL[] =
    "chrome://internet-detail-dialog/";
const char kChromeUIInternetConfigDialogHost[] = "internet-config-dialog";
const char kChromeUIInternetDetailDialogHost[] = "internet-detail-dialog";
const char kChromeUILinuxCreditsHost[] = "linux-credits";
const char kChromeUILinuxCreditsURL[] = "chrome://linux-credits/";
const char kChromeUIMobileSetupHost[] = "mobilesetup";
const char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
const char kChromeUIMultiDeviceSetupHost[] = "multidevice-setup";
const char kChromeUIMultiDeviceSetupUrl[] = "chrome://multidevice-setup";
const char kChromeUINetworkHost[] = "network";
const char kChromeUIOSCreditsHost[] = "os-credits";
const char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
const char kChromeUIOobeHost[] = "oobe";
const char kChromeUIOobeURL[] = "chrome://oobe/";
const char kChromeUIPowerHost[] = "power";
const char kChromeUIScreenlockIconHost[] = "screenlock-icon";
const char kChromeUIScreenlockIconURL[] = "chrome://screenlock-icon/";
const char kChromeUISetTimeHost[] = "set-time";
const char kChromeUISetTimeURL[] = "chrome://set-time/";
const char kChromeUISlowHost[] = "slow";
const char kChromeUISlowTraceHost[] = "slow_trace";
const char kChromeUISlowURL[] = "chrome://slow/";
const char kChromeUISysInternalsHost[] = "sys-internals";
const char kChromeUIUserImageHost[] = "userimage";
const char kChromeUIUserImageURL[] = "chrome://userimage/";
const char kChromeUIAssistantOptInHost[] = "assistant-optin";
const char kChromeUIAssistantOptInURL[] = "chrome://assistant-optin/";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
const char kChromeUIMetroFlowHost[] = "make-metro";
const char kChromeUIMetroFlowURL[] = "chrome://make-metro/";
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
const char kChromeUICastHost[] = "cast";
const char kChromeUICastURL[] = "chrome://cast/";
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
const char kChromeUIDiscardsHost[] = "discards";
const char kChromeUIDiscardsURL[] = "chrome://discards/";
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
const char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
const char kChromeUISandboxHost[] = "sandbox";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUITabModalConfirmDialogHost[] = "tab-modal-confirm-dialog";
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
const char kChromeUIPrintHost[] = "print";
#endif

const char kChromeUIWebRtcLogsHost[] = "webrtc-logs";

// Settings sub pages.

// NOTE: Add sub page paths to |kChromeSettingsSubPages| in
// chrome_autocomplete_provider_client.cc to be listed by the built-in
// AutocompleteProvider.

const char kAutofillSubPage[] = "autofill";
const char kClearBrowserDataSubPage[] = "clearBrowserData";
const char kContentSettingsSubPage[] = "content";
const char kDeprecatedExtensionsSubPage[] = "extensions";
const char kHandlerSettingsSubPage[] = "handlers";
const char kImportDataSubPage[] = "importData";
const char kLanguageOptionsSubPage[] = "languages";
const char kPasswordManagerSubPage[] = "passwords";
const char kPaymentsSubPage[] = "payments";
const char kPrintingSettingsSubPage[] = "printing";
const char kResetProfileSettingsSubPage[] = "resetProfileSettings";
const char kSearchEnginesSubPage[] = "searchEngines";
const char kSignOutSubPage[] = "signOut";
const char kSyncSetupSubPage[] = "syncSetup";
const char kTriggeredResetProfileSettingsSubPage[] =
    "triggeredResetProfileSettings";
#if defined(OS_CHROMEOS)
const char kAccessibilitySubPage[] = "accessibility";
const char kBluetoothSubPage[] = "bluetoothDevices";
const char kDateTimeSubPage[] = "dateTime";
const char kDisplaySubPage[] = "display";
const char kHelpSubPage[] = "help";
const char kInternetSubPage[] = "internet";
// 'multidevice/features' is a child of the 'multidevice' route
const char kConnectedDevicesSubPage[] = "multidevice/features";
const char kLockScreenSubPage[] = "lockScreen";
const char kNetworkDetailSubPage[] = "networkDetail";
const char kPowerSubPage[] = "power";
const char kSmbSharesPageAddDialog[] = "smbShares?showAddShare=true";
const char kStylusSubPage[] = "stylus";
#else
const char kCreateProfileSubPage[] = "createProfile";
const char kManageProfileSubPage[] = "manageProfile";
const char kPeopleSubPage[] = "people";
#endif  // defined(OS_CHROMEOS)
#if defined(OS_WIN)
const char kCleanupSubPage[] = "cleanup";
#endif  // defined(OS_WIN)

// Extension sub pages.
const char kExtensionConfigureCommandsSubPage[] = "configureCommands";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
const char* const kChromeHostURLs[] = {
    kChromeUIAboutHost,
    kChromeUIAccessibilityHost,
    kChromeUIBluetoothInternalsHost,
    kChromeUIChromeURLsHost,
    kChromeUIComponentsHost,
    kChromeUICrashesHost,
    kChromeUICreditsHost,
#if defined(OS_CHROMEOS) && !defined(OFFICIAL_BUILD)
    kChromeUIDeviceEmulatorHost,
#endif
    kChromeUIDeviceLogHost,
    kChromeUIDownloadInternalsHost,
    kChromeUIFlagsHost,
    kChromeUIGCMInternalsHost,
    kChromeUIHistoryHost,
    kChromeUIInterstitialHost,
    kChromeUIInterventionsInternalsHost,
    kChromeUIInvalidationsHost,
    kChromeUILocalStateHost,
    kChromeUIMediaEngagementHost,
    kChromeUINetExportHost,
    kChromeUINetInternalsHost,
    kChromeUINewTabHost,
    kChromeUIOmniboxHost,
    kChromeUIPasswordManagerInternalsHost,
    kChromeUIPolicyHost,
    kChromeUIPredictorsHost,
    kChromeUIQuotaInternalsHost,
    kChromeUISignInInternalsHost,
    kChromeUISiteEngagementHost,
    kChromeUINTPTilesInternalsHost,
    safe_browsing::kChromeUISafeBrowsingHost,
    kChromeUISuggestionsHost,
    kChromeUISupervisedUserInternalsHost,
    kChromeUISyncInternalsHost,
    kChromeUITaskSchedulerInternalsHost,
#if !defined(OS_ANDROID)
    kChromeUITermsHost,
    kChromeUIThumbnailListHost,
#endif
    kChromeUITranslateInternalsHost,
    kChromeUIUsbInternalsHost,
    kChromeUIUserActionsHost,
    kChromeUIVersionHost,
    content::kChromeUIAppCacheInternalsHost,
    content::kChromeUIBlobInternalsHost,
    content::kChromeUIDinoHost,
    content::kChromeUIGpuHost,
    content::kChromeUIHistogramHost,
    content::kChromeUIIndexedDBInternalsHost,
    content::kChromeUIMediaInternalsHost,
    content::kChromeUINetworkErrorHost,
    content::kChromeUINetworkErrorsListingHost,
    content::kChromeUIServiceWorkerInternalsHost,
#if !defined(OS_ANDROID)
    content::kChromeUITracingHost,
#endif
    content::kChromeUIWebRTCInternalsHost,
#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
    kChromeUIAppLauncherPageHost,
#endif
    kChromeUIBookmarksHost,
    kChromeUIDownloadsHost,
    kChromeUIFlashHost,
    kChromeUIHelpHost,
    kChromeUIInspectHost,
    kChromeUISettingsHost,
    kChromeUISystemInfoHost,
    kChromeUIUberHost,
#endif
#if defined(OS_ANDROID)
    kChromeUIEocInternalsHost,
    kChromeUIOfflineInternalsHost,
    kChromeUISnippetsInternalsHost,
    kChromeUIWebApksHost,
#endif
#if defined(OS_CHROMEOS)
    kChromeUICertificateManagerHost,
    kChromeUICryptohomeHost,
    kChromeUIDriveInternalsHost,
    kChromeUIFirstRunHost,
    kChromeUILinuxCreditsHost,
    kChromeUINetworkHost,
    kChromeUIOobeHost,
    kChromeUIOSCreditsHost,
    kChromeUIPowerHost,
    kChromeUIInternetConfigDialogHost,
    kChromeUIInternetDetailDialogHost,
    kChromeUIAssistantOptInHost,
#endif
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    kChromeUIDiscardsHost,
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
    kChromeUILinuxProxyConfigHost,
#endif
#if defined(OS_LINUX) || defined(OS_ANDROID)
    kChromeUISandboxHost,
#endif
#if defined(OS_WIN)
    kChromeUIConflictsHost,
#endif
#if BUILDFLAG(ENABLE_NACL)
    kChromeUINaClHost,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
    kChromeUIExtensionsHost,
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    kChromeUIPrintHost,
#endif
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    kChromeUIDevicesHost,
#endif
    kChromeUIWebRtcLogsHost,
};
const size_t kNumberOfChromeHostURLs = arraysize(kChromeHostURLs);

const char* const kChromeDebugURLs[] = {
    content::kChromeUIBadCastCrashURL,
    content::kChromeUIBrowserCrashURL,
    content::kChromeUICrashURL,
    content::kChromeUIDumpURL,
    content::kChromeUIKillURL,
    content::kChromeUIHangURL,
    content::kChromeUIShorthangURL,
    content::kChromeUIGpuCleanURL,
    content::kChromeUIGpuCrashURL,
    content::kChromeUIGpuHangURL,
    content::kChromeUIMemoryExhaustURL,
    content::kChromeUIPpapiFlashCrashURL,
    content::kChromeUIPpapiFlashHangURL,
#if defined(OS_WIN)
    content::kChromeUIBrowserHeapCorruptionURL,
    content::kChromeUIHeapCorruptionCrashURL,
#endif
#if defined(OS_ANDROID)
    content::kChromeUIGpuJavaCrashURL,
    kChromeUIJavaCrashURL,
#endif
    kChromeUIQuitURL,
    kChromeUIRestartURL};
const size_t kNumberOfChromeDebugURLs = arraysize(kChromeDebugURLs);

}  // namespace chrome
