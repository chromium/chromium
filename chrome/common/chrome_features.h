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
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
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
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppDeduplicationService);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppDeduplicationServiceFondue);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppManagementAppDetails);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAppPreloadService);
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAppShimRemoteCocoa);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppShimNewCloseBehavior);
#endif  // BUILDFLAG(IS_MAC)

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAsyncDns);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillAddressSurvey);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillCardSurvey);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillPasswordSurvey);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kBackgroundModeAllowRestart);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kBorealis);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kBrowserAppInstanceTracking);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kChangePictureVideoMode);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kClientStorageAccessContextAuditing);

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
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPreinstalledWebAppWindowExperiment);
// Finch-controlled user group for the experiment.
// Used in metrics. Do not renumber or reuse values.
enum class PreinstalledWebAppWindowExperimentUserGroup : int32_t {
  // Default. Experiment is not running.
  kUnknown = 0,
  // User assigned to have the default behaviour.
  kControl = 1,
  // User assigned to have preinstalled web apps open in windows with link
  // capturing.
  kWindow = 2,
  // User assigned to have preinstalled web apps open in browser tabs without
  // link capturing.
  kTab = 3
};
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kOsIntegrationSubManagers);
enum class OsIntegrationSubManagersStage {
  kWriteConfig,
  kExecuteAndWriteConfig,
};
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<OsIntegrationSubManagersStage>
    kOsIntegrationSubManagersStageParam;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPWAsDefaultOfflinePage);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsAdditionalWindowingControls);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsCacheDuringDefaultInstall);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsElidedExtensionsMenu);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsEnforceWebAppSettingsPolicy);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsFlashAppNameInsteadOfOrigin);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsIconHealthChecks);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsRunOnOsLogin);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsPreventClose);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsKeepAlive);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDesktopPWAsTabStripSettings);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kDesktopPWAsWebBundles);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kChromeAppsDeprecation);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kKeepForceInstalledPreinstalledApps);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kChromeAppsDeprecationHideLaunchAnyways;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDisruptiveNotificationPermissionRevocation);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kDnsOverHttps);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kDnsOverHttpsFallbackParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kDnsOverHttpsShowUiParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kDnsOverHttpsTemplatesParam;
COMPONENT_EXPORT(CHROME_FEATURES)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kEarlyLibraryLoad);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableAmbientAuthenticationInGuestSession);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableAmbientAuthenticationInIncognito);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableRestrictedWebApis);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableWebHidOnExtensionServiceWorker);
#endif

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

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kExternalExtensionDefaultButtonControl);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kFileTransferEnterpriseConnector);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kFlashDeprecationWarning);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kFocusMode);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGeoLanguage);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopDemo);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopPrivacySandbox);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacySandboxTime;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopSettings);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopSettingsPrivacy);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyNoSandbox;
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
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopWhatsNew);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopWhatsNewTime;

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
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystem);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemBluetoothRevamp);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemBatteryLife);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemEnt);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemStability);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemPerformance);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemOnboarding);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemUnlock);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemSmartLock);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemArcGames);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingSystemAudio);

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
BASE_DECLARE_FEATURE(kHappinessTrackingPrivacyHubBaseline);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHappinessTrackingOsSettingsSearch);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHideWebAppOriginText);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsOnlyMode);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstModeForAdvancedProtectionUsers);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsFirstModeV2);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsUpgrades);

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kImmersiveFullscreen);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kImmersiveFullscreenTabs);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kImmersiveFullscreenPWAs);
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

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIncognitoDownloadsWarning);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kIncognitoNtpRevamp);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kIsolatedWebAppDevMode);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kKioskEnableAppService);
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

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kMetricsSettingsAndroid);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kMigrateExternalPrefsToWebAppDB);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kMoveWebApp);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kMoveWebAppUninstallStartUrlPrefix;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kMoveWebAppUninstallStartUrlPattern;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kMoveWebAppInstallStartUrl;

#if BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kNativeNotifications);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSystemNotifications);
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kNewMacNotificationAPI);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kNoReferrers);

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kNotificationDurationLongForRequireInteraction);
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kOnConnectNative);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kOobeMarketingAdditionalCountriesSupported);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kOobeMarketingDoubleOptInCountriesSupported);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kOomIntervention);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kParentAccessCodeForOnlineLogin);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPermissionAuditing);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPermissionPredictions);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double> kPermissionPredictionsHoldbackChance;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPermissionGeolocationPredictions);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kPermissionGeolocationPredictionsHoldbackChance;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPluginVm);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrerenderFallbackToPreconnect);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPrivacyGuideAndroid);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPushMessagingBackgroundMode);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPwaUpdateDialogForIcon);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPwaUpdateDialogForName);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kQuietNotificationPrompts);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kRecordWebAppDebugInfo);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAbusiveNotificationPermissionRevocation);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRemoveSupervisedUsersOnStartup);
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyCheckNotificationPermissions);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsMinEnagementLimit;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsLowEnagementLimit;
#endif

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

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSecurityKeyAttestationPrompt);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSitePerProcess);

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

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSupportToolCopyTokenButton);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSupportToolScreenshot);
#endif

#if BUILDFLAG(IS_WIN)
// Only has an effect in branded builds.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kThirdPartyModulesBlocking);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kTreatUnsafeDownloadsAsActive);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kBlockInsecureDownloads);

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
    kTrustSafetySentimentSurveyPrivacySandbox3ConsentAcceptProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3ConsentDeclineProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeDismissProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeOkProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeSettingsProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeLearnMoreProbability;
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
    kTrustSafetySentimentSurveyPrivacySandbox3ConsentAcceptTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3ConsentDeclineTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeDismissTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeOkTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeSettingsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySandbox3NoticeLearnMoreTriggerId;
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
    kTrustSafetySentimentSurveyV2PasswordCheckProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyV2SafetyCheckProbability;
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
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2BrowsingDataTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2ControlGroupTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2PasswordCheckTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyV2SafetyCheckTriggerId;
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

// Android expects this string from Java code, so it is always needed.
// TODO(crbug.com/731802): Use #if BUILDFLAG(ENABLE_VR_BROWSING) instead.
#if BUILDFLAG(ENABLE_VR) || BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kVrBrowsing);
#endif
#if BUILDFLAG(ENABLE_VR)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kVrBrowsingExperimentalFeatures);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kVrBrowsingExperimentalRendering);
#endif  // ENABLE_VR

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppManifestIconUpdating);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppManifestImmediateUpdating);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppSyncGeneratedIconBackgroundFix);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppSyncGeneratedIconRetroactiveFix);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppSyncGeneratedIconUpdateFix);
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppManifestPolicyAppIdentityUpdate);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kWebAppsCrosapi);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kChromeKioskEnableLacros);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kWebKioskEnableLacros);
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kWebRtcRemoteEventLog);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebRtcRemoteEventLogGzipped);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kWebShare);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kWebUIDarkMode);

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kUmaStorageDimensions);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kWilcoDtc);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWin10AcceleratedDefaultBrowserFlow);
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

// This flag is used to toggle the reading of data from the web_app DB instead
// of the ExternallyInstalledWebAppPrefs. Data will be written to both storages,
// and this will be removed in the future once we move to using the web_app DB
// completely.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUseWebAppDBInsteadOfExternalPrefs);

// When enabled, use authentication through a browser tab, instead of
// an app window.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAuthFlowInBrowserTab);
enum class WebAuthFlowInBrowserTabMode {
  // Auth flow is presented in a new tab attached to a new/existing browser.
  kNewTab,
  // Auth flow is presented in a browser popup window.
  kPopupWindow
};
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<WebAuthFlowInBrowserTabMode>
    kWebAuthFlowInBrowserTabMode;

bool PrefServiceEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
