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
             "SegmentationPlatformUkmEngine",

#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformUserVisibleTaskRunner,
             "SegmentationPlatformUserVisibleTaskRunner",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformAdaptiveToolbarV2Feature,
             "SegmentationPlatformAdaptiveToolbarV2Feature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformLowEngagementFeature,
             "SegmentationPlatformLowEngagementFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShoppingUserSegmentFeature,
             "ShoppingUserSegmentFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformSearchUser,
             "SegmentationPlatformSearchUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformDeviceSwitcher,
             "SegmentationPlatformDeviceSwitcher",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformFeedSegmentFeature,
             "SegmentationPlatformFeedSegmentFeature",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kResumeHeavyUserSegmentFeature,
             "ResumeHeavyUserSegment",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformPowerUserFeature,
             "SegmentationPlatformPowerUserFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFrequentFeatureUserSegmentFeature,
             "FrequentFeatureUserSegmentFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualPageActions,
             "ContextualPageActions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualPageActionShareModel,
             "ContextualPageActionShareModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationDefaultReportingSegments,
             "SegmentationDefaultReportingSegments",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformDeviceTier,
             "SegmentationPlatformDeviceTier",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformTabletProductivityUser,
             "SegmentationPlatformTabletProductivityUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformModelExecutionSampling,
             "SegmentationPlatformModelExecutionSampling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformCrossDeviceUser,
             "SegmentationPlatformCrossDeviceUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformIntentionalUser,
             "SegmentationPlatformIntentionalUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformPasswordManagerUser,
             "SegmentationPlatformPasswordManagerUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformTabResumptionRanker,
             "SegmentationPlatformTabResumptionRanker",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformIosModuleRanker,
             "SegmentationPlatformIosModuleRanker",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformAndroidHomeModuleRanker,
             "SegmentationPlatformAndroidHomeModuleRanker",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformAndroidHomeModuleRankerV2,
             "SegmentationPlatformAndroidHomeModuleRankerV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformTimeDelaySampling,
             "SegmentationPlatformTimeDelaySampling",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformCollectTabRankData,
             "SegmentationPlatformCollectTabRankData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformModelInitializationDelay,
             "SegmentationPlatformModelInitializationDelay",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled only on iOS to improve startup performance of the module ranker.
BASE_FEATURE(kSegmentationPlatformSignalDbCache,
             "SegmentationPlatformSignalDbCache",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformComposePromotion,
             "SegmentationPlatformComposePromotion",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformUmaFromSqlDb,
             "SegmentationPlatformUmaFromSqlDb",
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformIosModuleRankerSplitBySurface,
             "SegmentationPlatformIosModuleRankerSplitBySurface",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformURLVisitResumptionRanker,
             "SegmentationPlatformURLVisitResumptionRanker",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformEphemeralBottomRank,
             "SegmentationPlatformEphemeralBottomRank",
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
             "SegmentationPlatformEphemeralCardRanker",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Feature flag for enabling the Tips Emphemeral Card.
BASE_FEATURE(kSegmentationPlatformTipsEphemeralCard,
             "SegmentationPlatformTipsEphemeralCard",
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
             "SegmentationSurveyPage",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

constexpr base::FeatureParam<bool> kSegmentationSurveyInternalsPage{
    &kSegmentationSurveyPage, "survey_internals_page", /*default_value=*/true};

BASE_FEATURE(kEducationalTipModule,
             "EducationalTipModule",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kMaxDefaultBrowserCardImpressions{
    &kEducationalTipModule, "max_default_browser_card_impressions",
    /*default_value=*/3};
constexpr base::FeatureParam<int> kMaxTabGroupCardImpressions{
    &kEducationalTipModule, "max_tab_group_card_impressions",
    /*default_value=*/10};
constexpr base::FeatureParam<int> kMaxTabGroupSyncCardImpressions{
    &kEducationalTipModule, "max_tab_group_sync_card_impressions",
    /*default_value=*/10};
constexpr base::FeatureParam<int> kMaxQuickDeleteCardImpressions{
    &kEducationalTipModule, "max_quick_delete_card_impressions",
    /*default_value=*/10};
constexpr base::FeatureParam<int> kMaxHistorySyncCardImpressions{
    &kEducationalTipModule, "max_history_sync_card_impressions",
    /*default_value=*/10};

constexpr base::FeatureParam<int> KDaysToShowEphemeralCardOnce{
    &kEducationalTipModule, "days_to_show_ephemeral_card_once",
    /*default_value=*/1};

constexpr base::FeatureParam<int> KDaysToShowEachEphemeralCardOnce{
    &kEducationalTipModule, "days_to_show_each_ephemeral_card_once",
    /*default_value=*/7};

constexpr base::FeatureParam<std::string> KNamesOfEphemeralCardsToShow{
    &kEducationalTipModule, "names_of_ephemeral_cards_to_show",
    /*default_value=*/""};

BASE_FEATURE(kAndroidAppIntegrationModule,
             "AndroidAppIntegrationModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kMaxAuxiliarySearchForceShow{
    &kAndroidAppIntegrationModule, "force_card_shown",
    /*default_value=*/false};

constexpr base::FeatureParam<int> kMaxAuxiliarySearchCardImpressions{
    &kAndroidAppIntegrationModule, "max_auxiliary_search_card_impressions",
    /*default_value=*/3};

BASE_FEATURE(kSegmentationPlatformFedCmUser,
             "SegmentationPlatformFedCmUser",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace segmentation_platform::features
