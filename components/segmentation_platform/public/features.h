// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Feature flags for the segmentation platform. Don't remove these feature flags.
namespace segmentation_platform::features {

// Core feature flag for segmentation platform.
BASE_DECLARE_FEATURE(kSegmentationPlatformFeature);

// Feature flag for enabling UKM based engine.
BASE_DECLARE_FEATURE(kSegmentationPlatformUkmEngine);

// Feature flag for enabling adaptive toolbar v2 multi-output model.
BASE_DECLARE_FEATURE(kSegmentationPlatformAdaptiveToolbarV2Feature);

// Feature flag for enabling low engagement segmentation key.
BASE_DECLARE_FEATURE(kSegmentationPlatformLowEngagementFeature);

// Feature flag for enabling Feed user segments feature.
BASE_DECLARE_FEATURE(kSegmentationPlatformFeedSegmentFeature);

// Feature flag for enabling categorization into resume heavy user.
BASE_DECLARE_FEATURE(kResumeHeavyUserSegmentFeature);

// Feature flag for enabling Power user segmentation.
BASE_DECLARE_FEATURE(kSegmentationPlatformPowerUserFeature);

// Feature flag for enabling frequent feature user segment.
BASE_DECLARE_FEATURE(kFrequentFeatureUserSegmentFeature);

// Feature flag for enabling contextual page actions. Do not remove this, as all
// segmentation platform powered functionalities must be behind a base::Feature.
BASE_DECLARE_FEATURE(kContextualPageActions);

// Feature flag for enabling search user segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformSearchUser);

// Feature flag for device switcher segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformDeviceSwitcher);

// Feature flag for enabling tab grouping action feature.
BASE_DECLARE_FEATURE(kContextualPageActionTabGrouping);

// Feature flag for enabling tab group throttling.
BASE_DECLARE_FEATURE(kContextualPageActionTabGroupThrottling);

extern const base::FeatureParam<bool>
    kContextualPageActionTabGroupParamThrottleOnNewTab;

extern const base::FeatureParam<bool>
    kContextualPageActionTabGroupParamShowWhenNotClickedInLastDay;

// Feature flag for enabling shopping user segment feature.
BASE_DECLARE_FEATURE(kShoppingUserSegmentFeature);

// Feature flag for enabling default reporting segments.
BASE_DECLARE_FEATURE(kSegmentationDefaultReportingSegments);

// Feature flag for enabling device tier segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformDeviceTier);

// Feature flag for enabling tablet productivity user segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformTabletProductivityUser);

// Feature flag for enabling model execution report sampling.
BASE_DECLARE_FEATURE(kSegmentationPlatformModelExecutionSampling);

// Feature flag for enabling cross device user segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformCrossDeviceUser);

// Feature flag for enabling intentional user segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformIntentionalUser);

// Feature flag for enabling password manager user segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformPasswordManagerUser);

// Feature flag for enabling tab resumption ranker.
BASE_DECLARE_FEATURE(kSegmentationPlatformTabResumptionRanker);

// Feature flag for enabling ios module ranker.
BASE_DECLARE_FEATURE(kSegmentationPlatformIosModuleRanker);

// Feature flag for enabling android home module ranker.
BASE_DECLARE_FEATURE(kSegmentationPlatformAndroidHomeModuleRanker);

// Feature flag for enabling on-demand service for ranking android
// home modules.
BASE_DECLARE_FEATURE(kSegmentationPlatformAndroidHomeModuleRankerV2);

// Feature flag for controlling sampling of training data collection.
BASE_DECLARE_FEATURE(kSegmentationPlatformTimeDelaySampling);

// Feature flag for turning of signal database cache.
BASE_DECLARE_FEATURE(kSegmentationPlatformSignalDbCache);

// Feature flag for Compose promotion targeting.
BASE_DECLARE_FEATURE(kSegmentationPlatformComposePromotion);

// Feature flag for using SQL database for UMA signals.
BASE_DECLARE_FEATURE(kSegmentationPlatformUmaFromSqlDb);

// Feature flag for having separate models for the Start and NTP surface.
BASE_DECLARE_FEATURE(kSegmentationPlatformIosModuleRankerSplitBySurface);

// Feature flag for enabling the URL visit resumption ranker.
BASE_DECLARE_FEATURE(kSegmentationPlatformURLVisitResumptionRanker);

// Feature flag for enabling the URL visit resumption ranker.
BASE_DECLARE_FEATURE(kSegmentationPlatformEphemeralBottomRank);

extern const char kEphemeralCardRankerForceShowCardParam[];
extern const char kEphemeralCardRankerForceHideCardParam[];

// Feature flag for enabling the Ephemeral Card ranker.
BASE_DECLARE_FEATURE(kSegmentationPlatformEphemeralCardRanker);

BASE_DECLARE_FEATURE(kSegmentationSurveyPage);
extern const base::FeatureParam<bool> kSegmentationSurveyInternalsPage;

// The maximum number of times the auxiliary search promo card can be visible to
// the user.
BASE_DECLARE_FEATURE(kAndroidAppIntegrationModule);
extern const base::FeatureParam<bool> kMaxAuxiliarySearchForceShow;
extern const base::FeatureParam<int> kMaxAuxiliarySearchCardImpressions;

// Feature flag for enabling FedCM user segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformFedCmUser);

// Feature flag enabling checking a propensity model before showing a default
// browser promo.
BASE_DECLARE_FEATURE(kDefaultBrowserPromoPropensityModel);

// Feature flag for enabling the App Bundle Promo Ephemeral card in the Magic
// Stack.
BASE_DECLARE_FEATURE(kAppBundlePromoEphemeralCard);
// The maximum number of impressions for the `AppBundlePromoEphemeralModule`
// Magic Stack card before the card should be hidden.
extern const base::FeatureParam<int> kMaxAppBundlePromoImpressions;
// The maximum number of app bundle apps that a user can have installed on their
// device to have the card be shown.
extern const base::FeatureParam<int> kMaxAppBundleAppsInstalled;

// Whether the App Bundle promo module should be shown in the Magic Stack.
bool IsAppBundlePromoEphemeralCardEnabled();

// Feature flag to enable the ephemeral Default Browser card in the Magic Stack
// on iOS.
BASE_DECLARE_FEATURE(kDefaultBrowserMagicStackIos);
// The maximum number impressions for `kDefaultBrowserMagicStackIos` before the
// card should be hidden.
extern const base::FeatureParam<int> kMaxDefaultBrowserMagicStackIosImpressions;

// Feature flag for enabling the tips notifications ranker.
BASE_DECLARE_FEATURE(kAndroidTipsNotifications);

// Feature flag for enabling the tips notifications ranker V2.
BASE_DECLARE_FEATURE(kAndroidTipsNotificationsV2);

// The prioritization of tips notifications based on trust and safety.
extern const base::FeatureParam<bool> kTrustAndSafety;
// The prioritization of tips notifications based on essential features.
extern const base::FeatureParam<bool> kEssential;
// The prioritization of tips notifications based on new features.
extern const base::FeatureParam<bool> kNewFeatures;
// The start time in minutes for scheduling the notification.
extern const base::FeatureParam<int> kStartTimeMinutes;
// The window time in minutes for the elapsed time since the start time.
extern const base::FeatureParam<int> kWindowTimeMinutes;
// The control param for the Enhanced Safe Browsing tip.
extern const base::FeatureParam<bool> kEnableEnhancedSafeBrowsingTip;
// The control param for the Quick Delete tip.
extern const base::FeatureParam<bool> kEnableQuickDeleteTip;
// The control param for the Google Lens tip.
extern const base::FeatureParam<bool> kEnableGoogleLensTip;
// The control param for the Bottom Omnibox tip.
extern const base::FeatureParam<bool> kEnableBottomOmniboxTip;

}  // namespace segmentation_platform::features

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FEATURES_H_
