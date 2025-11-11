// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/features.h"

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"

namespace segmentation_platform::features {

BASE_FEATURE(kSegmentationPlatformFeature,
             "SegmentationPlatform",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformUkmEngine,

#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformAdaptiveToolbarV2Feature,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformLowEngagementFeature,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShoppingUserSegmentFeature, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformSearchUser, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformDeviceSwitcher,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformFeedSegmentFeature,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kResumeHeavyUserSegmentFeature,
             "ResumeHeavyUserSegment",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformPowerUserFeature,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFrequentFeatureUserSegmentFeature,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualPageActions, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualPageActionTabGrouping,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualPageActionTabGroupThrottling,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool>
    kContextualPageActionTabGroupParamThrottleOnNewTab{
        &kContextualPageActionTabGroupThrottling, "throttle_on_new_tab", false};

const base::FeatureParam<bool>
    kContextualPageActionTabGroupParamShowWhenNotClickedInLastDay{
        &kContextualPageActionTabGroupThrottling,
        "show_when_not_clicked_in_last_day", false};

BASE_FEATURE(kSegmentationDefaultReportingSegments,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformDeviceTier, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformTabletProductivityUser,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformModelExecutionSampling,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformCrossDeviceUser,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformIntentionalUser,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformPasswordManagerUser,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformTabResumptionRanker,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformIosModuleRanker,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformAndroidHomeModuleRanker,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformAndroidHomeModuleRankerV2,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformTimeDelaySampling,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled only on iOS to improve startup performance of the module ranker.
BASE_FEATURE(kSegmentationPlatformSignalDbCache,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformComposePromotion,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformUmaFromSqlDb,
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformIosModuleRankerSplitBySurface,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformURLVisitResumptionRanker,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformEphemeralBottomRank,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

const char kEphemeralCardRankerForceShowCardParam[] =
    "EphemeralCardRankerForceShowCardParam";
const char kEphemeralCardRankerForceHideCardParam[] =
    "EphemeralCardRankerForceHideCardParam";

// Feature flag for enabling the Emphemeral Card ranker.
BASE_FEATURE(kSegmentationPlatformEphemeralCardRanker,
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Feature flag for enabling the Tips Emphemeral Card.
BASE_FEATURE(kSegmentationPlatformTipsEphemeralCard,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

const char kTipsEphemeralCardExperimentTrainParam[] =
    "TipsEphemeralCardExperimentTrainParam";

std::string TipsExperimentTrainEnabled() {
  return base::GetFieldTrialParamByFeatureAsString(
      segmentation_platform::features::kSegmentationPlatformTipsEphemeralCard,
      kTipsEphemeralCardExperimentTrainParam,
      /*default_value=*/
      base::StrCat({kLensEphemeralModuleSearchVariation, ",",
                    kEnhancedSafeBrowsingEphemeralModule}));
}

const char kTipsEphemeralCardModuleMaxImpressionCount[] =
    "TipsEphemeralCardModuleMaxImpressionCount";

int GetTipsEphemeralCardModuleMaxImpressionCount() {
  return base::GetFieldTrialParamByFeatureAsInt(
      segmentation_platform::features::kSegmentationPlatformTipsEphemeralCard,
      kTipsEphemeralCardModuleMaxImpressionCount, /*default_value=*/3);
}

BASE_FEATURE(kSegmentationSurveyPage,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

constexpr base::FeatureParam<bool> kSegmentationSurveyInternalsPage{
    &kSegmentationSurveyPage, "survey_internals_page", /*default_value=*/true};

BASE_FEATURE(kEducationalTipModule, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidAppIntegrationModule, base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kMaxAuxiliarySearchForceShow{
    &kAndroidAppIntegrationModule, "force_card_shown",
    /*default_value=*/false};

constexpr base::FeatureParam<int> kMaxAuxiliarySearchCardImpressions{
    &kAndroidAppIntegrationModule, "max_auxiliary_search_card_impressions",
    /*default_value=*/3};

BASE_FEATURE(kSegmentationPlatformFedCmUser, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserPromoPropensityModel,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppBundlePromoEphemeralCard,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

constexpr base::FeatureParam<int> kMaxAppBundlePromoImpressions{
    &kAppBundlePromoEphemeralCard, "max_app_bundle_promo_impressions",
    /*default_value=*/3};

constexpr base::FeatureParam<int> kMaxAppBundleAppsInstalled{
    &kAppBundlePromoEphemeralCard, "max_app_bundle_apps_installed",
    /*default_value=*/4};

bool IsAppBundlePromoEphemeralCardEnabled() {
  return base::FeatureList::IsEnabled(
      segmentation_platform::features::kAppBundlePromoEphemeralCard);
}

BASE_FEATURE(kDefaultBrowserMagicStackIos,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

constexpr base::FeatureParam<int> kMaxDefaultBrowserMagicStackIosImpressions{
    &kDefaultBrowserMagicStackIos,
    "max_default_browser_magic_stack_ios_impressions",
    /*default_value=*/6};

bool IsDefaultBrowserMagicStackEnabled() {
  return base::FeatureList::IsEnabled(
      segmentation_platform::features::kDefaultBrowserMagicStackIos);
}

BASE_FEATURE(kAndroidTipsNotifications, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kTrustAndSafety{&kAndroidTipsNotifications,
                                                   "trust_and_safety",
                                                   /*default_value=*/false};

constexpr base::FeatureParam<bool> kEssential{&kAndroidTipsNotifications,
                                              "essential",
                                              /*default_value=*/false};

constexpr base::FeatureParam<bool> kNewFeatures{&kAndroidTipsNotifications,
                                                "new_features",
                                                /*default_value=*/false};

constexpr base::FeatureParam<int> kStartTimeMinutes{&kAndroidTipsNotifications,
                                                    "start_time_minutes",
                                                    /*default_value=*/120};

constexpr base::FeatureParam<int> kWindowTimeMinutes{&kAndroidTipsNotifications,
                                                     "window_time_minutes",
                                                     /*default_value=*/120};

}  // namespace segmentation_platform::features
