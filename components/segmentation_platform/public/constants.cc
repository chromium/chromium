// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/constants.h"

#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

// Please keep in sync with SegmentationModel variant in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
// Should also update the field trials allowlist in
// go/segmentation-field-trials-map.
std::string SegmentIdToHistogramVariant(proto::SegmentId segment_id) {
  switch (segment_id) {
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      return "NewTab";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      return "Share";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      return "Voice";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY:
      return "Dummy";
    case proto::SegmentId::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID:
      return "ChromeStartAndroid";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES:
      return "QueryTiles";
    case proto::SegmentId::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT:
      return "ChromeLowUserEngagement";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER:
      return "FeedUserSegment";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER:
      return "ShoppingUser";
    case proto::SegmentId::
        OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING:
      return "ContextualPageActionPriceTracking";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER:
      return "SearchUserSegment";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR:
      return "AdaptiveToolbar";
    case proto::SegmentId::POWER_USER_SEGMENT:
      return "PowerUserSegment";
    case proto::SegmentId::CROSS_DEVICE_USER_SEGMENT:
      return "CrossDeviceUser";
    case proto::SegmentId::FREQUENT_FEATURE_USER_SEGMENT:
      return "FrequentFeatureUserSegment";
    case proto::SegmentId::INTENTIONAL_USER_SEGMENT:
      return "IntentionalUser";
    case proto::SegmentId::RESUME_HEAVY_USER_SEGMENT:
      return "ResumeHeavyUserSegment";
    case proto::SegmentId::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2:
      return "ChromeStartAndroidV2";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DEVICE_SWITCHER:
      return "DeviceSwitcher";
    case proto::SegmentId::
        OPTIMIZATION_TARGET_SEGMENTATION_TABLET_PRODUCTIVITY_USER:
      return "TabletProductivityUserSegment";
    case proto::SegmentId::OPTIMIZATION_TARGET_WEB_APP_INSTALLATION_PROMO:
      return "WebAppInstallationPromo";
    case proto::SegmentId::DEVICE_TIER_SEGMENT:
      return "DeviceTierSegment";
    case proto::TAB_RESUMPTION_CLASSIFIER:
      return "TabResumptionClassifier";
    case proto::PASSWORD_MANAGER_USER:
      return kPasswordManagerUserUmaName;
    default:
      // This case is reached when UNKNOWN segment is valid, in case of boolean
      // segment results.
      // TODO(crbug.com/1346389): UNKNOWN must be handled separately and add a
      // NOTREACHED() here after fixing tests.
      return "Other";
  }
}

proto::Predictor::PredictorTypeCase GetClassifierType(
    const std::string& segmentation_key) {
  // Please keep in sync with SegmentationKey variant in
  // //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
  // Should also update the field trials allowlist in
  // go/segmentation-field-trials-map.
  if (segmentation_key == kAdaptiveToolbarSegmentationKey ||
      segmentation_key == kContextualPageActionsKey) {
    return proto::Predictor::kMultiClassClassifier;
  } else if (segmentation_key == kChromeStartAndroidSegmentationKey ||
             segmentation_key == kChromeStartAndroidV2SegmentationKey ||
             segmentation_key == kChromeLowUserEngagementSegmentationKey ||
             segmentation_key == kCrossDeviceUserKey ||
             segmentation_key == kDeviceSwitcherKey ||
             segmentation_key == kFrequentFeatureUserKey ||
             segmentation_key == kIntentionalUserKey ||
             segmentation_key == kResumeHeavyUserKey ||
             segmentation_key == kShoppingUserSegmentationKey ||
             segmentation_key == kQueryTilesSegmentationKey) {
    return proto::Predictor::kBinaryClassifier;
  } else if (segmentation_key == kFeedUserSegmentationKey ||
             segmentation_key == kPowerUserKey ||
             segmentation_key == kSearchUserKey ||
             segmentation_key == kDeviceTierKey ||
             segmentation_key == kTabletProductivityUserKey) {
    return proto::Predictor::kBinnedClassifier;
  }
  // This case is reached when UNKNOWN segment is valid, in case of boolean
  // segment results.
  // TODO(crbug.com/1346389): UNKNOWN must be handled separately and add a
  // NOTREACHED() here after fixing tests.
  return proto::Predictor::kRegressor;
}

std::string GetSubsegmentKey(const std::string& segmentation_key) {
  return segmentation_key + kSubsegmentDiscreteMappingSuffix;
}

}  // namespace segmentation_platform
