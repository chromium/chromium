// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for WebUI UI/Host/SubPage constants. Anything else go in
// chrome/common/url_constants.h.

#ifndef CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
#define CHROME_COMMON_WEBUI_URL_CONSTANTS_H_

#include <stddef.h>

#include "base/strings/string_piece_forward.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/common/buildflags.h"
#include "content/public/common/url_constants.h"
#include "media/media_buildflags.h"
#include "printing/buildflags/buildflags.h"

namespace chrome {

// chrome: components (without schemes) and URLs (including schemes).
// e.g. kChromeUIFooHost = "foo" and kChromeUIFooURL = "chrome://foo/"
// Not all components have corresponding URLs and vice versa. Only add as
// needed.
// Please keep in alphabetical order, with OS/feature specific sections below.
extern const char kChromeUIAboutHost[];
extern const char kChromeUIAboutURL[];
extern const char kChromeUIActivateSafetyCheckSettingsURL[];
extern const char kChromeUIAccessibilityHost[];
extern const char kChromeUIAllSitesPath[];
extern const char kChromeUIAPCInternalsHost[];
extern const char kChromeUIAppIconHost[];
extern const char kChromeUIAppIconURL[];
extern const char kChromeUIAppLauncherPageHost[];
extern const char kChromeUIAppsURL[];
extern const char kChromeUIAppsWithDeprecationDialogURL[];
extern const char kChromeUIAppsWithForceInstalledDeprecationDialogURL[];
extern const char kChromeUIAutofillInternalsHost[];
extern const char kChromeUIBluetoothInternalsHost[];
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUIBookmarksURL[];
extern const char kChromeUIBrowsingTopicsInternalsHost[];
extern const char kChromeUICertificateViewerHost[];
extern const char kChromeUICertificateViewerURL[];
extern const char kChromeUIChromeSigninHost[];
extern const char kChromeUIChromeSigninURL[];
extern const char kChromeUIChromeURLsHost[];
extern const char kChromeUIChromeURLsURL[];
extern const char kChromeUIComponentsHost[];
extern const char kChromeUIComponentsUrl[];
extern const char kChromeUIConflictsHost[];
extern const char kChromeUIConstrainedHTMLTestURL[];
extern const char kChromeUIContentSettingsURL[];
extern const char kChromeUICookieSettingsURL[];
extern const char kChromeUICrashHost[];
extern const char kChromeUICrashesUrl[];
extern const char kChromeUICrashesHost[];
extern const char kChromeUICreditsHost[];
extern const char kChromeUICreditsURL[];
extern const char kChromeUIDefaultHost[];
extern const char kChromeUIDelayedHangUIHost[];
extern const char kChromeUIDevToolsBlankPath[];
extern const char kChromeUIDevToolsBundledPath[];
extern const char kChromeUIDevToolsCustomPath[];
extern const char kChromeUIDevToolsHost[];
extern const char kChromeUIDevToolsRemotePath[];
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDeviceLogHost[];
extern const char kChromeUIDeviceLogUrl[];
extern const char kChromeUIDevUiLoaderURL[];
extern const char kChromeUIDiceWebSigninInterceptHost[];
extern const char kChromeUIDiceWebSigninInterceptURL[];
extern const char kChromeUIDownloadInternalsHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIDriveInternalsHost[];
extern const char kChromeUIDriveInternalsUrl[];
extern const char kChromeUIEDUCoexistenceLoginURLV2[];
extern const char kChromeUIAccessCodeCastHost[];
extern const char kChromeUIAccessCodeCastURL[];
extern const char kChromeUIExtensionIconHost[];
extern const char kChromeUIExtensionIconURL[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIExtensionsInternalsHost[];
extern const char kChromeUIExtensionsURL[];
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
extern const char kChromeUIFamilyLinkUserInternalsHost[];
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
extern const char kChromeUIFaviconHost[];
extern const char kChromeUIFaviconURL[];
extern const char kChromeUIFavicon2Host[];
extern const char kChromeUIFeedbackHost[];
extern const char kChromeUIFeedbackURL[];
extern const char kChromeUIFileiconURL[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIGCMInternalsHost[];
extern const char kChromeUIHangUIHost[];
extern const char kChromeUIHelpHost[];
extern const char kChromeUIHelpURL[];
extern const char kChromeUIHistoryClustersURL[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIHistorySyncedTabs[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIHumanPresenceInternalsHost[];
extern const char kChromeUIHumanPresenceInternalsURL[];
extern const char kChromeUIIdentityInternalsHost[];
extern const char kChromeUIImageEditorHost[];
extern const char kChromeUIImageEditorURL[];
extern const char kChromeUIImageHost[];
extern const char kChromeUIImageURL[];
extern const char kChromeUIInspectHost[];
extern const char kChromeUIInspectURL[];
extern const char kChromeUIInternalsHost[];
extern const char kChromeUIInternalsQueryTilesPath[];
extern const char kChromeUIInterstitialHost[];
extern const char kChromeUIInterstitialURL[];
extern const char kChromeUIInvalidationsHost[];
extern const char kChromeUIInvalidationsUrl[];
extern const char kChromeUIKillHost[];
extern const char kChromeUILauncherInternalsHost[];
extern const char kChromeUILocalStateHost[];
extern const char kChromeUIManagementHost[];
extern const char kChromeUIManagementURL[];
extern const char kChromeUIMediaEngagementHost[];
extern const char kChromeUIMediaHistoryHost[];
extern const char kChromeUIMediaRouterInternalsHost[];
extern const char kChromeUIMemoryInternalsHost[];
extern const char kChromeUINTPTilesInternalsHost[];
extern const char kChromeUINaClHost[];
extern const char kChromeUINetExportHost[];
extern const char kChromeUINetInternalsHost[];
extern const char kChromeUINetInternalsURL[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUINewTabIconHost[];
extern const char kChromeUINewTabPageHost[];
extern const char kChromeUINewTabPageURL[];
extern const char kChromeUINewTabPageThirdPartyHost[];
extern const char kChromeUINewTabPageThirdPartyURL[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUIOfflineInternalsHost[];
extern const char kChromeUIOmniboxHost[];
extern const char kChromeUIOmniboxURL[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kChromeUIAppDisabledURL[];
extern const char kChromeUIOsFlagsAppURL[];
extern const char kChromeUIOsUrlAppURL[];
#endif
extern const char kChromeUIPasswordManagerInternalsHost[];
extern const char kChromeUIPolicyHost[];
extern const char kChromeUIPolicyURL[];
extern const char kChromeUIPredictorsHost[];
extern const char kChromeUIPrefsInternalsHost[];
extern const char kChromeUIPrintURL[];
extern const char kChromeUIPrivacySandboxDialogHost[];
extern const char kChromeUIPrivacySandboxDialogURL[];
extern const char kChromeUIProfileInternalsHost[];
extern const char kChromeUIQuitHost[];
extern const char kChromeUIQuitURL[];
extern const char kChromeUIResetPasswordHost[];
extern const char kChromeUIResetPasswordURL[];
extern const char kChromeUIRestartHost[];
extern const char kChromeUIRestartURL[];
extern const char kChromeUISafetyPixelbookURL[];
extern const char kChromeUISafetyPixelSlateURL[];
extern const char kChromeUISegmentationInternalsHost[];
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
extern const char kChromeUISessionServiceInternalsPath[];
#endif
extern const char kChromeUISettingsHost[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUISignInInternalsHost[];
extern const char kChromeUISignInInternalsUrl[];
extern const char kChromeUISigninEmailConfirmationHost[];
extern const char kChromeUISigninEmailConfirmationURL[];
extern const char kChromeUISigninErrorHost[];
extern const char kChromeUISigninErrorURL[];
extern const char kChromeUISigninReauthHost[];
extern const char kChromeUISigninReauthURL[];
extern const char kChromeUISiteDataDeprecatedPath[];
extern const char kChromeUISiteDetailsPrefixURL[];
extern const char kChromeUISiteEngagementHost[];
extern const char kChromeUISupervisedUserPassphrasePageHost[];
extern const char kChromeUISupportToolHost[];
extern const char kChromeUISyncConfirmationHost[];
extern const char kChromeUISyncConfirmationLoadingPath[];
extern const char kChromeUISyncConfirmationURL[];
extern const char kChromeUISyncFileSystemInternalsHost[];
extern const char kChromeUISyncHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUISyncInternalsUrl[];
extern const char kChromeUISystemInfoHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUITermsURL[];
extern const char kChromeUIThemeHost[];
extern const char kChromeUIThemeURL[];
extern const char kChromeUITopChromeDomain[];
extern const char kChromeUITranslateInternalsHost[];
extern const char kChromeUIUntrustedImageEditorURL[];
extern const char kChromeUIUntrustedPrintURL[];
extern const char kChromeUIUntrustedThemeURL[];
extern const char kChromeUIUsbInternalsHost[];
extern const char kChromeUIUserActionsHost[];
extern const char kChromeUIVersionHost[];
extern const char kChromeUIVersionURL[];
extern const char kChromeUIWelcomeHost[];
extern const char kChromeUIWelcomeURL[];
extern const char kChromeUIWhatsNewHost[];
extern const char kChromeUIWhatsNewURL[];
extern const char kChromeUIWebuiGalleryHost[];

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/1003960): Remove when issue is resolved.
extern const char kChromeUIWelcomeWin10Host[];
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
extern const char kChromeUIExploreSitesInternalsHost[];
extern const char kChromeUIJavaCrashURL[];
extern const char kChromeUINativeBookmarksURL[];
extern const char kChromeUINativeExploreURL[];
extern const char kChromeUINativeHistoryURL[];
extern const char kChromeUINativeNewTabURL[];
extern const char kChromeUISnippetsInternalsHost[];
extern const char kChromeUIUntrustedVideoTutorialsHost[];
extern const char kChromeUIUntrustedVideoPlayerUrl[];
extern const char kChromeUIWebApksHost[];
#else
extern const char kChromeUIAppServiceInternalsHost[];
extern const char kChromeUINearbyInternalsHost[];
extern const char kChromeUINearbyInternalsURL[];
extern const char kChromeUIBookmarksSidePanelHost[];
extern const char kChromeUIBookmarksSidePanelURL[];
extern const char kChromeUICustomizeChromeSidePanelHost[];
extern const char kChromeUIHistoryClustersSidePanelHost[];
extern const char kChromeUIHistoryClustersSidePanelURL[];
extern const char kChromeUIReadAnythingSidePanelHost[];
extern const char kChromeUIReadAnythingSidePanelURL[];
extern const char kChromeUIReadLaterHost[];
extern const char kChromeUIReadLaterURL[];
extern const char kChromeUIUntrustedFeedURL[];
extern const char kChromeUIWebAppInternalsHost[];
extern const char kChromeUIWebUITestHost[];
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kChromeUIGpuURL[];
extern const char kChromeUIHistogramsURL[];
extern const char kChromeUINotifGeneratorURL[];
extern const char kChromeUINotifGeneratorHost[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// NOTE: If you add a URL/host please check if it should be added to
// IsSystemWebUIHost().
extern const char kChromeUIAccountManagerErrorHost[];
extern const char kChromeUIAccountManagerErrorURL[];
extern const char kChromeUIAccountMigrationWelcomeHost[];
extern const char kChromeUIAccountMigrationWelcomeURL[];
extern const char kChromeUIActivationMessageHost[];
extern const char kChromeUIAddSupervisionHost[];
extern const char kChromeUIAddSupervisionURL[];
extern const char kChromeUIArcGraphicsTracingHost[];
extern const char kChromeUIArcGraphicsTracingURL[];
extern const char kChromeUIArcOverviewTracingHost[];
extern const char kChromeUIArcOverviewTracingURL[];
extern const char kChromeUIArcPowerControlHost[];
extern const char kChromeUIArcPowerControlURL[];
extern const char kChromeUIAssistantOptInHost[];
extern const char kChromeUIAssistantOptInURL[];
extern const char kChromeUIAppDisabledHost[];
extern const char kChromeUIAudioHost[];
extern const char kChromeUIAudioURL[];
extern const char kChromeUIBluetoothPairingHost[];
extern const char kChromeUIBluetoothPairingURL[];
extern const char kChromeUICertificateManagerDialogURL[];
extern const char kChromeUICertificateManagerHost[];
extern const char kChromeUICloudUploadHost[];
extern const char kChromeUICloudUploadURL[];
extern const char kChromeUIConfirmPasswordChangeHost[];
extern const char kChromeUIConfirmPasswordChangeUrl[];
extern const char kChromeUICrostiniInstallerHost[];
extern const char kChromeUICrostiniInstallerUrl[];
extern const char kChromeUICrostiniUpgraderHost[];
extern const char kChromeUICrostiniUpgraderUrl[];
extern const char kChromeUICryptohomeHost[];
extern const char kChromeUICryptohomeURL[];
extern const char kChromeUIDeviceEmulatorHost[];
extern const char kChromeUIDiagnosticsAppURL[];
extern const char kChromeUIEmojiPickerURL[];
extern const char kChromeUIEmojiPickerHost[];
extern const char kChromeUIFirmwareUpdatesAppURL[];
extern const char kChromeUIIntenetConfigDialogURL[];
extern const char kChromeUIIntenetDetailDialogURL[];
extern const char kChromeUIInternetConfigDialogHost[];
extern const char kChromeUIInternetDetailDialogHost[];
extern const char kChromeUIBorealisCreditsHost[];
extern const char kChromeUIBorealisCreditsURL[];
extern const char kChromeUICrostiniCreditsHost[];
extern const char kChromeUICrostiniCreditsURL[];
extern const char kChromeUILockScreenNetworkHost[];
extern const char kChromeUILockScreenNetworkURL[];
extern const char kChromeUILockScreenStartReauthHost[];
extern const char kChromeUILockScreenStartReauthURL[];
extern const char kChromeUIManageMirrorSyncHost[];
extern const char kChromeUIManageMirrorSyncURL[];
extern const char kChromeUIMobileSetupHost[];
extern const char kChromeUIMobileSetupURL[];
extern const char kChromeUIMultiDeviceInternalsHost[];
extern const char kChromeUIMultiDeviceInternalsURL[];
extern const char kChromeUIMultiDeviceSetupHost[];
extern const char kChromeUIMultiDeviceSetupUrl[];
extern const char kChromeUINetworkHost[];
extern const char kChromeUINetworkUrl[];
extern const char kChromeUINotificationTesterURL[];
extern const char kChromeUINotificationTesterHost[];
extern const char kChromeUIOSCreditsHost[];
extern const char kChromeUIOSCreditsURL[];
extern const char kChromeUIOobeHost[];
extern const char kChromeUIOobeURL[];
extern const char kChromeUIParentAccessHost[];
extern const char kChromeUIParentAccessURL[];
extern const char kChromeUIPasswordChangeHost[];
extern const char kChromeUIPasswordChangeUrl[];
extern const char kChromeUIPrintManagementUrl[];
extern const char kChromeUIPowerHost[];
extern const char kChromeUIPowerUrl[];
extern const char kChromeUIScanningAppURL[];
extern const char kChromeUIScreenlockIconHost[];
extern const char kChromeUIScreenlockIconURL[];
extern const char kChromeUISetTimeHost[];
extern const char kChromeUISetTimeURL[];
extern const char kChromeUISlowHost[];
extern const char kChromeUISlowTraceHost[];
extern const char kChromeUISlowURL[];
extern const char kChromeUISmbCredentialsHost[];
extern const char kChromeUISmbCredentialsURL[];
extern const char kChromeUISmbShareHost[];
extern const char kChromeUISmbShareURL[];
extern const char kChromeUISysInternalsHost[];
extern const char kChromeUISysInternalsUrl[];
extern const char kChromeUIUntrustedCroshHost[];
extern const char kChromeUIUntrustedCroshURL[];
extern const char kChromeUIUntrustedTerminalHost[];
extern const char kChromeUIUntrustedTerminalURL[];
extern const char kChromeUIUrgentPasswordExpiryNotificationHost[];
extern const char kChromeUIUrgentPasswordExpiryNotificationUrl[];
extern const char kChromeUIUserImageHost[];
extern const char kChromeUIUserImageURL[];
extern const char kChromeUIVmHost[];
extern const char kChromeUIVmUrl[];

extern const char kOsUIAccountManagerErrorURL[];
extern const char kOsUIAccountMigrationWelcomeURL[];
extern const char kOsUIAddSupervisionURL[];
extern const char kOsUIAppDisabledURL[];
extern const char kOsUIAppServiceInternalsURL[];
extern const char kOsUICrashesURL[];
extern const char kOsUICreditsURL[];
extern const char kOsUIDeviceLogURL[];
extern const char kOsUIDriveInternalsURL[];
extern const char kOsUIEmojiPickerURL[];
extern const char kOsUIGpuURL[];
extern const char kOsUIHistogramsURL[];
extern const char kOsUIInvalidationsURL[];
extern const char kOsUILockScreenNetworkURL[];
extern const char kOsUINetExportURL[];
extern const char kOsUIMultiDeviceInternalsURL[];
extern const char kOsUINearbyInternalsURL[];
extern const char kOsUINetworkURL[];
extern const char kOsUIRestartURL[];
extern const char kOsUISettingsURL[];
extern const char kOsUISignInInternalsURL[];
extern const char kOsUISyncInternalsURL[];
extern const char kOsUISysInternalsUrl[];
extern const char kOsUISystemURL[];
extern const char kOsUITermsURL[];

// Returns true if this web UI is part of the "system UI". Generally this is
// UI that opens in a window (not a browser tab) and that on other operating
// systems would be considered part of the OS or window manager.
bool IsSystemWebUIHost(base::StringPiece host);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kChromeUIAppDisabledHost[];
extern const char kChromeUIOSSettingsHost[];
extern const char kChromeUIOSSettingsURL[];
extern const char kOsUIAboutURL[];
extern const char kOsUIComponentsURL[];
extern const char kOsUIConnectivityDiagnosticsAppURL[];
extern const char kOsUIDiagnosticsAppURL[];
extern const char kOsUIFirmwareUpdaterAppURL[];
extern const char kOsUIFlagsURL[];
extern const char kOsUIHelpAppURL[];
extern const char kOsUIPrintManagementAppURL[];
extern const char kOsUIScanningAppURL[];
extern const char kOsUIVersionURL[];
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
extern const char kChromeUIWebUIJsErrorHost[];
extern const char kChromeUIWebUIJsErrorURL[];
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kChromeUIConnectorsInternalsHost[];
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
extern const char kChromeUIDiscardsHost[];
extern const char kChromeUIDiscardsURL[];
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
extern const char kChromeUIWebAppSettingsURL[];
extern const char kChromeUIWebAppSettingsHost[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kChromeUINearbyShareHost[];
extern const char kChromeUINearbyShareURL[];
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
extern const char kChromeUILinuxProxyConfigHost[];
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
extern const char kChromeUISandboxHost[];
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
extern const char kChromeUIBrowserSwitchHost[];
extern const char kChromeUIBrowserSwitchURL[];
extern const char kChromeUIEnterpriseProfileWelcomeHost[];
extern const char kChromeUIEnterpriseProfileWelcomeURL[];
extern const char kChromeUIProfileCustomizationHost[];
extern const char kChromeUIProfileCustomizationURL[];
extern const char kChromeUIProfilePickerHost[];
extern const char kChromeUIProfilePickerUrl[];
extern const char kChromeUIProfilePickerStartupQuery[];
#endif

#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
     defined(TOOLKIT_VIEWS)) ||                         \
    defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogHost[];
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
extern const char kChromeUIPrintHost[];
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
extern const char kChromeUITabStripHost[];
extern const char kChromeUITabStripURL[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kChromeUICommanderHost[];
extern const char kChromeUICommanderURL[];
extern const char kChromeUIDownloadShelfHost[];
extern const char kChromeUIDownloadShelfURL[];
extern const char kChromeUITabSearchHost[];
extern const char kChromeUITabSearchURL[];
#endif

extern const char kChromeUIWebRtcLogsHost[];

#if BUILDFLAG(PLATFORM_CFM)
extern const char kCfmNetworkSettingsHost[];
extern const char kCfmNetworkSettingsURL[];
#endif  // BUILDFLAG(PLATFORM_CFM)

// Settings sub-pages.
extern const char kAccessibilitySubPage[];
extern const char kAddressesSubPage[];
extern const char kAppearanceSubPage[];
extern const char kAutofillSubPage[];
extern const char kClearBrowserDataSubPage[];
extern const char kContentSettingsSubPage[];
extern const char kCookieSettingsSubPage[];
extern const char kDownloadsSubPage[];
extern const char kHandlerSettingsSubPage[];
extern const char kImportDataSubPage[];
extern const char kLanguagesSubPage[];
extern const char kLanguageOptionsSubPage[];
extern const char kManageProfileSubPage[];
extern const char kOnStartupSubPage[];
extern const char kPasswordCheckSubPage[];
extern const char kPasswordManagerSubPage[];
extern const char kPaymentsSubPage[];
extern const char kPeopleSubPage[];
extern const char kPrintingSettingsSubPage[];
extern const char kPrivacyGuideSubPage[];
extern const char kPrivacySubPage[];
extern const char kResetSubPage[];
extern const char kResetProfileSettingsSubPage[];
extern const char kSafeBrowsingEnhancedProtectionSubPage[];
extern const char kSafetyCheckSubPage[];
extern const char kSearchSubPage[];
extern const char kSearchEnginesSubPage[];
extern const char kSignOutSubPage[];
extern const char kSyncSetupSubPage[];
extern const char kTriggeredResetProfileSettingsSubPage[];
extern const char kPrivacySandboxAdPersonalizationSubPage[];
extern const char kPrivacySandboxLearnMoreSubPage[];
extern const char kPrivacySandboxSubPage[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kPrivacySandboxSubPagePath[];
#endif

#if BUILDFLAG(IS_WIN)
extern const char kCleanupSubPage[];
#endif

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const char kChromeUICastFeedbackHost[];
#endif

// Extensions sub pages.
extern const char kExtensionConfigureCommandsSubPage[];

// Gets the hosts/domains that are shown in chrome://chrome-urls.
extern const char* const kChromeHostURLs[];
extern const size_t kNumberOfChromeHostURLs;

// Gets the chrome://internals pages that are shown in chrome://chrome-urls.
extern const char* const kChromeInternalsPathURLs[];
extern const size_t kNumberOfChromeInternalsPathURLs;

// "Debug" pages which are dangerous and not for general consumption.
extern const char* const kChromeDebugURLs[];
extern const size_t kNumberOfChromeDebugURLs;

}  // namespace chrome

#endif  // CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
