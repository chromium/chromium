// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONSTANTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONSTANTS_H_

#include <string>

#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

// The key to be used for adaptive toolbar feature.
const char kAdaptiveToolbarSegmentationKey[] = "adaptive_toolbar";
const char kAdaptiveToolbarUmaName[] = "AdaptiveToolbar";

// The key is used to decide whether to show Chrome Start or not.
const char kChromeStartAndroidSegmentationKey[] = "chrome_start_android";
const char kChromeStartAndroidUmaName[] = "ChromeStartAndroid";

// The key is used to decide how long to wait before showing Chrome Start.
const char kChromeStartAndroidV2SegmentationKey[] = "chrome_start_android_v2";
const char kChromeStartAndroidV2UmaName[] = "ChromeStartAndroidV2";

// The key is used to decide whether to show query tiles.
const char kQueryTilesSegmentationKey[] = "query_tiles";
const char kQueryTilesUmaName[] = "QueryTiles";

// The key is used to decide whether a user has low user engagement with chrome.
// This is a generic model that can be used by multiple features targeting
// low-engaged users. Typically low engaged users are active in chrome below a
// certain threshold number of days over a time period. This is computed using
// Session.TotalDuration histogram.
const char kChromeLowUserEngagementSegmentationKey[] =
    "chrome_low_user_engagement";
const char kChromeLowUserEngagementUmaName[] = "ChromeLowUserEngagement";

// The key is used to decide whether the user likes to use Feed.
const char kFeedUserSegmentationKey[] = "feed_user_segment";
const char kFeedUserSegmentUmaName[] = "FeedUserSegment";

// The key is used to decide whether the user is interested in shopping or not.
const char kShoppingUserSegmentationKey[] = "shopping_user";
const char kShoppingUserUmaName[] = "ShoppingUser";

// The key is used to show a contextual page action.
const char kContextualPageActionsKey[] = "contextual_page_actions";
const char kContextualPageActionsUmaName[] = "ContextualPageActions";

// Determine search users of the browser app.
const char kSearchUserKey[] = "search_user";
const char kSearchUserUmaName[] = "SearchUser";

// Determine power users of the browser app.
const char kPowerUserKey[] = "power_user";
const char kPowerUserUmaName[] = "PowerUser";

// The key is used to decide whether the user is active on multiple synced
// devices.
const char kCrossDeviceUserKey[] = "cross_device_user";
const char kCrossDeviceUserUmaName[] = "CrossDeviceUser";

// Keys related to users of chrome features.
const char kFrequentFeatureUserKey[] = "frequent_feature_user";
const char kFrequentFeatureUserUmaName[] = "FrequentFeatureUser";

// This key is used to decide whether the user uses Chrome intentionally.
const char kIntentionalUserKey[] = "intentional_user";
const char kIntentionalUserUmaName[] = "IntentionalUser";

// Key for user segment that is more likely to use resume features in chrome.
const char kResumeHeavyUserKey[] = "resume_heavy_user";
const char kResumeHeavyUserUmaName[] = "ResumeHeavyUser";

// Key for user segment that is likely switched from Chrome on other devices.
const char kDeviceSwitcherKey[] = "device_switcher";
const char kDeviceSwitcherUmaName[] = "DeviceSwitcher";

// The key is used to decide whether the user is categorised as tablet
// productivity or not.
const char kTabletProductivityUserKey[] = "tablet_productivity_user";
const char kTabletProductivityUserUmaName[] = "TabletProductivityUser";

// Key for segment that tells in which tier the device used by the user belongs.
const char kDeviceTierKey[] = "device_tier";
const char kDeviceTierUmaName[] = "DeviceTier";

// The key provide a list of segment IDs, separated by commas, whose ML model
// execution results are allowed to be uploaded through UKM.
const char kSegmentIdsAllowedForReportingKey[] =
    "segment_ids_allowed_for_reporting";

// Config parameter name specified in experiment configs. Any experiment config
// or feature can include this param and segmentation will enable the config for
// storing cached results.
const char kSegmentationConfigParamName[] =
    "segmentation_platform_add_config_param";

// Parameter names used for defining segmentation config.
// TODO(ssid): These should be deprecated and clients should use
// `kSegmentationConfigParamName` to define config instead.
constexpr char kDefaultModelEnabledParam[] = "enable_default_model";
constexpr char kVariationsParamNameSegmentSelectionTTLDays[] =
    "segment_selection_ttl_days";
constexpr char kVariationsParamNameUnknownSelectionTTLDays[] =
    "unknown_selection_ttl_days";

const char kSubsegmentDiscreteMappingSuffix[] = "_subsegment";

// Returns an UMA display string for the given `segmentation_key`.
const char* SegmentationKeyToUmaName(const std::string& segmentation_key);

// Returns an UMA histogram variant for the given segment_id.
std::string SegmentIdToHistogramVariant(proto::SegmentId segment_id);

// Returns Subsegment key for the given `segmentation_key`.
std::string GetSubsegmentKey(const std::string& segmentation_key);

// Returns PredictorType for the given `segmentation_key`
proto::Predictor::PredictorTypeCase GetClassifierType(
    const std::string& segmentation_key);

// TODO(shaktisahu): Move these to a nicer location.

// Labels for adaptive toolbar model.
const char kAdaptiveToolbarModelLabelNewTab[] = "NewTab";
const char kAdaptiveToolbarModelLabelShare[] = "Share";
const char kAdaptiveToolbarModelLabelVoice[] = "Voice";

// Labels for contextual page actions model.
const char kContextualPageActionModelLabelPriceTracking[] = "price_tracking";
const char kContextualPageActionModelLabelReaderMode[] = "reader_mode";

// Labels for search user model.
// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
const char kSearchUserModelLabelNone[] = "None";
const char kSearchUserModelLabelLow[] = "Low";
const char kSearchUserModelLabelMedium[] = "Medium";
const char kSearchUserModelLabelHigh[] = "High";

// Custom inputs for contextual page actions model.
const char kContextualPageActionModelInputPriceTracking[] = "can_track_price";
const char kContextualPageActionModelInputReaderMode[] = "has_reader_mode";

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONSTANTS_H_
