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
#include "printing/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

enum class ActorPaintStabilityMode {
  // Paint stability tracking is not enabled at all.
  kDisabled,
  // Paint stability tracking is only added to the journal.
  kLogOnly,
  // Paint stability tracking is used as a page stability heuristic.
  kEnabled,
};
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<ActorPaintStabilityMode>
    kActorPaintStabilityMode;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>(
    kActorPaintStabilityIntialPaintTimeout);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>(
    kActorPaintStabilitySubsequentPaintTimeout);

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kAppSpecificNotifications);

// When enabled, invokes `SetProcessPriorityBoost` to disable priority boosting
// when a thread is taken out of the wait state. The default Windows behavior is
// to boost when taking a thread out of waking state. On other platforms, the
// default is not to boost and implementing boosting regresses input and page
// load metrics. Therefore, this experiment on Windows to evaluates if operating
// without boosting improves these metrics. This is a field-sampling experiment
// and is not intended to be shipped as is regardless of the outcome but rather
// to gather data before the design phase of enhanced cross-platform scheduling
// primitives.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDisableBoostPriority);
enum class DisableBoostPriorityMode {
  // In renderer processes: wait until after the first load completes before
  // disabling the boost. In all other processes, disable boost at startup.
  kAfterLoading,
  // Priority boosting is disabled for all processes at startup.
  kAtStartup,
};
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE_PARAM(DisableBoostPriorityMode, kDisableBoostPriorityMode);
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
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUseKeychainKeyProvider);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillAddressSurvey);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillCardSurvey);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kAutofillPasswordSurvey);
#endif

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
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCrostini);
COMPONENT_EXPORT(CHROME_FEATURES)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCrostiniAdvancedAccessControls);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCrostiniAnsibleSoftwareManagement);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCrostiniArcSideload);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCryptographyComplianceCnsa);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCryptohomeDistributedModel);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCryptohomeUserDataAuth);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCryptohomeUserDataAuthKillswitch);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDataLeakPreventionFilesRestriction);
#endif

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

// TODO(crbug.com/419817061): Remove this flag when launch is complete.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCreateProfileIfNoneExists);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kCustomizeTabGroupColorPalette);

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

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kShowResetProfileBannerV2);

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

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kEnableFullscreenToAnyScreenAndroid);
#endif

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
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUnicornChromeActivityReporting);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGeoLanguage);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicActor);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kGlicActorPageToolTimeout;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kGlicActorClickDelay;
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicActorUi);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorUiNudgeRedesign);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorUiTaskIconV2);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorUiTaskNudgeUiFix);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorUiTabIndicatorSpinnerIgnoreReducedMotion);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kActorUiThemed);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicHandoffButtonHiddenClientControl);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicHandoffButtonShowInImmersiveMode);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicHandoffButtonResetFocusAndHoverStatus);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kGlicActorUiTaskIconName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kGlicActorUiOverlayName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kGlicActorUiOverlayMagicCursorName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kGlicActorUiToastName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kGlicActorUiHandoffButtonName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kGlicActorUiTabIndicatorName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kGlicActorUiBorderGlowName[];
COMPONENT_EXPORT(CHROME_FEATURES)
extern const char kGlicActorUiDebounceTimerName[];

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>(kGlicActorUiTaskIcon);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>(kGlicActorUiOverlay);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>(kGlicActorUiOverlayMagicCursor);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>(kGlicActorUiToast);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>(kGlicActorUiHandoffButton);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>(kGlicActorUiTabIndicator);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>(kGlicActorUiBorderGlow);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>(kGlicActorUiStandaloneBorderGlow);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>(kGlicActorUiDebounceTimer);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>(
    kGlicActorPageStabilityTimeout);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kGlicActorPageStabilityMinWait;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>(kActorObservationDelayTimeout);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>(kActorObservationDelayLcp);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorObservationDelayExcludeAdFrameLoading);

// Specifies the default pref value for `glic:prefs::kGlicActuationOnWeb` for
// enterprise users. Does not affect non-enterprise users.
enum class GlicActorEnterprisePrefDefault {
  kEnabledByDefault = 0,
  kDisabledByDefault,
  // When this is set, the browser does not have the capability, regardless of
  // the policy value (the pref value is ignored).
  kForcedDisabled,
};
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<GlicActorEnterprisePrefDefault>(
    kGlicActorEnterprisePrefDefault);

// Exempts the user from ActorPolicyChecker.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicActorPolicyControlExemption;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorIncrementalTyping);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kGlicActorKeyDownDuration;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kGlicActorKeyUpDuration;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kGlicActorIncrementalTypingLongMultiplier;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<size_t>
    kGlicActorIncrementalTypingLongTextThreshold;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kGlicActorTypeToolEnterDelay;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<size_t>
    kGlicActorIncrementalTypingLongTextPasteThreshold;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorPermissionsBypass);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorToctouValidation);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorInternalPopups);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorIterativeInteractionPointDiscovery);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<size_t>
    kGlicActorInterationPointDiscoveryMaxIterations;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActorMoveBeforeClick);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta> kGlicActorMoveBeforeClickDelay;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicActOnWebCapabilityForManagedTrials);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicUnifiedFreScreen);

#if BUILDFLAG(ENABLE_GLIC)
// Controls whether the Glic feature is enabled.
// IMPORTANT: this feature should never be expired! It is used as the main
// kill-switch for Glic and can be used in the future to handle unsupported
// Chrome versions.
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlic);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicDevelopmentSyncGoogleCookies);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicStatusIconOpenMenuWithSecondaryClick;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicForceSimplifiedBorder);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicForceNonSkSLBorder);
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
#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicShowStatusTrayIcon);
#endif
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

enum class GlicEnterpriseCheckStrategy {
  // Use ManagementService to check if the account is managed.
  kPolicy,
  // Use AccountManagedStatusFinder to check if the account is managed.
  kManaged,
};
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<GlicEnterpriseCheckStrategy>
    kGlicUserStatusEnterpriseCheckStrategy;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicUserStatusRefreshApi;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kGlicUserStatusThrottleInterval;

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
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kGlicTabAccessToggleLearnMoreURLDataProtected;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kGlicDefaultTabAccessToggleLearnMoreURL;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kGlicDefaultTabAccessToggleLearnMoreURLDataProtected;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicExtensionsManagementUrl;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kGlicWebActuationToggleLearnMoreURL;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kGlicWebActuationToggleConsiderSafelyURL;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kGlicWebActuationToggleConsiderUnexpectedResultsURL;
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
extern const base::FeatureParam<bool>
    kGlicClientResponsivenessCheckIgnoreWhenDebuggerAttached;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicKeyboardShortcutNewBadge);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicAppMenuNewBadge);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicDetached);
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicMultiInstance);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicSidePanelMinWidth;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicMultiInstanceFloatyWidth;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicMultiInstanceFloatyHeight;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicDefaultToLastActiveConversation);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicEnableMultiInstanceBasedOnTier);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicZOrderChanges);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicDebugWebview);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicScrollTo);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicCaptureRegion);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicUseNonClient);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicScrollToEnforceDocumentId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicScrollToPDF;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicScrollToEnforceURLForPDF;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicWarming);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicGuestContentsVisibilityState);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicWarmingDelayMs;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicWarmingJitterMs;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicFreWarming);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicWarmMultiple);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicTieredRollout);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicRollout);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicIntro);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicLearnMore);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicDefaultTabContextSetting);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicDefaultContextPinOnBind);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicClosedCaptioning);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicUnloadOnClose);

// Causes certain glic API calls to fail or defer when the panel
// is inactive (see ActiveStateCalculator).
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicApiActivationGating);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicBindPinnedUnboundTab);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicExplicitBackgroundColor);

// Features to experiment with resetting the panel default location.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicPanelResetTopChromeButton);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicPanelResetTopChromeButtonDelayMs;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicPanelResetOnStart);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicPanelSetPositionOnDrag);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicPanelResetOnSessionTimeout);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double> kGlicPanelResetOnSessionTimeoutDelayH;
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicPanelResetSizeAndLocationOnOpen);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicPersonalContext);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicGeminiInstructions);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicPopupWindowsEnabled);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicRecordActorJournal);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicRecordActorJournalFeedbackProductId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kGlicRecordActorJournalFeedbackCategoryTag;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicRecordMemoryFootprintMetrics);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicWebClientUnresponsiveMetrics);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicUseShaderCache);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicParameterizedShader);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicParameterizedShaderColors;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicParameterizedShaderFloats;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicTabFocusDataDedupDebounce);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicTabFocusDataDebounceDelayMs;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicTabFocusDataMaxDebounces;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicGetTabByIdApi);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicOpenPasswordManagerSettingsPageApi);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicAssetsV2);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicFaviconDataUrls);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicIgnoreOfflineState);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicMultitabUnderlines);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicWindowDragRegions);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicHandleDraggingNatively);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicCaaGuestError);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicCaaLinkUrl;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicCaaLinkText;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicCaaGuestRedirectPatterns;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicEntrypointVariations);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicEntrypointVariationsShowLabel;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicEntrypointVariationsAltIcon;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kGlicEntrypointVariationsHighlightNudge;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicButtonAltLabel);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kGlicButtonAltLabelVariant;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicDaisyChainNewTabs);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicUseToolbarHeightSidePanel);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicButtonPressedState);

#endif  // BUILDFLAG(ENABLE_GLIC)

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicExtensions);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicHeader);

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicHeaderRequestTypes;

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicShareImage);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicShareImageEnterprise);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGlicActorAutofill);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kGlicActorAutofillFillingTimeout);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kGlicActorAutofillMaximumTimeout);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kActorFormFillingServiceEnableAddress);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kActorFormFillingServiceEnableCreditCard);

COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kGoogleChromeScheme);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicWebActuationSetting);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kGlicWebActuationAllowedTiers;

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kPrivacyGuideForceAvailable);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kGlicMetricsSession);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kGlicMetricsSessionInactivityTimeout;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kGlicMetricsSessionHiddenTimeout;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kGlicMetricsSessionRestartDebounceTimer;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kGlicMetricsSessionStartTimeout;

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
BASE_DECLARE_FEATURE(kHappinessTrackingSurveysForDesktopNextPanel);

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

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kInstantUsesSpareRenderer);
#endif  // !BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsUwSTrigger);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsNoPrivacyPracticesTrigger);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubExtensionsOffStoreTrigger);
#endif

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
BASE_DECLARE_FEATURE(kSafetyHubWeakAndReusedPasswords);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubLocalPasswordsModule);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubUnifiedPasswordsModule);
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSafetyHubTrustSafetySentimentSurvey);
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
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kProcessPerSiteForDSE);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kConsiderDSEWarmUpPageAsSRP);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kCameraCloudStorage);
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

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppManifestIconUpdating);

// Enable the usage of a single icon across the whole web applications system.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppUsePrimaryIcon);

// Periodically query a preinstalled app for updating, with the intention of
// doing this for all preinstalled apps with cheap install_urls (that do not
// redirect etc).
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppPeriodicPreinstallUpdate);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppMigratePreinstalledChat);
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebAppManifestPolicyAppIdentityUpdate);

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebium);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kInitialWebUI);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kInitialWebUIMetrics);
// TODO(crbug.com/444358999): after the experiment to collect metrics, either
// remove this reload button web UI or extend it to include more top-chrome
// components.
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWebUIReloadButton);
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRespectUserAgentOverrideInSearchPrefetch);

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kRestrictedWebUICodeCache);
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kRestrictedWebUICodeCacheResources;

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES) BASE_DECLARE_FEATURE(kUmaStorageDimensions);
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUseManagedPrintJobOptionsInPrintPreview);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kUserValueDefaultBrowserStrings);

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kWinPinPWAShortcutWithLAF);
#endif  // BUILDFLAG(IS_WIN)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kDisableShortcutsEnableDiy);

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kClassManagementEnabledMetricsProvider);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES)
BASE_DECLARE_FEATURE(kSilentPolicyAndDefaultAppUpdating);

bool PrefServiceEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
