// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the chrome
// module.

#ifndef CHROME_COMMON_CHROME_FEATURES_H_
#define CHROME_COMMON_CHROME_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kActivityReportingSessionType);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAdaptiveScreenBrightnessLogging);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAppPreloadService);
#endif

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppSpecificNotifications);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAppShimRemoteCocoa);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppShimNewCloseBehavior);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppShimLaunchChromeSilently);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppShimNotificationAttribution);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUseAdHocSigningForWebAppShims);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillAddressSurvey);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillCardSurvey);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillPasswordSurvey);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kBackgroundModeAllowRestart);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kQuickOfficeForceFileDownload);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kBoardingPassDetector);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kBoardingPassDetectorUrlParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kBoardingPassDetectorUrlParamName[];
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kBorealis);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kBrowserAppInstanceTracking);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kChangePictureVideoMode);

// Enable the Certificate Management UI v2.
//
// TODO(crbug.com/40928765) Remove this flag when UI is complete.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableCertManagementUIV2);
// Enable the Certificate Management UI v2 write features.
//
// TODO(crbug.com/40928765) Remove this flag when UI is complete.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableCertManagementUIV2Write);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCrOSEnableUSMUserService);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCrosCompUpdates);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCrostini);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCrostiniAdditionalEnterpriseReporting);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCrostiniAdvancedAccessControls);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCrostiniAnsibleInfrastructure);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCrostiniAnsibleSoftwareManagement);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCrostiniArcSideload);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCryptohomeDistributedModel);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCryptohomeUserDataAuth);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCryptohomeUserDataAuthKillswitch);
#endif

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCrosPrivacyHub);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDataLeakPreventionPolicy);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDataLeakPreventionFilesRestriction);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDMServerOAuthForChildUser);
#endif

#if !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPreinstalledWebAppInstallation);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPreinstalledWebAppDuplicationFixer);
#endif

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kDesktopTaskManagerEndProcessDisabledForExtension);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kChromeStructuredMetrics);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsElidedExtensionsMenu);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsFlashAppNameInsteadOfOrigin);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsIconHealthChecks);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPwaNavigationCapturingWithScopeExtensions);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsRunOnOsLogin);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsPreventClose);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsTabStripSettings);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kChromeAppsDeprecation);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kShortcutsNotApps);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kShortcutsNotAppsRevealDesktop);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDisruptiveNotificationPermissionRevocation);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kDnsOverHttps);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kDnsOverHttpsTemplatesParam;
COMPONENT_EXPORT(CHROME_FEATURES)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableAmbientAuthenticationInGuestSession);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableAmbientAuthenticationInIncognito);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableWebUsbOnExtensionServiceWorker);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kExtensionDeferredIndividualSettings);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnterpriseReportingInChromeOS);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEventBasedLogUpload);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kFileTransferEnterpriseConnector);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kFileTransferEnterpriseConnectorUI);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kFlashDeprecationWarning);
#endif

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kForcedAppRelaunchOnPlaceholderUpdate);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGeoLanguage);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrivacyGuideForceAvailable);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kLinkedServicesSetting);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCbdTimeframeRequired);
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopDemo);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysConfiguration);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kHappinessTrackingSurveysHostedUrl;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopSettings);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopSettingsPrivacy);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyNoGuide;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopPrivacyGuide);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacyGuideTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopNtpModules);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForNtpPhotosOptOut);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForWallpaperSearch);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopWhatsNew);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopWhatsNewTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kHappinessTrackingSurveysForDesktopWhatsNewEnActivationPercentage;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kHappinessTrackingSurveysForDesktopWhatsNewDeActivationPercentage;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kHappinessTrackingSurveysForDesktopWhatsNewJpActivationPercentage;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopM1AdPrivacyPage);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopM1AdPrivacyPageTime;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopM1TopicsSubpage);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopM1TopicsSubpageTime;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopM1FledgeSubpage);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopM1FledgeSubpageTime;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopM1AdMeasurementSubpage);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopM1AdMeasurementSubpageTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHaTSDesktopDevToolsIssuesCOEP);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHaTSDesktopDevToolsIssuesMixedContent);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(
    kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHaTSDesktopDevToolsIssuesHeavyAd);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHaTSDesktopDevToolsIssuesCSP);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForSecurityPage);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForSecurityPageTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kHappinessTrackingSurveysForSecurityPageTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>
    kHappinessTrackingSurveysForSecurityPageRequireInteraction;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystem);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemBluetoothRevamp);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemBatteryLife);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemPeripherals);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemEnt);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemStability);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemPerformance);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemOnboarding);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemArcGames);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemAudio);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemAudioOutputProc);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemBluetoothAudio);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingPersonalizationAvatar);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingPersonalizationScreensaver);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingPersonalizationWallpaper);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingMediaAppPdf);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemCameraApp);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingPhotosExperience);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingGeneralCamera);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingGeneralCameraPrioritized);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingPrivacyHubPostLaunch);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingOsSettingsSearch);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingBorealisGames);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingLauncherAppsFinding);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingLauncherAppsNeeding);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingOffice);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHideWebAppOriginText);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsFirstBalancedMode);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstBalancedModeAutoEnable);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsFirstDialogUi);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeInterstitialAugust2024Refresh);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeForAdvancedProtectionUsers);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeV2ForEngagedSites);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeV2ForTypicallySecureUsers);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsUpgrades);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeIncognito);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeIncognitoNewSettings);

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kImmersiveFullscreen);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kImmersiveFullscreenTabs);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kImmersiveFullscreenPWAs);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kFullscreenAnimateTabs);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kInSessionPasswordChange);
#endif

#if BUILDFLAG(IS_WIN)
// Only has an effect in branded builds.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIncompatibleApplicationsWarning);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIsolatedWebAppAutomaticUpdates);
#endif

// LINT.IfChange
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kIsolatedWebAppDevMode);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIsolatedWebAppUnmanagedInstall);
// LINT.ThenChange(//PRESUBMIT.py)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIsolatedWebAppManagedGuestSessionInstall);
#endif

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kKioskEnableSystemWebApps);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kKAnonymityService);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kKAnonymityServiceAuthServer;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kKAnonymityServiceJoinRelayServer;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kKAnonymityServiceJoinServer;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kKAnonymityServiceJoinInterval;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kKAnonymityServiceQueryServer;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kKAnonymityServiceQueryRelayServer;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kKAnonymityServiceQueryInterval;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kKAnonymityServiceOHTTPRequests);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kKAnonymityServiceStorage);

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kLinuxLowMemoryMonitor);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kLinuxLowMemoryMonitorModerateLevel;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kLinuxLowMemoryMonitorCriticalLevel;
#endif  // BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kListWebAppsSwitch);
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kMacSystemScreenCapturePermissionCheck);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kMeteredShowToggle);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kShowHiddenNetworkToggle);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kMoveWebApp);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kMoveWebAppUninstallStartUrlPrefix;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kMoveWebAppUninstallStartUrlPattern;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kMoveWebAppInstallStartUrl;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kNativeNotifications);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSystemNotifications);

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kNewMacNotificationAPI);
#endif

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kNewFilesPolicyUX);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kNoReferrers);

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kNotificationDurationLongForRequireInteraction);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kOfflineContentOnNetError);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kOfflineAutoFetch);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kOnConnectNative);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kOomIntervention);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPageContentOptIn);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kParentAccessCodeForOnlineLogin);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPeriodicLogUploadMigration);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPermissionAuditing);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPluginVm);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrerenderFallbackToPreconnect);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrintPreviewCrosPrimary);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrivacyGuideAndroid3);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrivacyGuidePreloadAndroid);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPushMessagingBackgroundMode);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPwaUpdateDialogForIcon);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kQuietNotificationPrompts);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kRecordWebAppDebugInfo);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAbusiveNotificationPermissionRevocation);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRemoveSupervisedUsersOnStartup);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyCheckExtensions);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsUwSTrigger);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsNoPrivacyPracticesTrigger);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsOffStoreTrigger);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHub);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubMagicStack);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubFollowup);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubAndroidSurvey);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kSafetyHubAndroidTriggerId;
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubTrustSafetySentimentSurvey);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubHaTSOneOffSurvey);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kHatsSurveyTriggerSafetyHubOneOffExperimentControlTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kHatsSurveyTriggerSafetyHubOneOffExperimentNotificationTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kHatsSurveyTriggerSafetyHubOneOffExperimentInteractionTriggerId;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kBackgroundPasswordCheckInterval;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kPasswordCheckOverdueInterval;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kPasswordCheckMonWeight;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kPasswordCheckTueWeight;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kPasswordCheckWedWeight;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kPasswordCheckThuWeight;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kPasswordCheckFriWeight;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kPasswordCheckSatWeight;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kPasswordCheckSunWeight;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsMinEnagementLimit;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsLowEnagementLimit;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kPasswordCheckNotificationIntervalName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kRevokedPermissionsNotificationIntervalName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kNotificationPermissionsNotificationIntervalName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kSafeBrowsingNotificationIntervalName[];

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kPasswordCheckNotificationInterval;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kRevokedPermissionsNotificationInterval;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kNotificationPermissionsNotificationInterval;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kSafeBrowsingNotificationInterval;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSchedulerConfiguration);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSCTAuditing);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double> kSCTAuditingSamplingRate;
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSCTAuditingHashdance);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kSCTLogExpectedIngestionDelay;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kSCTLogMaxIngestionRandomDelay;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSitePerProcess);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kProcessPerSiteSkipDevtoolsUsers);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kProcessPerSiteSkipEnterpriseUsers);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSkyVault);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSkyVaultV2);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSmartDim);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSoundContentSetting);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSysInternals);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kTPMFirmwareUpdate);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSupportTool);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSupportToolScreenshot);
#endif

#if BUILDFLAG(IS_WIN)
// Only has an effect in branded builds.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kThirdPartyModulesBlocking);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kTreatUnsafeDownloadsAsActive);

// TrustSafetySentimentSurvey
#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kTrustSafetySentimentSurvey);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMinTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMaxTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyNtpVisitsMinRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyNtpVisitsMaxRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySettingsProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTrustedSurfaceProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTransactionsProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox4ConsentAcceptProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox4ConsentDeclineProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox4NoticeOkProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox4NoticeSettingsProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySettingsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTrustedSurfaceTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTransactionsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox4ConsentAcceptTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox4ConsentDeclineTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox4NoticeOkTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox4NoticeSettingsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyPrivacySettingsTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTrustedSurfaceTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTransactionsPasswordManagerTime;
#endif

// TrustSafetySentimentSurveyV2
#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kTrustSafetySentimentSurveyV2);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MinTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MaxTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyV2NtpVisitsMinRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyV2NtpVisitsMaxRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2MinSessionTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2BrowsingDataProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2ControlGroupProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2DownloadWarningUIProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PasswordCheckProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PasswordProtectionUIProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyCheckProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyHubNotificationProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyHubInteractionProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2TrustedSurfaceProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacyGuideProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentAcceptProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentDeclineProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeOkProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeSettingsProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2BrowsingDataTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2ControlGroupTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2DownloadWarningUITriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordCheckTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordProtectionUITriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyCheckTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyHubInteractionTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyHubNotificationTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacyGuideTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentAcceptTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacySandbox4ConsentDeclineTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeOkTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PrivacySandbox4NoticeSettingsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTime;
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUseChromiumUpdater);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUserActivityEventLogging);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUserTypeByDeviceTypeMetricsProvider);
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppDontAddExistingAppsToSync);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppManifestIconUpdating);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppSyncGeneratedIconBackgroundFix);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppSyncGeneratedIconRetroactiveFix);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppSyncGeneratedIconUpdateFix);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppUniversalInstall);
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppManifestPolicyAppIdentityUpdate);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kWebRtcRemoteEventLog);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebRtcRemoteEventLogGzipped);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kWebShare);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRestrictedWebUICodeCache);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kRestrictedWebUICodeCacheResources;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kUmaStorageDimensions);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWin10AcceleratedDefaultBrowserFlow);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUtilWinProcessUsesUiPump);
#endif  // BUILDFLAG(IS_WIN)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWriteBasicSystemProfileToPersistentHistogramsFile);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
bool IsParentAccessCodeForOnlineLoginEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSupportsRtcWakeOver24Hours);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool PrefServiceEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
