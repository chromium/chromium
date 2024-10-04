// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the shared command-line switches used by code in the Chrome
// directory that don't have anywhere more specific to go.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

// Don't add more switch files here. This is linked into some places like the
// installer where dependencies should be limited. Instead, have files
// directly include your switch file.

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in
// media/base/media_switches.cc or ui/gl/gl_switches.cc or one of the
// .cc files corresponding to the *_switches.h files included above
// instead.
//
// Want to remove obsolete switches? Ensure that the switch isn't still in use
// by the Android Java code (ChromeSwitches.java.tmpl) under an aliased name.
// Also perform a string search to make sure the switch isn't in use only by a
// build-configuration, e.g. BUILDFLAG(GOOGLE_CHROME_BRANDING), that is not
// indexed for cross-reference or built by the CQ bots.
// -----------------------------------------------------------------------------

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kAcceptLang[];
extern const char kAllowCrossOriginAuthPrompt[];
extern const char kAllowHttpScreenCapture[];
extern const char kAllowRunningInsecureContent[];
extern const char kAllowSilentPush[];
extern const char kApp[];
extern const char kAppId[];
extern const char kAppLaunchUrlForShortcutsMenuItem[];
extern const char kAppModeAuthCode[];
extern const char kAppModeOAuth2Token[];
extern const char kAppRunOnOsLoginMode[];
extern const char kAppsGalleryDownloadURL[];
extern const char kAppsGalleryUpdateURL[];
extern const char kAppsGalleryURL[];
extern const char kAuthServerAllowlist[];
extern const char kAutoOpenDevToolsForTabs[];
extern const char kAutoSelectDesktopCaptureSource[];
extern const char kAutoSelectTabCaptureSourceByTitle[];
extern const char kAutoSelectWindowCaptureSourceByTitle[];
extern const char kBypassAccountAlreadyUsedByAnotherProfileCheck[];
extern const char kCheckForUpdateIntervalSec[];
extern const char kCipherSuiteBlacklist[];
extern const char kCrashOnHangThreads[];
extern const char kCreateBrowserOnStartupForTests[];
extern const char kCredits[];
extern const char kCustomDevtoolsFrontend[];
extern const char kDebugPackedApps[];
extern const char kDevToolsFlags[];
extern const char kDiagnostics[];
extern const char kDiagnosticsFormat[];
extern const char kDiagnosticsRecovery[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kDisableAutoMaximizeForTests[];
#endif
extern const char kDisableBackgroundNetworking[];
extern const char kDisableClientSidePhishingDetection[];
extern const char kDisableComponentExtensionsWithBackgroundPages[];
extern const char kDisableComponentUpdate[];
extern const char kDisableCrashpadForTesting[];
extern const char kDisableDefaultApps[];
extern const char kDisableDomainReliability[];
extern const char kDisableExtensions[];
extern const char kDisableExtensionsExcept[];
extern const char kDisableLazyLoading[];
extern const char kDisableNaCl[];
extern const char kDisablePrintPreview[];
extern const char kDisablePromptOnRepost[];
extern const char kDisableStackProfiler[];
extern const char kDisableZeroBrowsersOpenForTests[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kDumpBrowserHistograms[];
extern const char kEnableAudioDebugRecordingsFromExtension[];
extern const char kEnableBookmarkUndo[];
extern const char kEnableCloudPrintProxy[];
extern const char kEnableDomainReliability[];
extern const char kEnableDownloadWarningImprovements[];
extern const char kEnableExtensionActivityLogging[];
extern const char kEnableExtensionActivityLogTesting[];
extern const char kEnableUnsafeExtensionDebugging[];
extern const char kEnableHangoutServicesExtensionForTesting[];
extern const char kEnableNaCl[];
extern const char kEnableNetBenchmarking[];
extern const char kEnablePotentiallyAnnoyingSecurityFeatures[];
extern const char kExplicitlyAllowedPorts[];
extern const char kExtensionAiDataCollection[];
extern const char kExtensionContentVerification[];
extern const char kExtensionContentVerificationBootstrap[];
extern const char kExtensionContentVerificationEnforce[];
extern const char kExtensionContentVerificationEnforceStrict[];
extern const char kExtensionsInstallVerification[];
extern const char kExtensionsNotWebstore[];
extern const char kExtensionsToolbarZeroStateVariation[];
extern const char kExtensionsToolbarZeroStateSingleWebStoreLink[];
extern const char kExtensionsToolbarZeroStateExploreExtensionsByCategory[];
extern const char kForceAppMode[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kForceDevToolsAvailable[];
#endif
extern const char kForceFirstRun[];
extern const char kForceWhatsNew[];
extern const char kHideCrashRestoreBubble[];
extern const char kHomePage[];
extern const char kIncognito[];
#if BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kInitialPreferencesFile[];
#endif
extern const char kInitIsolateAsForeground[];
extern const char kInstallAutogeneratedTheme[];
extern const char kInstallChromeApp[];
extern const char kInstallIsolatedWebAppFromFile[];
extern const char kInstallIsolatedWebAppFromUrl[];
extern const char kInstantProcess[];
extern const char kKeepAliveForTest[];
extern const char kKioskMode[];
extern const char kKioskModePrinting[];
extern const char kLaunchInProcessSimpleBrowserSwitch[];
extern const char kLaunchSimpleBrowserSwitch[];
extern const char kMakeDefaultBrowser[];
extern const char kMonitoringDestinationID[];
extern const char kNativeMessagingConnectHost[];
extern const char kNativeMessagingConnectExtension[];
extern const char kNativeMessagingConnectId[];
extern const char kNoDefaultBrowserCheck[];
extern const char kNoExperiments[];
extern const char kNoFirstRun[];
extern const char kNoPings[];
extern const char kNoProxyServer[];
extern const char kNoServiceAutorun[];
extern const char kNoStartupWindow[];
extern const char kOnTheFlyMhtmlHashComputation[];
extern const char kOpenInNewWindow[];
extern const char kPackExtension[];
extern const char kPackExtensionKey[];
extern const char kPreCrashpadCrashTest[];
extern const char kPredictionServiceMockLikelihood[];
extern const char kPreinstalledWebAppsDir[];
extern const char kPrivetIPv6Only[];
extern const char kProductVersion[];
extern const char kProfileDirectory[];
extern const char kIgnoreProfileDirectoryIfNotExists[];
extern const char kProfileEmail[];
extern const char kProxyAutoDetect[];
extern const char kProxyBypassList[];
extern const char kProxyPacUrl[];
extern const char kProxyServer[];
extern const char kRemoteDebuggingTargets[];
extern const char kRestart[];
extern const char kRestoreLastSession[];
extern const char kSavePageAsMHTML[];
extern const char kScreenCaptureAudioDefaultUnchecked[];
extern const char kSilentDebuggerExtensionAPI[];
extern const char kSilentLaunch[];
extern const char kSimulateBrowsingDataLifetime[];
extern const char kSimulateCriticalUpdate[];
extern const char kSimulateElevatedRecovery[];
extern const char kSimulateOutdated[];
extern const char kSimulateOutdatedNoAU[];
extern const char kSimulateUpgrade[];
extern const char kSimulateIdleTimeout[];
extern const char kSSLVersionMax[];
extern const char kSSLVersionMin[];
extern const char kSSLVersionTLSv12[];
extern const char kSSLVersionTLSv13[];
extern const char kStartMaximized[];
extern const char kStartStackProfiler[];
extern const char kStartStackProfilerBrowserTest[];
extern const char kStoragePressureNotificationInterval[];
extern const char kSystemLogUploadFrequency[];
extern const char kThisTabCaptureAutoAccept[];
extern const char kThisTabCaptureAutoReject[];
extern const char kTestMemoryLogDelayInMinutes[];
extern const char kTestName[];
extern const char kTrustedDownloadSources[];
extern const char kUnlimitedStorage[];
extern const char kUnsafelyDisableDevToolsSelfXssWarnings[];
extern const char kUserDataDir[];
extern const char kUseSystemProxyResolver[];
extern const char kValidateCrx[];
extern const char kVersion[];
extern const char kWebRtcRemoteEventLogProactivePruningDelta[];
extern const char kWebRtcRemoteEventLogUploadDelayMs[];
extern const char kWebRtcRemoteEventLogUploadNoSuppression[];
extern const char kWebRtcIPHandlingPolicy[];
extern const char kWindowName[];
extern const char kWindowPosition[];
extern const char kWindowSize[];
extern const char kWindowWorkspace[];
extern const char kWinHttpProxyResolver[];
extern const char kWinJumplistAction[];

#if BUILDFLAG(IS_ANDROID)
extern const char kAuthAndroidNegotiateAccountType[];
extern const char kDisableDefaultBrowserPromo[];
extern const char kForceDeviceOwnership[];
extern const char kForceEnableNightMode[];
extern const char kForceShowUpdateMenuBadge[];
extern const char kForceShowUpdateMenuItemCustomSummary[];
extern const char kForceEnableSigninFRE[];
extern const char kForceDisableSigninFRE[];
extern const char kForceUpdateMenuType[];
extern const char kMarketUrlForTesting[];
extern const char kRequestDesktopSites[];
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kCroshCommand[];
extern const char kDisableLoggingRedirect[];
extern const char kDisableLoginScreenApps[];
extern const char kShortMergeSessionTimeoutForTest[];
extern const char kSchedulerConfiguration[];
extern const char kSchedulerConfigurationConservative[];
extern const char kSchedulerConfigurationPerformance[];
extern const char kSchedulerConfigurationDefault[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kHelp[];
extern const char kHelpShort[];
extern const char kWmClass[];
#endif

#if BUILDFLAG(IS_MAC)
extern const char kAppsKeepChromeAliveInTests[];
extern const char kEnableUserMetrics[];
extern const char kMetricsClientID[];
extern const char kRelauncherProcess[];
extern const char kRelauncherProcessDMGDevice[];
extern const char kMakeChromeDefault[];
extern const char kCodeSignCloneCleanupProcess[];
extern const char kUniqueTempDirSuffix[];
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
extern const char kEnableProfileShortcutManager[];
extern const char kFromBrowserSwitcher[];
extern const char kFromInstaller[];
extern const char kHideIcons[];
extern const char kNoNetworkProfileWarning[];
extern const char kNoPreReadMainDll[];
extern const char kNotificationInlineReply[];
extern const char kNotificationLaunchId[];
extern const char kPrefetchArgumentBrowserBackground[];
extern const char kPwaLauncherVersion[];
extern const char kShowIcons[];
extern const char kSourceShortcut[];
extern const char kUninstall[];
extern const char kUninstallAppId[];
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
extern const char kDebugPrint[];
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kAllowNaClCrxFsAPI[];
extern const char kAllowNaClFileHandleAPI[];
extern const char kAllowNaClSocketAPI[];
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
extern const char kEnableNewAppMenuIcon[];
extern const char kGuest[];
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
extern const char kListApps[];
extern const char kProfileBaseName[];
extern const char kProfileManagementAttributes[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
extern const char kWebApkServerUrl[];
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
extern const char kUseSystemDefaultPrinter[];
#endif

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
extern const char kUserDataMigrated[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
