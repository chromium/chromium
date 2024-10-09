// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for WebUI UI/Host/SubPage constants. Anything else go in
// chrome/common/url_constants.h.

#ifndef CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
#define CHROME_COMMON_WEBUI_URL_CONSTANTS_H_

#include <stddef.h>

#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/strings/cstring_view.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "components/lens/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/common/url_constants.h"
#include "media/media_buildflags.h"
#include "printing/buildflags/buildflags.h"

namespace chrome {

// chrome: components (without schemes) and URLs (including schemes).
// e.g. kChromeUIFooHost = "foo" and kChromeUIFooURL = "chrome://foo/"
// Not all components have corresponding URLs and vice versa. Only add as
// needed.
// Please keep in alphabetical order, with OS/feature specific sections below.
inline constexpr char kChromeUIAboutHost[] = "about";
inline constexpr char kChromeUIAboutURL[] = "chrome://about/";
inline constexpr char kChromeUIAccessCodeCastHost[] = "access-code-cast";
inline constexpr char kChromeUIAccessCodeCastURL[] =
    "chrome://access-code-cast/";
inline constexpr char kChromeUIAccessibilityHost[] = "accessibility";
inline constexpr char kChromeUIActivateSafetyCheckSettingsURL[] =
    "chrome://settings/safetyCheck?activateSafetyCheck";
inline constexpr char kChromeUIAllSitesPath[] = "/content/all";
inline constexpr char kChromeUIAppIconHost[] = "app-icon";
inline constexpr char kChromeUIAppIconURL[] = "chrome://app-icon/";
inline constexpr char kChromeUIAppLauncherPageHost[] = "apps";
inline constexpr char kChromeUIAppsURL[] = "chrome://apps/";
inline constexpr char kChromeUIAppsWithDeprecationDialogURL[] =
    "chrome://apps?showDeletionDialog=";
inline constexpr char kChromeUIAppsWithForceInstalledDeprecationDialogURL[] =
    "chrome://apps?showForceInstallDialog=";
inline constexpr char kChromeUIAutofillInternalsHost[] = "autofill-internals";
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
inline constexpr char kChromeUIBatchUploadHost[] = "batch-upload";
inline constexpr char kChromeUIBatchUploadURL[] = "chrome://batch-upload/";
#endif
inline constexpr char kChromeUIBluetoothInternalsHost[] = "bluetooth-internals";
inline constexpr char kChromeUIBluetoothInternalsURL[] =
    "chrome://bluetooth-internals";
inline constexpr char kChromeUIBookmarksHost[] = "bookmarks";
inline constexpr char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
inline constexpr char kChromeUIBrowsingTopicsInternalsHost[] =
    "topics-internals";
inline constexpr char kChromeUICertificateViewerHost[] = "view-cert";
inline constexpr char kChromeUICertificateViewerURL[] = "chrome://view-cert/";
inline constexpr char kChromeUIChromeSigninHost[] = "chrome-signin";
inline constexpr char kChromeUIChromeSigninURL[] = "chrome://chrome-signin/";
inline constexpr char kChromeUIChromeURLsHost[] = "chrome-urls";
inline constexpr char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
inline constexpr char16_t kChromeUIChromeURLsURL16[] = u"chrome://chrome-urls/";
inline constexpr char kChromeUIComponentsHost[] = "components";
inline constexpr char kChromeUIComponentsUrl[] = "chrome://components";
inline constexpr char kChromeUIConflictsHost[] = "conflicts";
inline constexpr char kChromeUIConstrainedHTMLTestURL[] =
    "chrome://constrained-test/";
inline constexpr char kChromeUIContentSettingsURL[] =
    "chrome://settings/content";
inline constexpr char16_t kChromeUIContentSettingsURL16[] =
    u"chrome://settings/content";
inline constexpr char16_t kChromeUICookieSettingsURL[] =
    u"chrome://settings/cookies";
inline constexpr char kChromeUICrashesHost[] = "crashes";
inline constexpr char kChromeUICrashesUrl[] = "chrome://crashes";
inline constexpr char kChromeUICrashHost[] = "crash";
inline constexpr char kChromeUICreditsHost[] = "credits";
inline constexpr char kChromeUICreditsURL[] = "chrome://credits/";
inline constexpr char16_t kChromeUICreditsURL16[] = u"chrome://credits/";
inline constexpr char kChromeUIDataSharingInternalsHost[] =
    "data-sharing-internals";
inline constexpr char kChromeUIDefaultHost[] = "version";
inline constexpr char kChromeUIDelayedHangUIHost[] = "delayeduithreadhang";
inline constexpr char kChromeUIDeviceLogHost[] = "device-log";
inline constexpr char kChromeUIDeviceLogUrl[] = "chrome://device-log";
inline constexpr char16_t kChromeUIDeviceLogUrl16[] = u"chrome://device-log";
inline constexpr char kChromeUIDevToolsBlankPath[] = "blank";
inline constexpr char kChromeUIDevToolsBundledPath[] = "bundled";
inline constexpr char kChromeUIDevToolsCustomPath[] = "custom";
inline constexpr char kChromeUIDevToolsHost[] = "devtools";
inline constexpr char kChromeUIDevToolsRemotePath[] = "remote";
inline constexpr char kChromeUIDevToolsURL[] =
    "devtools://devtools/bundled/inspector.html";
inline constexpr char kChromeUIDevUiLoaderURL[] = "chrome://dev-ui-loader/";
inline constexpr char kChromeUIDiceWebSigninInterceptChromeSigninSubPage[] =
    "chrome-signin";
inline constexpr char kChromeUIDiceWebSigninInterceptChromeSigninURL[] =
    "chrome://signin-dice-web-intercept.top-chrome/chrome-signin";
inline constexpr char kChromeUIDiceWebSigninInterceptHost[] =
    "signin-dice-web-intercept.top-chrome";
inline constexpr char kChromeUIDiceWebSigninInterceptURL[] =
    "chrome://signin-dice-web-intercept.top-chrome/";
inline constexpr char kChromeUIDownloadInternalsHost[] = "download-internals";
inline constexpr char kChromeUIDownloadsHost[] = "downloads";
inline constexpr char kChromeUIDownloadsURL[] = "chrome://downloads/";
inline constexpr char kChromeUIDriveInternalsHost[] = "drive-internals";
inline constexpr char kChromeUIDriveInternalsUrl[] = "chrome://drive-internals";
inline constexpr char kChromeUIEDUCoexistenceLoginURLV2[] =
    "chrome://chrome-signin/edu-coexistence";
inline constexpr char kChromeUIExtensionIconHost[] = "extension-icon";
inline constexpr char kChromeUIExtensionIconURL[] = "chrome://extension-icon/";
inline constexpr char kChromeUIExtensionsHost[] = "extensions";
inline constexpr char kChromeUIExtensionsInternalsHost[] =
    "extensions-internals";
inline constexpr char kChromeUIExtensionsInternalsURL[] =
    "chrome://extensions-internals";
inline constexpr char kChromeUIExtensionsURL[] = "chrome://extensions/";
inline constexpr char kChromeUIFamilyLinkUserInternalsHost[] =
    "family-link-user-internals";
inline constexpr char kChromeUIFavicon2Host[] = "favicon2";
inline constexpr char kChromeUIFaviconHost[] = "favicon";
inline constexpr char kChromeUIFaviconURL[] = "chrome://favicon/";
inline constexpr char kChromeUIFeedbackHost[] = "feedback";
inline constexpr char kChromeUIFeedbackURL[] = "chrome://feedback/";
inline constexpr char kChromeUIFileiconURL[] = "chrome://fileicon/";
inline constexpr char kChromeUIFlagsHost[] = "flags";
inline constexpr char kChromeUIFlagsURL[] = "chrome://flags/";
inline constexpr char16_t kChromeUIFlagsURL16[] = u"chrome://flags/";
inline constexpr char kChromeUIGCMInternalsHost[] = "gcm-internals";
inline constexpr char kChromeUIHangUIHost[] = "uithreadhang";
inline constexpr char kChromeUIHelpHost[] = "help";
inline constexpr char kChromeUIHelpURL[] = "chrome://help/";
inline constexpr char kChromeUIHistoryHost[] = "history";
inline constexpr char kChromeUIHistorySyncedTabs[] = "/syncedTabs";
inline constexpr char kChromeUIHistoryURL[] = "chrome://history/";
inline constexpr char16_t kChromeUIHistoryURL16[] = u"chrome://history/";
inline constexpr char kChromeUIIdentityInternalsHost[] = "identity-internals";
inline constexpr char kChromeUIImageHost[] = "image";
inline constexpr char kChromeUIImageURL[] = "chrome://image/";
inline constexpr char kChromeUIInspectHost[] = "inspect";
inline constexpr char kChromeUIInspectURL[] = "chrome://inspect/";
inline constexpr char kChromeUIInternalsHost[] = "internals";
inline constexpr char kChromeUIInterstitialHost[] = "interstitials";
inline constexpr char kChromeUIInterstitialURL[] = "chrome://interstitials/";
inline constexpr char kChromeUIKillHost[] = "kill";
inline constexpr char kChromeUILauncherInternalsHost[] = "launcher-internals";
inline constexpr char kChromeUILauncherInternalsURL[] =
    "chrome://launcher-internals";
inline constexpr char kChromeUILensSearchBubbleHost[] = "lens-search-bubble";
inline constexpr char kChromeUILensSearchBubbleURL[] =
    "chrome://lens-search-bubble/";
inline constexpr char kChromeUILocalStateHost[] = "local-state";
inline constexpr char kChromeUILocationInternalsHost[] = "location-internals";
inline constexpr char kChromeUIManagementHost[] = "management";
inline constexpr char kChromeUIManagementURL[] = "chrome://management";
inline constexpr char16_t kChromeUIManagementURL16[] = u"chrome://management";
inline constexpr char kChromeUIMediaEngagementHost[] = "media-engagement";
inline constexpr char kChromeUIMediaRouterInternalsHost[] =
    "media-router-internals";
inline constexpr char kChromeUIMemoryInternalsHost[] = "memory-internals";
inline constexpr char kChromeUIMetricsInternalsHost[] = "metrics-internals";
inline constexpr char kChromeUINaClHost[] = "nacl";
inline constexpr char kChromeUINetExportHost[] = "net-export";
inline constexpr char kChromeUINetExportURL[] = "chrome://net-export";
inline constexpr char kChromeUINetInternalsHost[] = "net-internals";
inline constexpr char kChromeUINetInternalsURL[] = "chrome://net-internals/";
inline constexpr char kChromeUINewTabHost[] = "newtab";
inline constexpr char kChromeUINewTabPageHost[] = "new-tab-page";
inline constexpr char kChromeUINewTabPageThirdPartyHost[] =
    "new-tab-page-third-party";
inline constexpr char kChromeUINewTabPageThirdPartyURL[] =
    "chrome://new-tab-page-third-party/";
inline constexpr char kChromeUINewTabPageURL[] = "chrome://new-tab-page/";
inline constexpr char kChromeUINewTabURL[] = "chrome://newtab/";
inline constexpr char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
inline constexpr char kChromeUIOmniboxHost[] = "omnibox";
inline constexpr char kChromeUIOmniboxPopupHost[] = "omnibox-popup.top-chrome";
inline constexpr char kChromeUIOmniboxPopupURL[] =
    "chrome://omnibox-popup.top-chrome/";
inline constexpr char kChromeUIOmniboxURL[] = "chrome://omnibox/";
inline constexpr char kChromeUIPasswordManagerCheckupURL[] =
    "chrome://password-manager/checkup?start=true";
inline constexpr char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
inline constexpr char kChromeUIPasswordManagerSettingsURL[] =
    "chrome://password-manager/settings";
inline constexpr char kChromeUIPasswordManagerURL[] =
    "chrome://password-manager";
inline constexpr char kChromeUIPolicyHost[] = "policy";
inline constexpr char kChromeUIPolicyTestURL[] = "chrome://policy/test";
inline constexpr char kChromeUIPolicyURL[] = "chrome://policy/";
inline constexpr char kChromeUIPredictorsHost[] = "predictors";
inline constexpr char kChromeUIPrefsInternalsHost[] = "prefs-internals";
inline constexpr char kChromeUIPrefsInternalsURL[] = "chrome://prefs-internals";
inline constexpr char kChromeUIPrintURL[] = "chrome://print/";
inline constexpr char kChromeUIPrivacySandboxDialogCombinedPath[] = "combined";
inline constexpr char kChromeUIPrivacySandboxDialogHost[] =
    "privacy-sandbox-dialog";
inline constexpr char kChromeUIPrivacySandboxDialogNoticePath[] = "notice";
inline constexpr char kChromeUIPrivacySandboxDialogNoticeRestrictedPath[] =
    "restricted";
inline constexpr char kChromeUIPrivacySandboxDialogURL[] =
    "chrome://privacy-sandbox-dialog";
inline constexpr char16_t kChromeUIPrivacySandboxFledgeURL[] =
    u"chrome://settings/adPrivacy/sites";
inline constexpr char kChromeUIPrivacySandboxInternalsHost[] =
    "privacy-sandbox-internals";
inline constexpr char kChromeUIPrivacySandboxInternalsURL[] =
    "chrome://privacy-sandbox-internals";
inline constexpr char16_t kChromeUIPrivacySandboxManageTopicsLearnMoreURL[] =
    u"https://support.google.com/chrome?p=ad_privacy";
inline constexpr char16_t kChromeUIPrivacySandboxTopicsURL[] =
    u"chrome://settings/adPrivacy/interests";
inline constexpr char kChromeUIProfileInternalsHost[] = "profile-internals";
inline constexpr char kChromeUIQuitHost[] = "quit";
inline constexpr char kChromeUIQuitURL[] = "chrome://quit/";
inline constexpr char kChromeUIResetPasswordHost[] = "reset-password";
inline constexpr char kChromeUIResetPasswordURL[] = "chrome://reset-password/";
inline constexpr char kChromeUIRestartHost[] = "restart";
inline constexpr char kChromeUIRestartURL[] = "chrome://restart/";
inline constexpr char kChromeUISafetyPixelbookURL[] =
    "https://g.co/Pixelbook/legal";
inline constexpr char kChromeUISafetyPixelSlateURL[] =
    "https://g.co/PixelSlate/legal";
inline constexpr char kChromeUISegmentationInternalsHost[] =
    "segmentation-internals";
inline constexpr char kChromeUISensorInfoHost[] = "sensor-info";
inline constexpr char kChromeUISensorInfoURL[] = "chrome://sensor-info/";
inline constexpr char kChromeUISettingsHost[] = "settings";
inline constexpr char16_t kChromeUISettingsHost16[] = u"settings";
inline constexpr char kChromeUISettingsURL[] = "chrome://settings/";
inline constexpr char16_t kChromeUISettingsURL16[] = u"chrome://settings/";
inline constexpr char kChromeUISigninEmailConfirmationHost[] =
    "signin-email-confirmation";
inline constexpr char kChromeUISigninEmailConfirmationURL[] =
    "chrome://signin-email-confirmation";
inline constexpr char kChromeUISigninErrorHost[] = "signin-error";
inline constexpr char kChromeUISigninErrorURL[] = "chrome://signin-error/";
inline constexpr char kChromeUISignInInternalsHost[] = "signin-internals";
inline constexpr char kChromeUISignInInternalsUrl[] =
    "chrome://signin-internals";
inline constexpr char kChromeUISigninReauthHost[] = "signin-reauth";
inline constexpr char kChromeUISigninReauthURL[] = "chrome://signin-reauth/";
inline constexpr char kChromeUISiteDataDeprecatedPath[] = "/siteData";
inline constexpr char kChromeUISiteEngagementHost[] = "site-engagement";
inline constexpr char kChromeUISuggestInternalsHost[] = "suggest-internals";
inline constexpr char kChromeUISuggestInternalsURL[] =
    "chrome://suggest-internals/";
inline constexpr char kChromeUISupervisedUserPassphrasePageHost[] =
    "managed-user-passphrase";
inline constexpr char kChromeUISupportToolHost[] = "support-tool";
inline constexpr char kChromeUISupportToolURL[] = "chrome://support-tool";
inline constexpr char kChromeUISyncConfirmationHost[] = "sync-confirmation";
inline constexpr char kChromeUISyncConfirmationLoadingPath[] = "loading";
inline constexpr char kChromeUISyncConfirmationURL[] =
    "chrome://sync-confirmation/";
inline constexpr char kChromeUISyncHost[] = "sync";
inline constexpr char kChromeUISyncInternalsHost[] = "sync-internals";
inline constexpr char kChromeUISyncInternalsUrl[] = "chrome://sync-internals";
inline constexpr char kChromeUISystemInfoHost[] = "system";
inline constexpr char kChromeUITermsHost[] = "terms";
inline constexpr char kChromeUITermsURL[] = "chrome://terms/";
inline constexpr char kChromeUIThemeHost[] = "theme";
inline constexpr char kChromeUIThemeURL[] = "chrome://theme/";
inline constexpr char kChromeUITopChromeDomain[] = "top-chrome";
inline constexpr char kChromeUITranslateInternalsHost[] = "translate-internals";
inline constexpr char kChromeUIUntrustedComposeHost[] = "compose";
inline constexpr char kChromeUIUntrustedComposeUrl[] =
    "chrome-untrusted://compose/";
inline constexpr char kChromeUIUntrustedDataSharingHost[] = "data-sharing";
inline constexpr char kChromeUIUntrustedDataSharingURL[] =
    "chrome-untrusted://data-sharing/";
inline constexpr char kChromeUIUntrustedDataSharingAPIURL[] =
    "chrome-untrusted://data-sharing/data_sharing_api.html";
inline constexpr char kChromeUIUntrustedImageEditorURL[] =
    "chrome-untrusted://image-editor/";
inline constexpr char kChromeUIUntrustedPrintURL[] =
    "chrome-untrusted://print/";
inline constexpr char kChromeUIUntrustedPrivacySandboxDialogURL[] =
    "chrome-untrusted://privacy-sandbox-dialog/";
inline constexpr char
    kChromeUIUntrustedPrivacySandboxDialogPrivacyPolicyPath[] =
        "privacy-policy";
inline constexpr char kChromeUIUntrustedThemeURL[] =
    "chrome-untrusted://theme/";
inline constexpr char kChromeUIUsbInternalsHost[] = "usb-internals";
inline constexpr char kChromeUIUserActionsHost[] = "user-actions";
inline constexpr char kChromeUIVersionHost[] = "version";
inline constexpr char kChromeUIVersionURL[] = "chrome://version/";
inline constexpr char16_t kChromeUIVersionURL16[] = u"chrome://version/";
inline constexpr char kChromeUIWebRtcLogsHost[] = "webrtc-logs";
inline constexpr char kChromeUIWebuiGalleryHost[] = "webui-gallery";
inline constexpr char kChromeUIWelcomeHost[] = "welcome";
inline constexpr char kChromeUIWelcomeURL[] = "chrome://welcome/";

#if BUILDFLAG(IS_ANDROID)
inline constexpr char kChromeUIInternalsQueryTilesPath[] = "query-tiles";
inline constexpr char kChromeUIJavaCrashURL[] = "chrome://java-crash/";
inline constexpr char kChromeUINativeBookmarksURL[] =
    "chrome-native://bookmarks/";
inline constexpr char kChromeUINativeExploreURL[] = "chrome-native://explore";
inline constexpr char kChromeUINativeNewTabURL[] = "chrome-native://newtab/";
inline constexpr char kChromeUIOfflineInternalsHost[] = "offline-internals";
inline constexpr char kChromeUISnippetsInternalsHost[] = "snippets-internals";
inline constexpr char kChromeUIWebApksHost[] = "webapks";
#else
inline constexpr char kAdPrivacySubPagePath[] = "/adPrivacy";
inline constexpr char kChromeUIAppServiceInternalsHost[] =
    "app-service-internals";
inline constexpr char kChromeUIAppServiceInternalsURL[] =
    "chrome://app-service-internals";
inline constexpr char kChromeUIBookmarksSidePanelHost[] =
    "bookmarks-side-panel.top-chrome";
inline constexpr char kChromeUIBookmarksSidePanelURL[] =
    "chrome://bookmarks-side-panel.top-chrome/";
inline constexpr char kChromeUICustomizeChromeSidePanelHost[] =
    "customize-chrome-side-panel.top-chrome";
inline constexpr char kChromeUICustomizeChromeSidePanelURL[] =
    "chrome://customize-chrome-side-panel.top-chrome";
inline constexpr char kChromeUIHistoryClustersSidePanelHost[] =
    "history-clusters-side-panel.top-chrome";
inline constexpr char kChromeUIHistoryClustersSidePanelURL[] =
    "chrome://history-clusters-side-panel.top-chrome/";
inline constexpr char kChromeUILensHost[] = "lens";
inline constexpr char kChromeUILensSidePanelHost[] = "lens";
inline constexpr char kChromeUILensUntrustedSidePanelAPIURL[] =
    "chrome-untrusted://lens/side_panel/side_panel.html";
inline constexpr char kChromeUILensUntrustedSidePanelURL[] =
    "chrome-untrusted://lens/";
inline constexpr char kChromeUILensOverlayHost[] = "lens-overlay";
inline constexpr char kChromeUILensOverlayUntrustedURL[] =
    "chrome-untrusted://lens-overlay/";
inline constexpr char kChromeUINearbyInternalsHost[] = "nearby-internals";
inline constexpr char kChromeUINearbyInternalsURL[] =
    "chrome://nearby-internals";
inline constexpr char kChromeUINearbyShareHost[] = "nearby";
inline constexpr char kChromeUINearbyShareURL[] = "chrome://nearby/";
inline constexpr char kChromeUIOnDeviceInternalsHost[] = "on-device-internals";
inline constexpr char kChromeUIReadLaterHost[] = "read-later.top-chrome";
inline constexpr char kChromeUIReadLaterURL[] =
    "chrome://read-later.top-chrome/";
inline constexpr char kChromeUISearchEngineChoiceHost[] =
    "search-engine-choice";
inline constexpr char kChromeUISearchEngineChoiceURL[] =
    "chrome://search-engine-choice";
inline constexpr char kChromeUITabSearchHost[] = "tab-search.top-chrome";
inline constexpr char kChromeUITabSearchURL[] =
    "chrome://tab-search.top-chrome/";
inline constexpr char kChromeUIUntrustedCompanionSidePanelHost[] =
    "companion-side-panel.top-chrome";
inline constexpr char kChromeUIUntrustedCompanionSidePanelURL[] =
    "chrome-untrusted://companion-side-panel.top-chrome/";
inline constexpr char kChromeUIUntrustedFeedURL[] = "chrome-untrusted://feed/";
inline constexpr char kChromeUIUntrustedHatsHost[] = "hats";
inline constexpr char kChromeUIUntrustedHatsURL[] = "chrome-untrusted://hats/";
inline constexpr char kChromeUIUntrustedReadAnythingSidePanelHost[] =
    "read-anything-side-panel.top-chrome";
inline constexpr char kChromeUIUntrustedReadAnythingSidePanelURL[] =
    "chrome-untrusted://read-anything-side-panel.top-chrome/";
inline constexpr char kChromeUIUntrustedWebUITestURL[] =
    "chrome-untrusted://webui-test/";
inline constexpr char kChromeUIWebAppInternalsHost[] = "web-app-internals";
inline constexpr char kChromeUIWebAppInternalsURL[] =
    "chrome://web-app-internals";
inline constexpr char kChromeUIWebUITestHost[] = "webui-test";
inline constexpr char kCookiesSubPagePath[] = "/cookies";
inline constexpr char kTrackingProtectionSubPagePath[] = "/trackingProtection";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// NOTE: If you add a URL/host please check if it should be added to
// IsSystemWebUIHost().
inline constexpr char kChromeUIAccountManagerErrorHost[] =
    "account-manager-error";
inline constexpr char kChromeUIAccountManagerErrorURL[] =
    "chrome://account-manager-error";
inline constexpr char kChromeUIAccountMigrationWelcomeHost[] =
    "account-migration-welcome";
inline constexpr char kChromeUIAccountMigrationWelcomeURL[] =
    "chrome://account-migration-welcome";
inline constexpr char kChromeUIAddSupervisionHost[] = "add-supervision";
inline constexpr char kChromeUIAddSupervisionURL[] =
    "chrome://add-supervision/";
inline constexpr char kChromeUIAppDisabledHost[] = "app-disabled";
inline constexpr char kChromeUIAppInstallDialogHost[] = "app-install-dialog";
inline constexpr char kChromeUIAppInstallDialogURL[] =
    "chrome://app-install-dialog/";
inline constexpr char kChromeUIArcOverviewTracingHost[] =
    "arc-overview-tracing";
inline constexpr char kChromeUIArcOverviewTracingURL[] =
    "chrome://arc-overview-tracing/";
inline constexpr char kChromeUIArcPowerControlHost[] = "arc-power-control";
inline constexpr char kChromeUIArcPowerControlURL[] =
    "chrome://arc-power-control/";
inline constexpr char kChromeUIAssistantOptInHost[] = "assistant-optin";
inline constexpr char kChromeUIAssistantOptInURL[] =
    "chrome://assistant-optin/";
inline constexpr char kChromeUIAudioHost[] = "audio";
inline constexpr char kChromeUIAudioURL[] = "chrome://audio/";
inline constexpr char kChromeUIBluetoothPairingHost[] = "bluetooth-pairing";
inline constexpr char kChromeUIBluetoothPairingURL[] =
    "chrome://bluetooth-pairing/";
inline constexpr char kChromeUIBorealisCreditsHost[] = "borealis-credits";
inline constexpr char kChromeUIBorealisCreditsURL[] =
    "chrome://borealis-credits/";
inline constexpr char kChromeUIBorealisInstallerHost[] = "borealis-installer";
inline constexpr char kChromeUIBorealisInstallerUrl[] =
    "chrome://borealis-installer";
inline constexpr char kChromeUICloudUploadHost[] = "cloud-upload";
inline constexpr char kChromeUICloudUploadURL[] = "chrome://cloud-upload/";
inline constexpr char kChromeUIConfirmPasswordChangeHost[] =
    "confirm-password-change";
inline constexpr char kChromeUIConfirmPasswordChangeUrl[] =
    "chrome://confirm-password-change";
inline constexpr char kChromeUIConnectivityDiagnosticsAppURL[] =
    "chrome://connectivity-diagnostics";
inline constexpr char kChromeUICrostiniCreditsHost[] = "crostini-credits";
inline constexpr char kChromeUICrostiniCreditsURL[] =
    "chrome://crostini-credits/";
inline constexpr char16_t kChromeUICrostiniCreditsURL16[] =
    u"chrome://crostini-credits/";
inline constexpr char kChromeUICrostiniInstallerHost[] = "crostini-installer";
inline constexpr char kChromeUICrostiniInstallerUrl[] =
    "chrome://crostini-installer";
inline constexpr char kChromeUICrostiniUpgraderHost[] = "crostini-upgrader";
inline constexpr char kChromeUICrostiniUpgraderUrl[] =
    "chrome://crostini-upgrader";
inline constexpr char kChromeUICryptohomeHost[] = "cryptohome";
inline constexpr char kChromeUICryptohomeURL[] = "chrome://cryptohome";
inline constexpr char kChromeUIDeviceEmulatorHost[] = "device-emulator";
inline constexpr char kChromeUIDeviceEmulatorURL[] = "chrome://device-emulator";
inline constexpr char kChromeUIDiagnosticsAppURL[] = "chrome://diagnostics";
inline constexpr char kChromeUIEmojiPickerHost[] = "emoji-picker";
inline constexpr char kChromeUIEmojiPickerURL[] = "chrome://emoji-picker/";
inline constexpr char kChromeUIEnterpriseReportingHost[] =
    "enterprise-reporting";
inline constexpr char kChromeUIEnterpriseReportingURL[] =
    "chrome://enterprise-reporting";
inline constexpr char kChromeUIExtendedUpdatesDialogHost[] =
    "extended-updates-dialog";
inline constexpr char kChromeUIExtendedUpdatesDialogURL[] =
    "chrome://extended-updates-dialog";
inline constexpr char kChromeUIFirmwareUpdaterAppURL[] =
    "chrome://accessory-update";
inline constexpr char kChromeUIHealthdInternalsHost[] = "healthd-internals";
inline constexpr char kChromeUIHealthdInternalsURL[] =
    "chrome://healthd-internals";
inline constexpr char kChromeUIInternetConfigDialogHost[] =
    "internet-config-dialog";
inline constexpr char kChromeUIInternetConfigDialogURL[] =
    "chrome://internet-config-dialog/";
inline constexpr char kChromeUIInternetDetailDialogHost[] =
    "internet-detail-dialog";
inline constexpr char kChromeUIInternetDetailDialogURL[] =
    "chrome://internet-detail-dialog/";
inline constexpr char kChromeUILocalFilesMigrationHost[] =
    "local-files-migration";
inline constexpr char kChromeUILocalFilesMigrationURL[] =
    "chrome://local-files-migration/";
inline constexpr char kChromeUILockScreenNetworkHost[] = "lock-network";
inline constexpr char kChromeUILockScreenNetworkURL[] = "chrome://lock-network";
inline constexpr char kChromeUILockScreenStartReauthHost[] = "lock-reauth";
inline constexpr char kChromeUILockScreenStartReauthURL[] =
    "chrome://lock-reauth";
inline constexpr char kChromeUIManageMirrorSyncHost[] = "manage-mirrorsync";
inline constexpr char kChromeUIManageMirrorSyncURL[] =
    "chrome://manage-mirrorsync";
inline constexpr char kChromeUIMobileSetupHost[] = "mobilesetup";
inline constexpr char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
inline constexpr char kChromeUIMultiDeviceInternalsHost[] =
    "multidevice-internals";
inline constexpr char kChromeUIMultiDeviceInternalsURL[] =
    "chrome://multidevice-internals";
inline constexpr char kChromeUIMultiDeviceSetupHost[] = "multidevice-setup";
inline constexpr char kChromeUIMultiDeviceSetupUrl[] =
    "chrome://multidevice-setup";
inline constexpr char kChromeUINetworkHost[] = "network";
inline constexpr char kChromeUINetworkUrl[] = "chrome://network";
inline constexpr char kChromeUINotificationTesterHost[] = "notification-tester";
inline constexpr char kChromeUINotificationTesterURL[] =
    "chrome://notification-tester";
inline constexpr char kChromeUIOfficeFallbackHost[] = "office-fallback";
inline constexpr char kChromeUIOfficeFallbackURL[] =
    "chrome://office-fallback/";
inline constexpr char kChromeUIOobeHost[] = "oobe";
inline constexpr char kChromeUIOobeURL[] = "chrome://oobe/";
inline constexpr char kChromeUIOSCreditsHost[] = "os-credits";
inline constexpr char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
inline constexpr char16_t kChromeUIOSCreditsURL16[] = u"chrome://os-credits/";
inline constexpr char kChromeUIParentAccessHost[] = "parent-access";
inline constexpr char kChromeUIParentAccessURL[] = "chrome://parent-access/";
inline constexpr char kChromeUIPasswordChangeHost[] = "password-change";
inline constexpr char kChromeUIPasswordChangeUrl[] = "chrome://password-change";
inline constexpr char kChromeUIPowerHost[] = "power";
inline constexpr char kChromeUIPowerUrl[] = "chrome://power";
inline constexpr char kChromeUIPrintManagementUrl[] =
    "chrome://print-management";
inline constexpr char kChromeUIRemoteManagementCurtainHost[] =
    "security-curtain";
inline constexpr char kChromeUISanitizeAppURL[] = "chrome://sanitize";
inline constexpr char kChromeUIScanningAppURL[] = "chrome://scanning";
inline constexpr char kChromeUISetTimeHost[] = "set-time";
inline constexpr char kChromeUISetTimeURL[] = "chrome://set-time/";
inline constexpr char kChromeUISlowHost[] = "slow";
inline constexpr char kChromeUISlowTraceHost[] = "slow_trace";
inline constexpr char kChromeUISlowURL[] = "chrome://slow/";
inline constexpr char kChromeUISmbCredentialsHost[] = "smb-credentials-dialog";
inline constexpr char kChromeUISmbCredentialsURL[] =
    "chrome://smb-credentials-dialog/";
inline constexpr char kChromeUISmbShareHost[] = "smb-share-dialog";
inline constexpr char kChromeUISmbShareURL[] = "chrome://smb-share-dialog/";
inline constexpr char kChromeUISysInternalsHost[] = "sys-internals";
inline constexpr char kChromeUISysInternalsUrl[] = "chrome://sys-internals";
inline constexpr char kChromeUIUntrustedCroshHost[] = "crosh";
inline constexpr char kChromeUIUntrustedCroshURL[] =
    "chrome-untrusted://crosh/";
inline constexpr char kChromeUIUntrustedTerminalHost[] = "terminal";
inline constexpr char kChromeUIUntrustedTerminalURL[] =
    "chrome-untrusted://terminal/";
inline constexpr char kChromeUIUrgentPasswordExpiryNotificationHost[] =
    "urgent-password-expiry-notification";
inline constexpr char kChromeUIUrgentPasswordExpiryNotificationUrl[] =
    "chrome://urgent-password-expiry-notification/";
inline constexpr char kChromeUIUserImageHost[] = "userimage";
inline constexpr char kChromeUIUserImageURL[] = "chrome://userimage/";
inline constexpr char kChromeUIVcTrayTesterHost[] = "vc-tray-tester";
inline constexpr char kChromeUIVcTrayTesterURL[] = "chrome://vc-tray-tester";
inline constexpr char kChromeUIVmHost[] = "vm";
inline constexpr char kChromeUIVmUrl[] = "chrome://vm";

// Returns true if this web UI is part of the "system UI". Generally this is
// UI that opens in a window (not a browser tab) and that on other operating
// systems would be considered part of the OS or window manager.
bool IsSystemWebUIHost(std::string_view host);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
inline constexpr char kChromeUIAppDisabledURL[] = "chrome://app-disabled";
inline constexpr char kChromeUIDlpInternalsHost[] = "dlp-internals";
inline constexpr char kChromeUIDlpInternalsURL[] = "chrome://dlp-internals";
inline constexpr char kChromeUIGpuURL[] = "chrome://gpu";
inline constexpr char kChromeUIHistogramsURL[] = "chrome://histograms";
inline constexpr char kChromeUIKerberosInBrowserHost[] = "kerberos-in-browser";
inline constexpr char kChromeUIKerberosInBrowserURL[] =
    "chrome://kerberos-in-browser";
inline constexpr char kChromeUILocationInternalsURL[] =
    "chrome://location-internals";
inline constexpr char kChromeUIOsFlagsAppURL[] = "chrome://flags/";
inline constexpr char kChromeUIOSSettingsHost[] = "os-settings";
inline constexpr char kChromeUIOSSettingsURL[] = "chrome://os-settings/";
inline constexpr char kChromeUIOsUrlAppURL[] = "chrome://internal/";
inline constexpr char kChromeUISystemURL[] = "chrome://system";
inline constexpr char16_t kChromeUISystemURL16[] = u"chrome://system";
inline constexpr char kOsUIComponentsURL[] = "os://components";
inline constexpr char kOsUIConnectivityDiagnosticsAppURL[] =
    "os://connectivity-diagnostics";
inline constexpr char16_t kOsUIDeviceLogURL[] = u"os://device-log";
inline constexpr char kOsUIDiagnosticsAppURL[] = "os://diagnostics";
inline constexpr char kOsUIFirmwareUpdaterAppURL[] = "os://accessory-update";
inline constexpr char kOsUIFlagsURL[] = "os://flags";
inline constexpr char kOsUIHelpAppURL[] = "os://help-app";
inline constexpr char kOsUIPrintManagementAppURL[] = "os://print-management";
inline constexpr char kOsUIScanningAppURL[] = "os://scanning";
inline constexpr char kOsUIShortcutCustomizationAppURL[] =
    "os://shortcut-customization";
inline constexpr char16_t kOsUISystemURL[] = u"os://system";
inline constexpr char kOsUIVersionURL[] = "os://version";
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
inline constexpr char kChromeUIWebUIJsErrorHost[] = "webuijserror";
inline constexpr char kChromeUIWebUIJsErrorURL[] = "chrome://webuijserror/";
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
inline constexpr char kChromeUIConnectorsInternalsHost[] =
    "connectors-internals";
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
inline constexpr char kChromeUIDiscardsHost[] = "discards";
inline constexpr char kChromeUIDiscardsURL[] = "chrome://discards/";
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
inline constexpr char kChromeUIWebAppSettingsHost[] = "app-settings";
inline constexpr char kChromeUIWebAppSettingsURL[] = "chrome://app-settings/";
inline constexpr char kChromeUIWhatsNewHost[] = "whats-new";
inline constexpr char kChromeUIWhatsNewURL[] = "chrome://whats-new/";
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
inline constexpr char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
inline constexpr char kChromeUISandboxHost[] = "sandbox";
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
inline constexpr char kChromeUIBrowserSwitchHost[] = "browser-switch";
inline constexpr char kChromeUIBrowserSwitchURL[] = "chrome://browser-switch/";
inline constexpr char kChromeUIIntroDefaultBrowserSubPage[] = "default-browser";
inline constexpr char kChromeUIIntroDefaultBrowserURL[] =
    "chrome://intro/default-browser";
inline constexpr char kChromeUIIntroHost[] = "intro";
inline constexpr char kChromeUIIntroURL[] = "chrome://intro";
inline constexpr char kChromeUIManagedUserProfileNoticeHost[] =
    "managed-user-profile-notice";
inline constexpr char kChromeUIManagedUserProfileNoticeUrl[] =
    "chrome://managed-user-profile-notice/";
inline constexpr char kChromeUIProfileCustomizationHost[] =
    "profile-customization";
inline constexpr char kChromeUIProfileCustomizationURL[] =
    "chrome://profile-customization";
inline constexpr char kChromeUIProfilePickerHost[] = "profile-picker";
inline constexpr char kChromeUIProfilePickerStartupQuery[] = "startup";
inline constexpr char kChromeUIProfilePickerUrl[] = "chrome://profile-picker/";
#endif

#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
     defined(TOOLKIT_VIEWS)) ||                         \
    defined(USE_AURA)
inline constexpr char kChromeUITabModalConfirmDialogHost[] =
    "tab-modal-confirm-dialog";
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
inline constexpr char kChromeUIPrintHost[] = "print";
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
inline constexpr char kChromeUISessionServiceInternalsPath[] =
    "session-service";
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
inline constexpr char kChromeUITabStripHost[] = "tab-strip.top-chrome";
inline constexpr char kChromeUITabStripURL[] = "chrome://tab-strip.top-chrome";
#endif

// Settings sub-pages.
//
// NOTE: Add sub page paths to |kChromeSettingsSubPages| in
// chrome_autocomplete_provider_client.cc to be listed by the built-in
// AutocompleteProvider.

inline constexpr char kAccessibilitySubPage[] = "accessibility";
inline constexpr char kAddressesSubPage[] = "addresses";
inline constexpr char kAdPrivacySubPage[] = "adPrivacy";
inline constexpr char kAllSitesSettingsSubpage[] = "content/all";
inline constexpr char kAppearanceSubPage[] = "appearance";
inline constexpr char kAutofillSubPage[] = "autofill";
inline constexpr char kAutofillPredictionImprovementsSubPage[] =
    "autofillPredictionImprovements";
inline constexpr char kClearBrowserDataSubPage[] = "clearBrowserData";
inline constexpr char kContentSettingsSubPage[] = "content";
inline constexpr char kCookieSettingsSubPage[] = "cookies";
inline constexpr char kDefaultBrowserSubPage[] = "defaultBrowser";
inline constexpr char kDownloadsSubPage[] = "downloads";
inline constexpr char kExperimentalAISettingsSubPage[] = "ai";
inline constexpr char kFileSystemSettingsSubpage[] =
    "content/filesystem/siteDetails";
inline constexpr char kFileSystemSubpage[] = "content/filesystem";
inline constexpr char kHandlerSettingsSubPage[] = "handlers";
inline constexpr char kHistorySearchSubpage[] = "historySearch";
inline constexpr char kImportDataSubPage[] = "importData";
inline constexpr char kLanguageOptionsSubPage[] = "languages";
inline constexpr char kLanguagesSubPage[] = "languages/details";
inline constexpr char kManageProfileSubPage[] = "manageProfile";
inline constexpr char kOfferWritingHelpSubpage[] = "content/offerWritingHelp";
inline constexpr char kOnDeviceSiteDataSubpage[] = "content/siteData";
inline constexpr char kOnStartupSubPage[] = "onStartup";
inline constexpr char kPasskeysSubPage[] = "passkeys";
inline constexpr char kPasswordCheckSubPage[] = "passwords/check?start=true";
inline constexpr char kPasswordManagerSubPage[] = "passwords";
inline constexpr char kPaymentsSubPage[] = "payments";
inline constexpr char kPeopleSubPage[] = "people";
inline constexpr char kPerformanceSubPage[] = "performance";
inline constexpr char kPrintingSettingsSubPage[] = "printing";
inline constexpr char kPrivacyGuideSubPage[] = "privacy/guide";
inline constexpr char kPrivacySandboxMeasurementSubpage[] =
    "adPrivacy/measurement";
inline constexpr char kPrivacySubPage[] = "privacy";
inline constexpr char kResetProfileSettingsSubPage[] = "resetProfileSettings";
inline constexpr char kResetSubPage[] = "reset";
inline constexpr char kSafeBrowsingEnhancedProtectionSubPage[] =
    "security?q=enhanced";
inline constexpr char kSafetyCheckSubPage[] = "safetyCheck";
inline constexpr char kSafetyHubSubPage[] = "safetyCheck";
inline constexpr char kSearchEnginesSubPage[] = "searchEngines";
inline constexpr char kSearchSubPage[] = "search";
inline constexpr char kSignOutSubPage[] = "signOut";
inline constexpr char kSiteDetailsSubpage[] = "content/siteDetails";
inline constexpr char kSyncSetupSubPage[] = "syncSetup";
inline constexpr char kTriggeredResetProfileSettingsSubPage[] =
    "triggeredResetProfileSettings";

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/40647483): Remove when issue is resolved.
inline constexpr char kChromeUIWelcomeWin10Host[] = "welcome-win10";
inline constexpr char kCleanupSubPage[] = "cleanup";
#endif

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr char kChromeUICastFeedbackHost[] = "cast-feedback";
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
inline constexpr char kChromeUILensURL[] = "chrome://lens/";
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
inline constexpr char kChromeUICertificateManagerDialogURL[] =
    "chrome://certificate-manager";
inline constexpr char kChromeUICertificateManagerHost[] = "certificate-manager";
inline constexpr char kChromeUICertificateRedirectPath[] = "/certificates";
inline constexpr char kChromeUICertificateRedirectURL[] =
    "chrome://settings/certificates";
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

// Extensions sub pages.
inline constexpr char kExtensionConfigureCommandsSubPage[] =
    "configureCommands";

// Gets the hosts/domains that are shown in chrome://chrome-urls.
base::span<const base::cstring_view> ChromeURLHosts();

// Gets the path strings of chrome://internals URLs that are shown in
// chrome://chrome-urls.
base::span<const base::cstring_view> ChromeInternalsURLPaths();

// Gets the URL strings of "debug" pages which are dangerous and not for general
// consumption.
base::span<const base::cstring_view> ChromeDebugURLs();

}  // namespace chrome

#endif  // CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
