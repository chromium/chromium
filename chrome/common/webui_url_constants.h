// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for WebUI UI/Host/SubPage constants. Anything else go in
// chrome/common/url_constants.h.

#ifndef CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
#define CHROME_COMMON_WEBUI_URL_CONSTANTS_H_

#include <stddef.h>

#include "build/build_config.h"
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
extern const char kChromeUIAccessibilityHost[];
extern const char kChromeUIAppLauncherPageHost[];
extern const char kChromeUIAppListStartPageURL[];
extern const char kChromeUIAppsURL[];
extern const char kChromeUIBluetoothInternalsHost[];
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUIBookmarksURL[];
extern const char kChromeUICertificateViewerDialogHost[];
extern const char kChromeUICertificateViewerDialogURL[];
extern const char kChromeUICertificateViewerHost[];
extern const char kChromeUICertificateViewerURL[];
extern const char kChromeUIChromeSigninHost[];
extern const char kChromeUIChromeSigninURL[];
extern const char kChromeUIChromeURLsHost[];
extern const char kChromeUIChromeURLsURL[];
extern const char kChromeUIComponentsHost[];
extern const char kChromeUIConflictsHost[];
extern const char kChromeUIConflictsURL[];
extern const char kChromeUIConstrainedHTMLTestURL[];
extern const char kChromeUIContentSettingsURL[];
extern const char kChromeUICrashHost[];
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
extern const char kChromeUIDevicesHost[];
extern const char kChromeUIDevicesURL[];
extern const char kChromeUIDomainReliabilityInternalsHost[];
extern const char kChromeUIDownloadInternalsHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIDriveInternalsHost[];
extern const char kChromeUIExtensionIconHost[];
extern const char kChromeUIExtensionIconURL[];
extern const char kChromeUIExtensionsFrameHost[];
extern const char kChromeUIExtensionsFrameURL[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIExtensionsInternalsHost[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIFaviconHost[];
extern const char kChromeUIFaviconURL[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIFlashHost[];
extern const char kChromeUIGCMInternalsHost[];
extern const char kChromeUIHangUIHost[];
extern const char kChromeUIHelpHost[];
extern const char kChromeUIHelpURL[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIIdentityInternalsHost[];
extern const char kChromeUIInspectHost[];
extern const char kChromeUIInspectURL[];
extern const char kChromeUIInterstitialHost[];
extern const char kChromeUIInterstitialURL[];
extern const char kChromeUIInterventionsInternalsHost[];
extern const char kChromeUIInvalidationsHost[];
extern const char kChromeUIKillHost[];
extern const char kChromeUILocalStateHost[];
extern const char kChromeUIManagementHost[];
extern const char kChromeUIManagementURL[];
extern const char kChromeUIMdUserManagerHost[];
extern const char kChromeUIMdUserManagerUrl[];
extern const char kChromeUIMediaEngagementHost[];
extern const char kChromeUIMediaRouterHost[];
extern const char kChromeUIMediaRouterURL[];
extern const char kChromeUIMediaRouterInternalsHost[];
extern const char kChromeUIMediaRouterInternalsURL[];
extern const char kChromeUIMemoryInternalsHost[];
extern const char kChromeUINTPTilesInternalsHost[];
extern const char kChromeUINaClHost[];
extern const char kChromeUINetExportHost[];
extern const char kChromeUINetInternalsHost[];
extern const char kChromeUINetInternalsURL[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUINewTabIconHost[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUIOfflineInternalsHost[];
extern const char kChromeUIOmniboxHost[];
extern const char kChromeUIOmniboxURL[];
extern const char kChromeUIPasswordManagerInternalsHost[];
extern const char kChromeUIPhysicalWebHost[];
extern const char kChromeUIPolicyHost[];
extern const char kChromeUIPolicyToolHost[];
extern const char kChromeUIPolicyURL[];
extern const char kChromeUIPredictorsHost[];
extern const char kChromeUIPrefsInternalsHost[];
extern const char kChromeUIPrintURL[];
extern const char kChromeUIQuitHost[];
extern const char kChromeUIQuitURL[];
extern const char kChromeUIQuotaInternalsHost[];
extern const char kChromeUIResetPasswordHost[];
extern const char kChromeUIResetPasswordURL[];
extern const char kChromeUIRestartHost[];
extern const char kChromeUIRestartURL[];
extern const char kChromeUISafetyURL[];
extern const char kChromeUISettingsHost[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUISignInInternalsHost[];
extern const char kChromeUISigninEmailConfirmationHost[];
extern const char kChromeUISigninEmailConfirmationURL[];
extern const char kChromeUISigninErrorHost[];
extern const char kChromeUISigninErrorURL[];
extern const char kChromeUISiteDetailsPrefixURL[];
extern const char kChromeUISiteEngagementHost[];
extern const char kChromeUISuggestionsHost[];
extern const char kChromeUISuggestionsURL[];
extern const char kChromeUISupervisedUserInternalsHost[];
extern const char kChromeUISupervisedUserPassphrasePageHost[];
extern const char kChromeUISyncConfirmationHost[];
extern const char kChromeUISyncConfirmationURL[];
extern const char kChromeUISyncConsentBumpURL[];
extern const char kChromeUISyncFileSystemInternalsHost[];
extern const char kChromeUISyncHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUISyncResourcesHost[];
extern const char kChromeUISystemInfoHost[];
extern const char kChromeUITaskSchedulerInternalsHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUITermsURL[];
extern const char kChromeUIThemeHost[];
extern const char kChromeUIThemeURL[];
extern const char kChromeUIThumbnailHost2[];
extern const char kChromeUIThumbnailHost[];
extern const char kChromeUIThumbnailListHost[];
extern const char kChromeUIThumbnailURL[];
extern const char kChromeUITranslateInternalsHost[];
extern const char kChromeUIUkmHost[];
extern const char kChromeUIUberHost[];
extern const char kChromeUIUsbInternalsHost[];
extern const char kChromeUIUserActionsHost[];
extern const char kChromeUIVersionHost[];
extern const char kChromeUIVersionURL[];
extern const char kChromeUIWelcomeHost[];
extern const char kChromeUIWelcomeURL[];
extern const char kChromeUIWelcomeWin10Host[];
extern const char kChromeUIWelcomeWin10URL[];
extern const char kDeprecatedChromeUIHistoryFrameHost[];
extern const char kDeprecatedChromeUIHistoryFrameURL[];

#if defined(OS_ANDROID)
extern const char kChromeUIEocInternalsHost[];
extern const char kChromeUIJavaCrashURL[];
extern const char kChromeUINativeBookmarksURL[];
extern const char kChromeUINativeExploreURL[];
extern const char kChromeUINativeHistoryURL[];
extern const char kChromeUINativeNewTabURL[];
extern const char kChromeUINativePhysicalWebDiagnosticsURL[];
extern const char kChromeUINativeScheme[];
extern const char kChromeUIPhysicalWebDiagnosticsHost[];
extern const char kChromeUISnippetsInternalsHost[];
extern const char kChromeUIWebApksHost[];
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessageHost[];
extern const char kChromeUIBluetoothPairingHost[];
extern const char kChromeUIBluetoothPairingURL[];
extern const char kChromeUICertificateManagerDialogURL[];
extern const char kChromeUICertificateManagerHost[];
extern const char kChromeUICryptohomeHost[];
extern const char kChromeUIDeviceEmulatorHost[];
extern const char kChromeUIDiscoverURL[];
extern const char kChromeUIFirstRunHost[];
extern const char kChromeUIFirstRunURL[];
extern const char kChromeUIIntenetConfigDialogURL[];
extern const char kChromeUIIntenetDetailDialogURL[];
extern const char kChromeUIInternetConfigDialogHost[];
extern const char kChromeUIInternetDetailDialogHost[];
extern const char kChromeUILinuxCreditsHost[];
extern const char kChromeUILinuxCreditsURL[];
extern const char kChromeUIMobileSetupHost[];
extern const char kChromeUIMobileSetupURL[];
extern const char kChromeUIMultiDeviceSetupHost[];
extern const char kChromeUIMultiDeviceSetupUrl[];
extern const char kChromeUINetworkHost[];
extern const char kChromeUIOSCreditsHost[];
extern const char kChromeUIOSCreditsURL[];
extern const char kChromeUIOobeHost[];
extern const char kChromeUIOobeURL[];
extern const char kChromeUIPowerHost[];
extern const char kChromeUIScreenlockIconHost[];
extern const char kChromeUIScreenlockIconURL[];
extern const char kChromeUISetTimeHost[];
extern const char kChromeUISetTimeURL[];
extern const char kChromeUISlowHost[];
extern const char kChromeUISlowTraceHost[];
extern const char kChromeUISlowURL[];
extern const char kChromeUISysInternalsHost[];
extern const char kChromeUIUserImageHost[];
extern const char kChromeUIUserImageURL[];
extern const char kChromeUIAssistantOptInHost[];
extern const char kChromeUIAssistantOptInURL[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
extern const char kChromeUIMetroFlowHost[];
extern const char kChromeUIMetroFlowURL[];
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
extern const char kChromeUICastHost[];
extern const char kChromeUICastURL[];
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
extern const char kChromeUIDiscardsHost[];
extern const char kChromeUIDiscardsURL[];
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
extern const char kChromeUILinuxProxyConfigHost[];
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
extern const char kChromeUISandboxHost[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogHost[];
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
extern const char kChromeUIPrintHost[];
#endif

extern const char kChromeUIWebRtcLogsHost[];

// Settings sub-pages.
extern const char kAutofillSubPage[];
extern const char kClearBrowserDataSubPage[];
extern const char kContentSettingsSubPage[];
extern const char kCreateProfileSubPage[];
extern const char kDeprecatedExtensionsSubPage[];
extern const char kHandlerSettingsSubPage[];
extern const char kImportDataSubPage[];
extern const char kLanguageOptionsSubPage[];
extern const char kManageProfileSubPage[];
extern const char kPasswordManagerSubPage[];
extern const char kPaymentsSubPage[];
extern const char kPeopleSubPage[];
extern const char kPrintingSettingsSubPage[];
extern const char kResetProfileSettingsSubPage[];
extern const char kSearchEnginesSubPage[];
extern const char kSignOutSubPage[];
extern const char kSyncSetupSubPage[];
extern const char kTriggeredResetProfileSettingsSubPage[];
#if defined(OS_CHROMEOS)
extern const char kAccessibilitySubPage[];
extern const char kBluetoothSubPage[];
extern const char kDateTimeSubPage[];
extern const char kDisplaySubPage[];
extern const char kHelpSubPage[];
extern const char kInternetSubPage[];
extern const char kConnectedDevicesSubPage[];
extern const char kLockScreenSubPage[];
extern const char kNetworkDetailSubPage[];
extern const char kPowerSubPage[];
extern const char kSmbSharesPage[];
extern const char kSmbSharesPageAddDialog[];
extern const char kStylusSubPage[];
#endif
#if defined(OS_WIN)
extern const char kCleanupSubPage[];
#endif

// Extensions sub pages.
extern const char kExtensionConfigureCommandsSubPage[];

// Gets the hosts/domains that are shown in chrome://chrome-urls.
extern const char* const kChromeHostURLs[];
extern const size_t kNumberOfChromeHostURLs;

// "Debug" pages which are dangerous and not for general consumption.
extern const char* const kChromeDebugURLs[];
extern const size_t kNumberOfChromeDebugURLs;

}  // namespace chrome

#endif  // CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
