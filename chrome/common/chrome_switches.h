// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the shared command-line switches used by code in the Chrome
// directory that don't have anywhere more specific to go.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_

#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/ui_features.h"

// Don't add more switch files here. This is linked into some places like the
// installer where dependencies should be limited. Instead, have files
// directly include your switch file.

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in
// media/base/media_switches.cc or ui/gl/gl_switches.cc or one of the
// .cc files corresponding to the *_switches.h files included above
// instead.
// -----------------------------------------------------------------------------

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kAllowCrossOriginAuthPrompt[];
extern const char kAllowHttpScreenCapture[];
extern const char kAllowOutdatedPlugins[];
extern const char kAllowRunningInsecureContent[];
extern const char kAllowSilentPush[];
extern const char kApp[];
extern const char kAppId[];
extern const char kAppModeAuthCode[];
extern const char kAppModeOAuth2Token[];
extern const char kAppsGalleryDownloadURL[];
extern const char kAppsGalleryUpdateURL[];
extern const char kAppsGalleryURL[];
extern const char kAuthExtensionPath[];
extern const char kAuthServerWhitelist[];
extern const char kAutoOpenDevToolsForTabs[];
extern const char kAutoSelectDesktopCaptureSource[];
extern const char kBypassAppBannerEngagementChecks[];
extern const char kCheckForUpdateIntervalSec[];
extern const char kCipherSuiteBlacklist[];
extern const char kCloudPrintFile[];
extern const char kCloudPrintFileType[];
extern const char kCloudPrintJobTitle[];
extern const char kCloudPrintPrintTicket[];
extern const char kCloudPrintServiceProcess[];
extern const char kCloudPrintSetupProxy[];
extern const char kCrashOnHangThreads[];
extern const char kCreateBrowserOnStartupForTests[];
extern const char kCustomDevtoolsFrontend[];
extern const char kDebugEnableFrameToggle[];
extern const char kDebugPackedApps[];
extern const char kDenyPermissionPrompts[];
extern const char kDevToolsFlags[];
extern const char kDiagnostics[];
extern const char kDiagnosticsFormat[];
extern const char kDiagnosticsRecovery[];
extern const char kDisableBackgroundNetworking[];
extern const char kDisableBundledPpapiFlash[];
extern const char kDisableCastStreamingHWEncoding[];
extern const char kDisableClientSidePhishingDetection[];
extern const char kDisableComponentExtensionsWithBackgroundPages[];
extern const char kDisableComponentUpdate[];
extern const char kDisableDefaultApps[];
extern const char kDisableDeviceDiscoveryNotifications[];
extern const char kDisableDomainReliability[];
extern const char kDisableExtensions[];
extern const char kDisableExtensionsExcept[];
extern const char kDisableExtensionsFileAccessCheck[];
extern const char kDisableOfflineAutoReload[];
extern const char kDisableOfflineAutoReloadVisibleOnly[];
extern const char kDisablePopupBlocking[];
extern const char kDisablePrintPreview[];
extern const char kDisablePromptOnRepost[];
extern const char kDisableSearchGeolocationDisclosure[];
extern const char kDisableZeroBrowsersOpenForTests[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kDnsLogDetails[];
extern const char kDumpBrowserHistograms[];
extern const char kEasyUnlockAppPath[];
extern const char kEnableAudioDebugRecordingsFromExtension[];
extern const char kEnableBookmarkUndo[];
extern const char kEnableCloudPrintProxy[];
extern const char kEnableDeviceDiscoveryNotifications[];
extern const char kEnableDevToolsExperiments[];
extern const char kEnableDomainReliability[];
extern const char kEnableExtensionActivityLogging[];
extern const char kEnableExtensionActivityLogTesting[];
extern const char kEnableFastUnload[];
extern const char kEnableNaCl[];
extern const char kEnableNavigationTracing[];
extern const char kEnableNetBenchmarking[];
extern const char kEnableOfflineAutoReload[];
extern const char kEnableOfflineAutoReloadVisibleOnly[];
extern const char kEnablePotentiallyAnnoyingSecurityFeatures[];
extern const char kEnablePowerOverlay[];
extern const char kEnablePrintPreviewRegisterPromos[];
extern const char kEnableUiDevTools[];
extern const char kExtensionContentVerification[];
extern const char kExtensionContentVerificationBootstrap[];
extern const char kExtensionContentVerificationEnforce[];
extern const char kExtensionContentVerificationEnforceStrict[];
extern const char kExtensionsInstallVerification[];
extern const char kExtensionsNotWebstore[];
extern const char kFastStart[];
extern const char kForceAndroidAppMode[];
extern const char kForceAppMode[];
extern const char kForceDesktopIOSPromotion[];
extern const char kForceFirstRun[];
extern const char kForceFirstRunDialog[];
extern const char kForceLocalNtp[];
extern const char kForceStackedTabStripLayout[];
extern const char kHomePage[];
extern const char kIgnoreUrlFetcherCertRequests[];
extern const char kIncognito[];
extern const char kInstallChromeApp[];
extern const char kInstallSupervisedUserWhitelists[];
extern const char kInstantProcess[];
extern const char kKeepAliveForTest[];
extern const char kKioskMode[];
extern const char kKioskModePrinting[];
extern const char kLaunchInProcessSimpleBrowserSwitch[];
extern const char kLaunchSimpleBrowserSwitch[];
extern const char kLoadMediaRouterComponentExtension[];
extern const char kMakeDefaultBrowser[];
extern const char kMediaCacheSize[];
extern const char kMonitoringDestinationID[];
extern const char kNetLogCaptureMode[];
extern const char kNewNetErrorPageUI[];
extern const char kNewTabButtonPosition[];
extern const char kNewTabButtonPositionOppositeCaption[];
extern const char kNewTabButtonPositionLeading[];
extern const char kNewTabButtonPositionAfterTabs[];
extern const char kNewTabButtonPositionTrailing[];
extern const char kNoDefaultBrowserCheck[];
extern const char kNoExperiments[];
extern const char kNoFirstRun[];
extern const char kNoPings[];
extern const char kNoProxyServer[];
extern const char kNoServiceAutorun[];
extern const char kNoStartupWindow[];
extern const char kNoSupervisedUserAcknowledgmentCheck[];
extern const char kOpenInNewWindow[];
extern const char kOriginalProcessStartTime[];
extern const char kOriginTrialDisabledFeatures[];
extern const char kOriginTrialDisabledTokens[];
extern const char kOriginTrialPublicKey[];
extern const char kPackExtension[];
extern const char kPackExtensionKey[];
extern const char kParentProfile[];
extern const char kPermissionRequestApiScope[];
extern const char kPermissionRequestApiUrl[];
extern const char kPpapiFlashPath[];
extern const char kPpapiFlashVersion[];
extern const char kPrivetIPv6Only[];
extern const char kProductVersion[];
extern const char kProfileDirectory[];
extern const char kProfilingAtStart[];
extern const char kProfilingFlush[];
extern const char kProxyAutoDetect[];
extern const char kProxyBypassList[];
extern const char kProxyPacUrl[];
extern const char kRemoteDebuggingTargets[];
extern const char kRestoreLastSession[];
extern const char kSavePageAsMHTML[];
extern const char kShowAppList[];
extern const char kSilentDebuggerExtensionAPI[];
extern const char kSilentLaunch[];
extern const char kSimulateCriticalUpdate[];
extern const char kSimulateElevatedRecovery[];
extern const char kSimulateOutdated[];
extern const char kSimulateOutdatedNoAU[];
extern const char kSimulateUpgrade[];
extern const char kSSLVersionMax[];
extern const char kSSLVersionMin[];
extern const char kSSLVersionTLSv1[];
extern const char kSSLVersionTLSv11[];
extern const char kSSLVersionTLSv12[];
extern const char kSSLVersionTLSv13[];
extern const char kStartMaximized[];
extern const char kStartStackProfiler[];
extern const char kSupervisedUserId[];
extern const char kSystemLogUploadFrequency[];
extern const char kTaskManagerShowExtraRenderers[];
extern const char kTestName[];
extern const char kTLS13Variant[];
extern const char kTLS13VariantDisabled[];
extern const char kTLS13VariantDraft23[];
extern const char kTLS13VariantFinal[];
extern const char kTrustedDownloadSources[];
extern const char kTryChromeAgain[];
extern const char kUnlimitedStorage[];
extern const char kUnsafelyTreatInsecureOriginAsSecure[];
extern const char kUnsafePacUrl[];
extern const char kUserAgent[];
extern const char kUserDataDir[];
extern const char kValidateCrx[];
extern const char kVersion[];
extern const char kWebRtcRemoteEventLogProactivePruningDelta[];
extern const char kWebRtcRemoteEventLogUploadDelayMs[];
extern const char kWebRtcRemoteEventLogUploadNoSuppression[];
extern const char kWindowPosition[];
extern const char kWindowSize[];
extern const char kWindowWorkspace[];
extern const char kWinHttpProxyResolver[];
extern const char kWinJumplistAction[];

#if defined(GOOGLE_CHROME_BUILD)
extern const char kEnableGoogleBrandedContextMenu[];
#endif  // defined(GOOGLE_CHROME_BUILD)

#if !defined(GOOGLE_CHROME_BUILD)
extern const char kLocalNtpReload[];
#endif

#if defined(OS_ANDROID)
extern const char kAuthAndroidNegotiateAccountType[];
extern const char kChromeHomeSwipeLogicType[];
extern const char kDisableContextualSearch[];
extern const char kEnableAccessibilityTabSwitcher[];
extern const char kEnableContextualSearch[];
extern const char kEnableHungRendererInfoBar[];
extern const char kForceShowUpdateMenuBadge[];
extern const char kForceShowUpdateMenuItemCustomSummary[];
extern const char kForceUpdateMenuType[];
extern const char kMarketUrlForTesting[];
extern const char kProgressBarAnimation[];
extern const char kTrustedCDNBaseURLForTests[];
extern const char kWebApkServerUrl[];
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
extern const char kCroshCommand[];
extern const char kDisableLoggingRedirect[];
extern const char kDisableLoginScreenApps[];
extern const char kMashServiceName[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
extern const char kHelp[];
extern const char kHelpShort[];
extern const char kPasswordStore[];
extern const char kEnableEncryptionSelection[];
extern const char kWmClass[];
#endif

#if defined(OS_MACOSX)
extern const char kAppsKeepChromeAliveInTests[];
extern const char kDisableAppInfoDialogMac[];
extern const char kDisableFullscreenTabDetaching[];
extern const char kDisableHostedAppShimCreation[];
extern const char kDisableHostedAppsInWindows[];
extern const char kDisableMacViewsNativeAppWindows[];
extern const char kEnableAppInfoDialogMac[];
extern const char kEnableFullscreenTabDetaching[];
extern const char kEnableFullscreenToolbarReveal[];
extern const char kEnableHostedAppsInWindows[];
extern const char kEnableUserMetrics[];
extern const char kHostedAppQuitNotification[];
extern const char kMetricsClientID[];
extern const char kRelauncherProcess[];
extern const char kRelauncherProcessDMGDevice[];
extern const char kMakeChromeDefault[];
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
extern const char kEnableCloudPrintXps[];
extern const char kEnableProfileShortcutManager[];
extern const char kHideIcons[];
extern const char kNoNetworkProfileWarning[];
extern const char kNotificationInlineReply[];
extern const char kNotificationLaunchId[];
extern const char kPrefetchArgumentBrowserBackground[];
extern const char kPrefetchArgumentWatcher[];
extern const char kShowIcons[];
extern const char kUninstall[];
extern const char kWatcherProcess[];
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
extern const char kDebugPrint[];
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kAllowNaClCrxFsAPI[];
extern const char kAllowNaClFileHandleAPI[];
extern const char kAllowNaClSocketAPI[];
#endif

#if defined(OS_WIN) || defined(OS_LINUX)
extern const char kDisableInputImeAPI[];
extern const char kEnableInputImeAPI[];
#endif

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
extern const char kEnableNewAppMenuIcon[];
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && \
    !defined(GOOGLE_CHROME_BUILD)
extern const char kEnableMachineLevelUserCloudPolicy[];
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
extern const char kUseSystemDefaultPrinter[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
