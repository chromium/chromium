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
#include "chrome/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAppPreloadService);
#endif

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppSpecificNotifications);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kDisableBoostPriority);
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

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kBoardingPassDetector);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kBoardingPassDetectorUrlParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kBoardingPassDetectorUrlParamName[];
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kBorealis);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kBrowserAppInstanceTracking);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
// Enable the Certificate Management UI v2.
//
// TODO(crbug.com/390333881): Remove this flag when launch is complete.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableCertManagementUIV2);
// Enable the Certificate Management UI v2 write features.
//
// TODO(crbug.com/390333881): Remove this flag when launch is complete.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableCertManagementUIV2Write);

// TODO(crbug.com/390333881): Remove this flag when launch is complete.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableCertManagementUIV2EditCerts);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCrostini);
COMPONENT_EXPORT(CHROME_FEATURES)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCrostiniAdvancedAccessControls);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCrostiniAnsibleSoftwareManagement);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCrostiniArcSideload);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCryptohomeDistributedModel);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCryptohomeUserDataAuth);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCryptohomeUserDataAuthKillswitch);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDataLeakPreventionFilesRestriction);
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kDbdRevampDesktop);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPreinstalledWebAppInstallation);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPreinstalledWebAppAlwaysMigrateCalculator);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPreinstalledWebAppAlwaysMigrate);
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

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDisplayEdgeToEdgeFullscreen);
#endif

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kDnsOverHttps);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kDnsOverHttpsTemplatesParam;
COMPONENT_EXPORT(CHROME_FEATURES)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableAmbientAuthenticationInGuestSession);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableAmbientAuthenticationInIncognito);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnterpriseReportingInChromeOS);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEventBasedLogUpload);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kFileTransferEnterpriseConnector);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kFileTransferEnterpriseConnectorUI);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kForcedAppRelaunchOnPlaceholderUpdate);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGeoLanguage);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicActor);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>(
    kGlicActorActorObservationDelay);

#if BUILDFLAG(ENABLE_GLIC)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlic);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicDevelopmentSyncGoogleCookies);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicStatusIconOpenMenuWithSecondaryClick;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicForceSimplifiedBorder);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicPreLoadingTimeMs;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicMinLoadingTimeMs;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicMaxLoadingTimeMs;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicReloadMaxLoadingTimeMs;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicInitialWidth;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicInitialHeight;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicFreInitialWidth;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicFreInitialHeight;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicScreenshotEncodeQuality;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicDefaultHotkey;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicURLConfig);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicGuestURL;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicUserStatusCheck);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicUserStatusUrl;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kGlicUserStatusRequestDelay;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGeminiOAuth2Scope;
COMPONENT_EXPORT(CHROME_FEATURES)
// This is the maximum deviation. The jitter to the delay is a uniformly random
// sample from the chosen deviation. The value should be less than 1.
extern const base::FeatureParam<double> kGlicUserStatusRequestDelayJitter;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicFreURLConfig);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicFreURL;

// TODO(b/414418994): remove features/parameters when URLs are finalized.
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicLearnMoreURLConfig);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicShortcutsLearnMoreURL;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicSettingsPageLearnMoreURL;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicLauncherToggleLearnMoreURL;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicLocationToggleLearnMoreURL;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicTabAccessToggleLearnMoreURL;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicCSPConfig);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicAllowedOriginsOverride;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicClientResponsivenessCheck);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicClientResponsivenessCheckIntervalMs;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicClientResponsivenessCheckTimeoutMs;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicClientUnresponsiveUiMaxTimeMs;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicKeyboardShortcutNewBadge);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicAppMenuNewBadge);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicDetached);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicZOrderChanges);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicDebugWebview);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicScrollTo);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicScrollToEnforceDocumentId;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicSizingFitWindow);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicWarming);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicDisableWarming);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicWarmingDelayMs;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicWarmingJitterMs;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicFreWarming);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicWarmMultiple);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicTieredRollout);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicRollout);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicClosedCaptioning);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicPageContextEligibility);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>
    kGlicPageContextEligibilityAllowNoMetadata;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicUnloadOnClose);

// Causes certain glic API calls to fail or defer when the panel
// is inactive (see ActiveStateCalculator).
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicApiActivationGating);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicGetUserProfileInfoApiActivationGating);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicWebClientUnresponsiveMetrics);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicUseShaderCache);
#endif  // BUILDFLAG(ENABLE_GLIC)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrivacyGuideForceAvailable);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kLinkedServicesSetting);

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
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopSettingsTime;

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
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForHistoryEmbeddings);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForHistoryEmbeddingsDelayTime;

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

#if BUILDFLAG(IS_CHROMEOS)
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

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsFirstBalancedMode);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kHttpsFirstBalancedModeAutoEnable);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kHttpsFirstDialogUi);
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
BASE_DECLARE_FEATURE(kImmersiveFullscreenPWAs);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kFullscreenAnimateTabs);
#endif

#if BUILDFLAG(IS_WIN)
// Only has an effect in branded builds.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIncompatibleApplicationsWarning);
#endif  // BUILDFLAG(IS_ANDROID)

// LINT.IfChange
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kIsolatedWebAppDevMode);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIsolatedWebAppUnmanagedInstall);
// LINT.ThenChange(//PRESUBMIT.py)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIsolatedWebAppManagedGuestSessionInstall);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kIsolatedWebAppBundleCache);
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

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kShowHiddenNetworkToggle);
#endif

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
BASE_DECLARE_FEATURE(kOfflineAutoFetch);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kOnConnectNative);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kOomIntervention);
#endif

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kOverridePrefetchOnSingleton);
#endif

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSkipParentAccessCodeForReauth);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPeriodicLogUploadMigration);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPluginVm);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrerenderFallbackToPreconnect);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrintPreviewCrosPrimary);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPushMessagingBackgroundMode);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kPwaUpdateDialogForIcon);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kQuietNotificationPrompts);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kRecordWebAppDebugInfo);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAbusiveNotificationPermissionRevocation);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRemoveSupervisedUsersOnStartup);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsUwSTrigger);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsNoPrivacyPracticesTrigger);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsOffStoreTrigger);
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHub);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubThreeDotDetails);

// Automatically revoke disruptive notifications
// in Safety Hub.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubDisruptiveNotificationRevocation);

// And integer which tracks the current version of the running experiment for
// disruptive notification revocation. Proposed revocations will be versioned
// and ignored upon version change. This allows to ignore proposed revocations
// from previous experiments in order to consistently revoke and report metrics.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationExperimentVersion;

// Whether the disruptive notification revocation will be performed as a shadow
// run (without actually revoking permissions). Used to collect metrics and
// evaluate the conditions for autorevocation.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>
    kSafetyHubDisruptiveNotificationRevocationShadowRun;

// The minimum number of average daily notifications over last 7 days for a
// website to classify for disruptive notification revocation. Used in a
// combination with
// `kSafetyHubDisruptiveNotificationRevocationMaxEngagementScore`.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMinNotificationCount;

// The maximum site engagement score for a website to classify for disruptive
// notification revocation. Used in a combination with
// `kSafetyHubDisruptiveNotificationRevocationMinNotificationCount`.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kSafetyHubDisruptiveNotificationRevocationMaxEngagementScore;

// The waiting time for a website classified as sending disruptive notifications
// before notification permission is revoked. The website has to satisfy the
// disruptive requirements for this amount of time before the revocation is
// actually enforced.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kSafetyHubDisruptiveNotificationRevocationWaitingTimeAsProposed;

// Timeout in seconds for the Safety Hub OS notification informing users about
// revoked notification permissions.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationNotificationTimeoutSeconds;

// The minimum number of days since the revocation until a site can be
// considered a false positive disruptive notification revocation. The cooldown
// period allows to gather interactions for a period of time to understand how
// much users have interacted with a site and whether it might have been a flake
// (ex. accidental click on a notification).
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMinFalsePositiveCooldown;

// The maximum number of days since the revocation when a site can be considered
// a false positive disruptive notification revocation. After it runs out, the
// revocation won't be reported as a false positive.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationMaxFalsePositivePeriod;

// The minimum site engagement score delta for a website to be considered a
// false positive disruptive notification revocation.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kSafetyHubDisruptiveNotificationRevocationMinSiteEngagementScoreDelta;

// The maximum number of days to observe the revoked site for user regranting
// the permission while visiting the site. The period is a number of days since
// a false positive was detected (a page visit or a notification click).
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationUserRegrantWaitingPeriod;

// The maximum number of days to wait for metrics to be reported for proposed
// disruptive notification revocation. After the period runs out, the permission
// will be revoked. The number is a number of days since a revocation was
// proposed.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kSafetyHubDisruptiveNotificationRevocationWaitingForMetricsDays;

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubMagicStack);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubFollowup);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubAndroidOrganicSurvey);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kSafetyHubAndroidOrganicTriggerId;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubAndroidSurvey);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kSafetyHubAndroidTriggerId;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubAndroidSurveyV2);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubWeakAndReusedPasswords);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubLocalPasswordsModule);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubUnifiedPasswordsModule);
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubServicesOnStartUp);

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
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kProcessPerSiteForDSE);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSkyVault);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSkyVaultV2);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSkyVaultV3);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSmartDim);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kSysInternals);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kTPMFirmwareUpdate);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
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
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySettingsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTrustedSurfaceTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTransactionsTriggerId;
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
    kTrustSafetySentimentSurveyV2SafeBrowsingInterstitialTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyV2TrustedSurfaceTime;
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUseChromiumUpdater);
#endif  // BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppManifestIconUpdating);
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppManifestPolicyAppIdentityUpdate);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRestrictedWebUICodeCache);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kRestrictedWebUICodeCacheResources;

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kUmaStorageDimensions);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUseManagedPrintJobOptionsInPrintPreview);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWin10AcceleratedDefaultBrowserFlow);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
bool IsParentAccessCodeForReauthEnabled();
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSupportsRtcWakeOver24Hours);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDisableShortcutsEnableDiy);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
bool IsK12AgeClassificationMetricsProviderEnabled();
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kK12AgeClassificationMetricsProvider);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kClassManagementEnabledMetricsProvider);
#endif  // BUILDFLAG(IS_CHROMEOS)

bool PrefServiceEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
