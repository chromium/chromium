// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"

#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "components/nacl/common/buildflags.h"
#include "components/safe_browsing/core/web_ui/constants.h"
#include "extensions/buildflags/buildflags.h"

namespace chrome {

// Please keep this file in the same order as the header.

// Note: Add hosts to |kChromeHostURLs| at the bottom of this file to be listed
// by chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.

const char kChromeUIAboutHost[] = "about";
const char kChromeUIAboutURL[] = "chrome://about/";
const char kChromeUIAccessibilityHost[] = "accessibility";
const char kChromeUIAppIconHost[] = "app-icon";
const char kChromeUIAppIconURL[] = "chrome://app-icon/";
const char kChromeUIAppLauncherPageHost[] = "apps";
const char kChromeUIAppsURL[] = "chrome://apps/";
const char kChromeUIAutofillInternalsHost[] = "autofill-internals";
const char kChromeUIBluetoothInternalsHost[] = "bluetooth-internals";
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUICertificateViewerHost[] = "view-cert";
const char kChromeUICertificateViewerURL[] = "chrome://view-cert/";
const char kChromeUIChromeSigninHost[] = "chrome-signin";
const char kChromeUIChromeSigninURL[] = "chrome://chrome-signin/";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUIComponentsHost[] = "components";
const char kChromeUIConflictsHost[] = "conflicts";
const char kChromeUIConstrainedHTMLTestURL[] = "chrome://constrained-test/";
const char kChromeUIContentSettingsURL[] = "chrome://settings/content";
// TODO (crbug.com/1107816): Remove deprecated cookie URL redirection.
const char kChromeUICookieSettingsDeprecatedURL[] =
    "chrome://settings/content/cookies";
const char kChromeUICookieSettingsURL[] = "chrome://settings/cookies";
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
    "devtools://devtools/bundled/inspector.html";
const char kChromeUIDeviceLogHost[] = "device-log";
const char kChromeUIDevicesHost[] = "devices";
const char kChromeUIDevicesURL[] = "chrome://devices/";
const char kChromeUIDevUiLoaderURL[] = "chrome://dev-ui-loader/";
const char kChromeUIDiceWebSigninInterceptHost[] = "signin-dice-web-intercept";
const char kChromeUIDiceWebSigninInterceptURL[] =
    "chrome://signin-dice-web-intercept/";
const char kChromeUIDomainReliabilityInternalsHost[] =
    "domain-reliability-internals";
const char kChromeUIDownloadInternalsHost[] = "download-internals";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIDriveInternalsHost[] = "drive-internals";
const char kChromeUIEDUCoexistenceLoginURLV1[] = "chrome://chrome-signin/edu";
const char kChromeUIEDUCoexistenceLoginURLV2[] =
    "chrome://chrome-signin/edu-coexistence";
const char kChromeUIExtensionIconHost[] = "extension-icon";
const char kChromeUIExtensionIconURL[] = "chrome://extension-icon/";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIExtensionsInternalsHost[] = "extensions-internals";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIFaviconHost[] = "favicon";
const char kChromeUIFaviconURL[] = "chrome://favicon/";
const char kChromeUIFavicon2Host[] = "favicon2";
const char kChromeUIFileiconURL[] = "chrome://fileicon/";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHangUIHost[] = "uithreadhang";
const char kChromeUIHelpHost[] = "help";
const char kChromeUIHelpURL[] = "chrome://help/";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIHistorySyncedTabs[] = "/syncedTabs";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIIdentityInternalsHost[] = "identity-internals";
const char kChromeUIImageHost[] = "image";
const char kChromeUIImageURL[] = "chrome://image/";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUIInternalsHost[] = "internals";
const char kChromeUIInternalsQueryTilesPath[] = "query-tiles";
const char kChromeUIInternalsWebAppPath[] = "web-app";
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
const char kChromeUIMediaFeedsHost[] = "media-feeds";
const char kChromeUIMediaHistoryHost[] = "media-history";
const char kChromeUIMediaRouterInternalsHost[] = "media-router-internals";
const char kChromeUIMemoryInternalsHost[] = "memory-internals";
const char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
const char kChromeUINaClHost[] = "nacl";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINetInternalsURL[] = "chrome://net-internals/";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUINewTabIconHost[] = "ntpicon";
const char kChromeUINewTabPageHost[] = "new-tab-page";
const char kChromeUINewTabPageURL[] = "chrome://new-tab-page";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUIOmniboxHost[] = "omnibox";
const char kChromeUIOmniboxURL[] = "chrome://omnibox/";
const char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
const char kChromeUIPolicyHost[] = "policy";
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
const char kChromeUISafetyPixelbookURL[] = "https://g.co/Pixelbook/legal";
const char kChromeUISafetyPixelSlateURL[] = "https://g.co/PixelSlate/legal";
const char kChromeUISettingsHost[] = "settings";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUISigninEmailConfirmationHost[] = "signin-email-confirmation";
const char kChromeUISigninEmailConfirmationURL[] =
    "chrome://signin-email-confirmation";
const char kChromeUISigninErrorHost[] = "signin-error";
const char kChromeUISigninErrorURL[] = "chrome://signin-error/";
const char kChromeUISigninReauthHost[] = "signin-reauth";
const char kChromeUISigninReauthURL[] = "chrome://signin-reauth/";
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
const char kChromeUISyncFileSystemInternalsHost[] = "syncfs-internals";
const char kChromeUISyncHost[] = "sync";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUISystemInfoHost[] = "system";
const char kChromeUITermsHost[] = "terms";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIThemeHost[] = "theme";
const char kChromeUIThemeURL[] = "chrome://theme/";
const char kChromeUIUntrustedThemeURL[] = "chrome-untrusted://theme/";
const char kChromeUIThumbnailHost2[] = "thumb2";
const char kChromeUIThumbnailHost[] = "thumb";
const char kChromeUIThumbnailListHost[] = "thumbnails";
const char kChromeUIThumbnailURL[] = "chrome://thumb/";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIUsbInternalsHost[] = "usb-internals";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIVersionHost[] = "version";
const char kChromeUIVersionURL[] = "chrome://version/";
const char kChromeUIWebFooterExperimentHost[] = "web-footer-experiment";
const char kChromeUIWebFooterExperimentURL[] =
    "chrome://web-footer-experiment/";
const char kChromeUIWelcomeHost[] = "welcome";
const char kChromeUIWelcomeURL[] = "chrome://welcome/";

#if defined(OS_WIN)
// TODO(crbug.com/1003960): Remove when issue is resolved.
const char kChromeUIWelcomeWin10Host[] = "welcome-win10";
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
const char kChromeUIExploreSitesInternalsHost[] = "explore-sites-internals";
const char kChromeUIJavaCrashURL[] = "chrome://java-crash/";
const char kChromeUINativeBookmarksURL[] = "chrome-native://bookmarks/";
const char kChromeUINativeExploreURL[] = "chrome-native://explore";
const char kChromeUINativeHistoryURL[] = "chrome-native://history/";
const char kChromeUINativeNewTabURL[] = "chrome-native://newtab/";
const char kChromeUIOfflineInternalsHost[] = "offline-internals";
const char kChromeUISnippetsInternalsHost[] = "snippets-internals";
const char kChromeUIUntrustedVideoPlayerUrl[] =
    "chrome-untrusted://video-tutorials/";
const char kChromeUIWebApksHost[] = "webapks";
#else
const char kChromeUINearbyInternalsHost[] = "nearby-internals";
const char kChromeUIReadLaterHost[] = "read-later";
const char kChromeUIReadLaterURL[] = "chrome://read-later/";
#endif

#if defined(OS_CHROMEOS)
// Keep alphabetized.
const char kChromeUIAccountManagerErrorHost[] = "account-manager-error";
const char kChromeUIAccountManagerErrorURL[] = "chrome://account-manager-error";
const char kChromeUIAccountManagerWelcomeHost[] = "account-manager-welcome";
const char kChromeUIAccountManagerWelcomeURL[] =
    "chrome://account-manager-welcome";
const char kChromeUIAccountMigrationWelcomeHost[] = "account-migration-welcome";
const char kChromeUIAccountMigrationWelcomeURL[] =
    "chrome://account-migration-welcome";
const char kChromeUIActivationMessageHost[] = "activationmessage";
const char kChromeUIAddSupervisionHost[] = "add-supervision";
const char kChromeUIAddSupervisionURL[] = "chrome://add-supervision/";
const char kChromeUIArcGraphicsTracingHost[] = "arc-graphics-tracing";
const char kChromeUIArcGraphicsTracingURL[] = "chrome://arc-graphics-tracing/";
const char kChromeUIArcOverviewTracingHost[] = "arc-overview-tracing";
const char kChromeUIArcOverviewTracingURL[] = "chrome://arc-overview-tracing/";
const char kChromeUIAssistantOptInHost[] = "assistant-optin";
const char kChromeUIAssistantOptInURL[] = "chrome://assistant-optin/";
const char kChromeUIAppDisabledHost[] = "app-disabled";
const char kChromeUIAppDisabledURL[] = "chrome://app-disabled";
const char kChromeUIBluetoothPairingHost[] = "bluetooth-pairing";
const char kChromeUIBluetoothPairingURL[] = "chrome://bluetooth-pairing/";
const char kChromeUICellularSetupHost[] = "cellular-setup";
const char kChromeUICellularSetupUrl[] = "chrome://cellular-setup";
const char kChromeUICertificateManagerDialogURL[] =
    "chrome://certificate-manager/";
const char kChromeUICertificateManagerHost[] = "certificate-manager";
const char kChromeUIConfirmPasswordChangeHost[] = "confirm-password-change";
const char kChromeUIConfirmPasswordChangeUrl[] =
    "chrome://confirm-password-change";
const char kChromeUICrostiniInstallerHost[] = "crostini-installer";
const char kChromeUICrostiniInstallerUrl[] = "chrome://crostini-installer";
const char kChromeUICrostiniUpgraderHost[] = "crostini-upgrader";
const char kChromeUICrostiniUpgraderUrl[] = "chrome://crostini-upgrader";
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
const char kChromeUICrostiniCreditsHost[] = "crostini-credits";
const char kChromeUICrostiniCreditsURL[] = "chrome://crostini-credits/";
const char kChromeUILockScreenStartReauthHost[] = "lock-reauth";
const char kChromeUILockScreenStartReauthURL[] = "chrome://lock-reauth";
const char kChromeUIMachineLearningInternalsHost[] =
    "machine-learning-internals";
const char kChromeUIMobileSetupHost[] = "mobilesetup";
const char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
const char kChromeUIMultiDeviceInternalsHost[] = "multidevice-internals";
const char kChromeUIMultiDeviceSetupHost[] = "multidevice-setup";
const char kChromeUIMultiDeviceSetupUrl[] = "chrome://multidevice-setup";
const char kChromeUINetworkHost[] = "network";
const char kChromeUIOSCreditsHost[] = "os-credits";
const char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
const char kChromeUIOSSettingsHost[] = "os-settings";
const char kChromeUIOSSettingsURL[] = "chrome://os-settings/";
const char kChromeUIOobeHost[] = "oobe";
const char kChromeUIOobeURL[] = "chrome://oobe/";
const char kChromeUIPasswordChangeHost[] = "password-change";
const char kChromeUIPasswordChangeUrl[] = "chrome://password-change";
const char kChromeUIPrintManagementUrl[] = "chrome://print-management";
const char kChromeUIPowerHost[] = "power";
const char kChromeUIScreenlockIconHost[] = "screenlock-icon";
const char kChromeUIScreenlockIconURL[] = "chrome://screenlock-icon/";
const char kChromeUISetTimeHost[] = "set-time";
const char kChromeUISetTimeURL[] = "chrome://set-time/";
const char kChromeUISlowHost[] = "slow";
const char kChromeUISlowTraceHost[] = "slow_trace";
const char kChromeUISlowURL[] = "chrome://slow/";
const char kChromeUISmbShareHost[] = "smb-share-dialog";
const char kChromeUISmbShareURL[] = "chrome://smb-share-dialog/";
const char kChromeUISmbCredentialsHost[] = "smb-credentials-dialog";
const char kChromeUISmbCredentialsURL[] = "chrome://smb-credentials-dialog/";
const char kChromeUISysInternalsHost[] = "sys-internals";
const char kChromeUIUntrustedCroshURL[] = "chrome-untrusted://crosh/";
const char kChromeUIUntrustedTerminalURL[] = "chrome-untrusted://terminal/";
const char kChromeUIUserImageHost[] = "userimage";
const char kChromeUIUserImageURL[] = "chrome://userimage/";
const char kChromeUIUrgentPasswordExpiryNotificationHost[] =
    "urgent-password-expiry-notification";
const char kChromeUIUrgentPasswordExpiryNotificationUrl[] =
    "chrome://urgent-password-expiry-notification/";
// Keep alphabetized.

bool IsSystemWebUIHost(base::StringPiece host) {
  // Compares host instead of full URL for performance (the strings are
  // shorter).
  static const char* const kHosts[] = {
      kChromeUIAccountManagerErrorHost,
      kChromeUIAccountManagerWelcomeHost,
      kChromeUIAccountMigrationWelcomeHost,
      kChromeUIActivationMessageHost,
      kChromeUIAddSupervisionHost,
      kChromeUIAssistantOptInHost,
      kChromeUIBluetoothPairingHost,
      kChromeUICellularSetupHost,
      kChromeUICertificateManagerHost,
      kChromeUICrostiniCreditsHost,
      kChromeUICrostiniInstallerHost,
      kChromeUICryptohomeHost,
      kChromeUIDeviceEmulatorHost,
      kChromeUIFirstRunHost,
      kChromeUIInternetConfigDialogHost,
      kChromeUIInternetDetailDialogHost,
      kChromeUILockScreenStartReauthHost,
      kChromeUIMobileSetupHost,
      kChromeUIMultiDeviceSetupHost,
      kChromeUINetworkHost,
      kChromeUIOobeHost,
      kChromeUIOSCreditsHost,
      kChromeUIOSSettingsHost,
      kChromeUIPasswordChangeHost,
      kChromeUIPowerHost,
      kChromeUISetTimeHost,
      kChromeUISmbCredentialsHost,
      kChromeUISmbShareHost,
  };
  for (const char* h : kHosts) {
    if (host == h)
      return true;
  }
  return false;
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
const char kChromeUIDiscardsHost[] = "discards";
const char kChromeUIDiscardsURL[] = "chrome://discards/";
const char kChromeUIHatsHost[] = "hats";
const char kChromeUIHatsURL[] = "chrome://hats/";
#if !defined(OS_CHROMEOS)
const char kChromeUIProfilePickerHost[] = "profile-picker";
const char kChromeUIProfilePickerUrl[] = "chrome://profile-picker/";
const char kChromeUIProfilePickerStartupQuery[] = "startup";
#endif
#endif

#if !defined(OS_ANDROID)
const char kChromeUINearbyShareHost[] = "nearby";
const char kChromeUINearbyShareURL[] = "chrome://nearby/";
#endif  // !defined(OS_ANDROID)

#if defined(OS_POSIX) && !defined(OS_MAC) && !defined(OS_ANDROID)
const char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
#endif

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_ANDROID)
const char kChromeUISandboxHost[] = "sandbox";
#endif

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
const char kChromeUIBrowserSwitchHost[] = "browser-switch";
const char kChromeUIBrowserSwitchURL[] = "chrome://browser-switch/";
#endif

#if ((defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(TOOLKIT_VIEWS)) || \
    defined(USE_AURA)
const char kChromeUITabModalConfirmDialogHost[] = "tab-modal-confirm-dialog";
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
const char kChromeUIPrintHost[] = "print";
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
const char kChromeUITabStripHost[] = "tab-strip";
const char kChromeUITabStripURL[] = "chrome://tab-strip";
#endif

#if !defined(OS_ANDROID)
const char kChromeUICommanderHost[] = "commander";
const char kChromeUICommanderURL[] = "chrome://commander";
const char kChromeUITabSearchHost[] = "tab-search";
const char kChromeUITabSearchURL[] = "chrome://tab-search/";
#endif

const char kChromeUIWebRtcLogsHost[] = "webrtc-logs";

// Settings sub pages.

// NOTE: Add sub page paths to |kChromeSettingsSubPages| in
// chrome_autocomplete_provider_client.cc to be listed by the built-in
// AutocompleteProvider.

const char kAccessibilitySubPage[] = "accessibility";
const char kAddressesSubPage[] = "addresses";
const char kAppearanceSubPage[] = "appearance";
const char kAutofillSubPage[] = "autofill";
const char kClearBrowserDataSubPage[] = "clearBrowserData";
const char kCloudPrintersSubPage[] = "cloudPrinters";
const char kContentSettingsSubPage[] = "content";
const char kCookieSettingsSubPage[] = "cookies";
const char kDownloadsSubPage[] = "downloads";
const char kHandlerSettingsSubPage[] = "handlers";
const char kImportDataSubPage[] = "importData";
const char kLanguagesSubPage[] = "languages/details";
const char kLanguageOptionsSubPage[] = "languages";
const char kOnStartupSubPage[] = "onStartup";
const char kPasswordCheckSubPage[] = "passwords/check?start=true";
const char kPasswordManagerSubPage[] = "passwords";
const char kPaymentsSubPage[] = "payments";
const char kPrintingSettingsSubPage[] = "printing";
const char kPrivacySubPage[] = "privacy";
const char kResetSubPage[] = "reset";
const char kResetProfileSettingsSubPage[] = "resetProfileSettings";
const char kSafeBrowsingEnhancedProtectionSubPage[] = "security?q=enhanced";
const char kSafetyCheckSubPage[] = "safetyCheck";
const char kSearchSubPage[] = "search";
const char kSearchEnginesSubPage[] = "searchEngines";
const char kSignOutSubPage[] = "signOut";
const char kSyncSetupSubPage[] = "syncSetup";
const char kTriggeredResetProfileSettingsSubPage[] =
    "triggeredResetProfileSettings";
const char kCreateProfileSubPage[] = "createProfile";
const char kManageProfileSubPage[] = "manageProfile";
const char kPeopleSubPage[] = "people";

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
    kChromeUIAutofillInternalsHost,
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
#if !defined(OS_ANDROID)
    kChromeUIManagementHost,
#endif
    kChromeUIMediaEngagementHost,
    kChromeUINetExportHost,
    kChromeUINetInternalsHost,
    kChromeUINewTabHost,
    kChromeUIOmniboxHost,
    kChromeUIPasswordManagerInternalsHost,
    kChromeUIPolicyHost,
    kChromeUIPredictorsHost,
    kChromeUIPrefsInternalsHost,
    kChromeUIQuotaInternalsHost,
    kChromeUISignInInternalsHost,
    kChromeUISiteEngagementHost,
    kChromeUINTPTilesInternalsHost,
    safe_browsing::kChromeUISafeBrowsingHost,
    kChromeUISuggestionsHost,
    kChromeUISupervisedUserInternalsHost,
    kChromeUISyncInternalsHost,
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
    content::kChromeUIConversionInternalsHost,
    content::kChromeUIDinoHost,
    content::kChromeUIGpuHost,
    content::kChromeUIHistogramHost,
    content::kChromeUIIndexedDBInternalsHost,
    content::kChromeUIMediaInternalsHost,
    content::kChromeUINetworkErrorHost,
    content::kChromeUINetworkErrorsListingHost,
    content::kChromeUIProcessInternalsHost,
    content::kChromeUIServiceWorkerInternalsHost,
#if !defined(OS_ANDROID)
    content::kChromeUITracingHost,
#endif
    content::kChromeUIUkmHost,
    content::kChromeUIWebRTCInternalsHost,
#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
    kChromeUIAppLauncherPageHost,
#endif
    kChromeUIBookmarksHost,
    kChromeUIDownloadsHost,
    kChromeUIHelpHost,
    kChromeUIInspectHost,
    kChromeUINewTabPageHost,
    kChromeUISettingsHost,
    kChromeUISystemInfoHost,
#endif
#if defined(OS_ANDROID)
    kChromeUIExploreSitesInternalsHost,
    kChromeUIOfflineInternalsHost,
    kChromeUISnippetsInternalsHost,
    kChromeUIWebApksHost,
#endif
#if defined(OS_CHROMEOS)
    kChromeUICertificateManagerHost,
    kChromeUICrostiniCreditsHost,
    kChromeUICryptohomeHost,
    kChromeUIDriveInternalsHost,
    kChromeUIFirstRunHost,
    kChromeUIMachineLearningInternalsHost,
    kChromeUINetworkHost,
    kChromeUIOobeHost,
    kChromeUIOSCreditsHost,
    kChromeUIOSSettingsHost,
    kChromeUIPowerHost,
    kChromeUIInternetConfigDialogHost,
    kChromeUIInternetDetailDialogHost,
    kChromeUIAssistantOptInHost,
#endif
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    kChromeUIDiscardsHost,
#endif
#if defined(OS_POSIX) && !defined(OS_MAC) && !defined(OS_ANDROID)
    kChromeUILinuxProxyConfigHost,
#endif
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_ANDROID)
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
const size_t kNumberOfChromeHostURLs = base::size(kChromeHostURLs);

// Add chrome://internals/* subpages here to be included in chrome://chrome-urls
// (about:about).
const char* const kChromeInternalsPathURLs[] = {
#if defined(OS_ANDROID)
    kChromeUIInternalsQueryTilesPath,
#else
    kChromeUIInternalsWebAppPath,
#endif  // defined(OS_ANDROID)
};
const size_t kNumberOfChromeInternalsPathURLs =
    base::size(kChromeInternalsPathURLs);

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
    content::kChromeUIMemoryPressureCriticalURL,
    content::kChromeUIMemoryPressureModerateURL,
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
const size_t kNumberOfChromeDebugURLs = base::size(kChromeDebugURLs);

}  // namespace chrome
