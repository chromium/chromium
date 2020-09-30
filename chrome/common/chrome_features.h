// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the chrome
// module.

#ifndef CHROME_COMMON_CHROME_FEATURES_H_
#define CHROME_COMMON_CHROME_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "net/net_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(ENABLE_EXTENSIONS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAcknowledgeNtpOverrideOnDeactivate;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAdaptiveScreenBrightnessLogging;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAddToHomescreenMessaging;
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAllowDisableMouseAcceleration;
#endif  // defined(OS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAlwaysReinstallSystemWebApps;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAndroidDarkSearch;
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kApkWebAppInstalls;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppActivityReporting;
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppServiceAdaptiveIcon;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppServiceExternalProtocol;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppServiceIntentHandling;
#endif  // !defined(OS_ANDROID)

#if defined(OS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppShimRemoteCocoa;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppShimNewCloseBehavior;
#endif  // defined(OS_MAC)

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kAsyncDns;

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBackgroundModeAllowRestart;
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kBorealis;
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCertDualVerificationTrialFeature;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kChangePictureVideoMode;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kClearOldBrowsingData;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kClickToOpenPDFPlaceholder;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kClientStorageAccessContextAuditing;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kContentSettingsRedesign;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCpuAffinityRestrictToLittleCores;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrOSEnableUSMUserService;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kCrosCompUpdates;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kCrostini;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAdditionalEnterpriseReporting;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAdvancedAccessControls;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAnsibleInfrastructure;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAnsibleSoftwareManagement;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniArcSideload;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniForceClose;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCryptohomeDistributedModel;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCryptohomeUserDataAuth;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCryptohomeUserDataAuthKillswitch;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDataLeakPreventionPolicy;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDMServerOAuthForChildUser;
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDefaultWebAppInstallation;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopCaptureTabSharingInfobar;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsAppIconShortcutsMenu;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsCacheDuringDefaultInstall;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsLocalUpdating;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsMigrationUserDisplayModeCleanUp;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsRunOnOsLogin;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsSharedStoreService;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsTabStrip;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsTabStripLinkCapturing;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsWithoutExtensions;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDnsOverHttps;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kDnsOverHttpsFallbackParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kDnsOverHttpsShowUiParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kDnsOverHttpsTemplatesParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kDnsOverHttpsDisabledProvidersParam;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDownloadsLocationChange;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableAllSystemWebApps;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableAmbientAuthenticationInGuestSession;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableAmbientAuthenticationInIncognito;

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableEphemeralGuestProfilesOnDesktop;
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MAC)

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableIncognitoShortcutOnDesktop;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnterpriseReportingInChromeOS;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEventBasedStatusReporting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kExternalExtensionDefaultButtonControl;

#if BUILDFLAG(ENABLE_PLUGINS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kFlashDeprecationWarning;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kFlocIdComputedEventLogging;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kFlocIdBlocklistFiltering;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kFocusMode;

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kGdiTextPrinting;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kGeoLanguage;

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktop;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopDemo;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopMigration;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopSettings;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopSettingsPrivacy;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystem;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHeavyAdIntervention;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHeavyAdInterventionWarning;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHeavyAdPrivacyMitigations;

#if defined(OS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kImmersiveFullscreen;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kInSessionPasswordChange;
#endif

#if defined(OS_WIN)
// Only has an effect in branded builds.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIncompatibleApplicationsWarning;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kInstallableAmbientBadgeInfoBar;
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIntentHandlingSharing;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIntentPickerPWAPersistence;
#endif  // !defined(OS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kInvalidatorUniqueOwnerName;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kKernelnextVMs;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLiteVideo;

#if defined(OS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacFullSizeContentView;
#endif

#if defined(OS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacMaterialDesignDownloadShelf;
#endif

#if defined(OS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacSystemMediaPermissionsInfoUi;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacSystemScreenCapturePermissionCheck;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMeteredShowToggle;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMetricsSettingsAndroid;
#endif

#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNativeNotifications;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNoReferrers;

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNotificationDurationLongForRequireInteraction;
#endif

#if defined(OS_POSIX)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNtlmV2Enabled;
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOnConnectNative;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOobeMarketingAdditionalCountriesSupported;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOobeMarketingDoubleOptInCountriesSupported;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOobeMarketingScreen;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOomIntervention;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kParentAccessCodeForOnlineLogin;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPerAppTimeLimits;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPluginVm;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPredictivePrefetchingAllowedOnAllConnectionTypes;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrerenderFallbackToPreconnect;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrivacyElevatedAndroid;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrivacyReorderedAndroid;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrivacySettingsRedesign;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPushMessagingBackgroundMode;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kQuickUnlockFingerprint;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kQuietNotificationPrompts;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAbusiveNotificationPermissionRevocation;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kRemoveSupervisedUsersOnStartup;
#endif

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSafetyCheckAndroid;
#endif

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSafetyCheckChromeCleanerChild;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSchedulerConfiguration;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSCTAuditing;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double> kSCTAuditingSamplingRate;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSecurityKeyAttestationPrompt;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSharesheet;
#endif

#if defined(OS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShow10_10ObsoleteInfobar;
#endif  // defined(OS_MAC)

#if defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShowTrustedPublisherURL;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kSitePerProcess;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kSmartDim;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kSmbFs;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSoundContentSetting;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSysInternals;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTPMFirmwareUpdate;
#endif

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTabMetricsLogging;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTeamfoodFlags;

#if defined(OS_WIN)
// Only has an effect in branded builds.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kThirdPartyModulesBlocking;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kTreatUnsafeDownloadsAsActive;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUploadZippedSystemLogs;
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kUsbbouncer;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kUsbguard;
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUserActivityEventLogging;
#endif

// Android expects this string from Java code, so it is always needed.
// TODO(crbug.com/731802): Use #if BUILDFLAG(ENABLE_VR_BROWSING) instead.
#if BUILDFLAG(ENABLE_VR) || defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kVrBrowsing;
#endif
#if BUILDFLAG(ENABLE_VR)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kVrBrowsingExperimentalFeatures;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kVrBrowsingExperimentalRendering;
#endif  // ENABLE_VR

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebRtcRemoteEventLog;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebRtcRemoteEventLogGzipped;
#endif

#if defined(OS_WIN) || defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kWebShare;
#endif

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebTimeLimits;
#endif  // defined(OS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kWebUIDarkMode;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUmaStorageDimensions;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kWilcoDtc;
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWin10AcceleratedDefaultBrowserFlow;
#endif  // defined(OS_WIN)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWindowNaming;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWriteBasicSystemProfileToPersistentHistogramsFile;

#if defined(OS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
bool IsParentAccessCodeForOnlineLoginEnabled();
#endif  // defined(OS_CHROMEOS)

bool PrefServiceEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
