// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/constants.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace segmentation_platform {

const char* SegmentationKeyToUmaName(const std::string& segmentation_key) {
  // Please keep in sync with SegmentationKey variant in
  // //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
  // Should also update the field trials allowlist in
  // go/segmentation-field-trials-map.
  if (segmentation_key == kAdaptiveToolbarSegmentationKey) {
    return kAdaptiveToolbarUmaName;
  } else if (segmentation_key == kChromeStartAndroidSegmentationKey) {
    return kChromeStartAndroidUmaName;
  } else if (segmentation_key == kChromeStartAndroidV2SegmentationKey) {
    return kChromeStartAndroidV2UmaName;
  } else if (segmentation_key == kQueryTilesSegmentationKey) {
    return kQueryTilesUmaName;
  } else if (segmentation_key == kChromeLowUserEngagementSegmentationKey) {
    return kChromeLowUserEngagementUmaName;
  } else if (segmentation_key == kFeedUserSegmentationKey) {
    return kFeedUserSegmentUmaName;
  } else if (segmentation_key == kShoppingUserSegmentationKey) {
    return kShoppingUserUmaName;
  } else if (segmentation_key == kContextualPageActionsKey) {
    return kContextualPageActionsUmaName;
  } else if (segmentation_key == kSearchUserKey) {
    return kSearchUserUmaName;
  } else if (segmentation_key == kPowerUserKey) {
    return kPowerUserUmaName;
  } else if (segmentation_key == kCrossDeviceUserKey) {
    return kCrossDeviceUserUmaName;
  } else if (segmentation_key == kFrequentFeatureUserKey) {
    return kFrequentFeatureUserUmaName;
  } else if (segmentation_key == kIntentionalUserKey) {
    return kIntentionalUserUmaName;
  } else if (segmentation_key == kResumeHeavyUserKey) {
    return kResumeHeavyUserUmaName;
  } else if (base::StartsWith(segmentation_key, "test_key")) {
    return "TestKey";
  }
  NOTREACHED();
  return "Unknown";
}

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
    default:
      // This case is reached when UNKNOWN segment is valid, in case of boolean
      // segment results.
      // TODO(crbug.com/1346389): UNKNOWN must be handled separately and add a
      // NOTREACHED() here after fixing tests.
      return "Other";
  }
}

std::string GetSubsegmentKey(const std::string& segmentation_key) {
  return segmentation_key + kSubsegmentDiscreteMappingSuffix;
}

}  // namespace segmentation_platform
