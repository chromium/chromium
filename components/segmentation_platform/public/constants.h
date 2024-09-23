// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONSTANTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONSTANTS_H_

#include <string>

#include "components/segmentation_platform/public/proto/output_config.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

// SEGMENTATION_CLIENT_KEYS_BEGIN

// The key to be used for adaptive toolbar feature.
const char kAdaptiveToolbarSegmentationKey[] = "adaptive_toolbar";
const char kAdaptiveToolbarUmaName[] = "AdaptiveToolbar";

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

// The key is used to decide whether the user should receive a web app
// installation promotion.
const char kWebAppInstallationPromoKey[] = "web_app_installation_promo";
const char kWebAppInstallationPromoUmaName[] = "WebAppInstallationPromo";

const char kPasswordManagerUserKey[] = "password_manager_user";
const char kPasswordManagerUserUmaName[] = "PasswordManagerUser";

// Key for segment that tells in which tier the device used by the user belongs.
const char kDeviceTierKey[] = "device_tier";
const char kDeviceTierUmaName[] = "DeviceTier";

const char kTabResumptionClassifierKey[] = "tab_resupmtion_classifier";
const char kTabResumptionClassifierUmaName[] = "TabResumptionClassifier";

// The key is used to rank `URLVisitAggregate` objects leveraged by UI tab
// resumption features.
const char kURLVisitResumptionRankerKey[] = "url_visit_resumption_ranker";
const char kURLVisitResumptionRankerUmaName[] = "URLVisitResumptionRanker";

const char kIosModuleRankerKey[] = "ios_module_ranker";
const char kIosModuleRankerUmaName[] = "IosModuleRanker";

const char kAndroidHomeModuleRankerKey[] = "android_home_module_ranker";
const char kAndroidHomeModuleRankerUmaName[] = "AndroidHomeModuleRanker";

// This key is used to decide what modules a user should see on their Desktop
// New Tab Page.
const char kDesktopNtpModuleKey[] = "desktop_ntp_module";
const char kDesktopNtpModuleUmaName[] = "DesktopNtpModule";

const char kOptimizationTargetSegmentationDummyKey[] = "segmentation_dummy";
const char kOptimizationTargetSegmentationDummyUmaName[] = "SegmentationDummy";

const char kComposePromotionKey[] = "compose_promotion";
const char kComposePromotionUmaName[] = "ComposePromotion";

const char kEphemeralHomeModuleBackendKey[] = "ephemeral_home_module_backend";

// SEGMENTATION_CLIENT_KEYS_END

// Please keep the UMA names for keys in sync with SegmentationKey variant in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
// Should also update the field trials allowlist in
// go/segmentation-field-trials-map.

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

// Returns an UMA histogram variant for the given segment_id.
// TODO(ssid): Move this to stats.cc since, no need to be in public.
std::string SegmentIdToHistogramVariant(proto::SegmentId segment_id);

// Returns Subsegment key for the given `segmentation_key`.
std::string GetSubsegmentKey(const std::string& segmentation_key);

// TODO(shaktisahu): Move these to a nicer location.

// Legacy label used for users not in the segment for binary user segment
// classifier.
const char kLegacyNegativeLabel[] = "Other";

// Labels for adaptive toolbar model.
const char kAdaptiveToolbarModelLabelNewTab[] = "NewTab";
const char kAdaptiveToolbarModelLabelShare[] = "Share";
const char kAdaptiveToolbarModelLabelVoice[] = "Voice";
const char kAdaptiveToolbarModelLabelTranslate[] = "Translate";
const char kAdaptiveToolbarModelLabelAddToBookmarks[] = "AddToBookmarks";
const char kAdaptiveToolbarModelLabelReadAloud[] = "ReadAloud";

// Labels for contextual page actions model.
const char kContextualPageActionModelLabelDiscounts[] = "discounts";
const char kContextualPageActionModelLabelPriceTracking[] = "price_tracking";
const char kContextualPageActionModelLabelReaderMode[] = "reader_mode";
const char kContextualPageActionModelLabelPriceInsights[] = "price_insights";

// Labels for cross device segment.
const char kNoCrossDeviceUsage[] = "NoCrossDeviceUsage";
const char kCrossDeviceMobile[] = "CrossDeviceMobile";
const char kCrossDeviceDesktop[] = "CrossDeviceDesktop";
const char kCrossDeviceTablet[] = "CrossDeviceTablet";
const char kCrossDeviceMobileAndDesktop[] = "CrossDeviceMobileAndDesktop";
const char kCrossDeviceMobileAndTablet[] = "CrossDeviceMobileAndTablet";
const char kCrossDeviceDesktopAndTablet[] = "CrossDeviceDesktopAndTablet";
const char kCrossDeviceAllDeviceTypes[] = "CrossDeviceAllDeviceTypes";
const char kCrossDeviceOther[] = "CrossDeviceOther";

// Labels for search user model.
// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
const char kSearchUserModelLabelNone[] = "None";
const char kSearchUserModelLabelLow[] = "Low";
const char kSearchUserModelLabelMedium[] = "Medium";
const char kSearchUserModelLabelHigh[] = "High";

// Labels for device tier model.
// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
const char kDeviceTierSegmentLabelNone[] = "None";
const char kDeviceTierSegmentLabelLow[] = "Low";
const char kDeviceTierSegmentLabelMedium[] = "Medium";
const char kDeviceTierSegmentLabelHigh[] = "High";

// Labels for tablet productivity user model.
// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
const char kTabletProductivityUserModelLabelNone[] = "None";
const char kTabletProductivityUserModelLabelMedium[] = "Medium";
const char kTabletProductivityUserModelLabelHigh[] = "High";

// Labels for Android Home modules for ranking.
const char kSingleTab[] = "SingleTab";
const char kPriceChange[] = "PriceChange";
const char kTabResumptionForAndroidHome[] = "TabResumption";
const char kSafetyHub[] = "SafetyHub";

// Input Context keys for freshness for Android Home modules.
const char kSingleTabFreshness[] = "single_tab_freshness";
const char kPriceChangeFreshness[] = "price_change_freshness";
const char kTabResumptionForAndroidHomeFreshness[] = "tab_resumption_freshness";
const char kSafetyHubFreshness[] = "safety_hub_freshness";

// Labels for IOS modules for ranking.
const char kMostVisitedTiles[] = "MostVisitedTiles";
const char kShortcuts[] = "Shortcuts";
const char kSafetyCheck[] = "SafetyCheck";
const char kTabResumption[] = "TabResumption";
const char kParcelTracking[] = "ParcelTracking";
const char kPriceTrackingPromo[] = "PriceTrackingPromo";

// Input Context keys for freshness for IOS modules.
const char kMostVisitedTilesFreshness[] = "most_visited_tiles_freshness";
const char kShortcutsFreshness[] = "shortcuts_freshness";
const char kSafetyCheckFreshness[] = "safety_check_freshness";
const char kTabResumptionFreshness[] = "tab_resumption_freshness";
const char kParcelTrackingFreshness[] = "parcel_tracking_freshness";
const char kIsShowingStartSurface[] = "is_showing_start_surface";

// Labels for desktop new tab page drive module model.
// Any updates to these strings need to also update the field trials allowlist
// in go/segmentation-field-trials-map.
const char kDesktopNtpDriveModuleLabelShow[] = "show_drive";
const char kDesktopNtpDriveModuleLabelDontShow[] = "dont_show_drive";

// Custom inputs for contextual page actions model.
const char kContextualPageActionModelInputPriceTracking[] = "can_track_price";
const char kContextualPageActionModelInputReaderMode[] = "has_reader_mode";
const char kContextualPageActionModelInputPriceInsights[] =
    "has_price_insights";
const char kContextualPageActionModelInputDiscounts[] = "has_discounts";

const char kComposePrmotionLabelShow[] = "Show";
const char kComposePrmotionLabelDontShow[] = "DontShow";

// Finch parameter key for sampling rate of the model execution results.
constexpr char kModelExecutionSamplingRateKey[] =
    "model_execution_sampling_rate";

// Finch parameter key for introducing delay(in ms) in model initialization at
// startup.
constexpr char kModelInitializationDelay[] = "model_initialization_delay";

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONSTANTS_H_
