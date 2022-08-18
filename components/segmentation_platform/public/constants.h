// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONSTANTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONSTANTS_H_

#include <string>

#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

// The key to be used for adaptive toolbar feature.
const char kAdaptiveToolbarSegmentationKey[] = "adaptive_toolbar";
const char kAdaptiveToolbarUmaName[] = "AdaptiveToolbar";

// The key to be used for any feature that needs to collect and store data on
// client side while being built.
const char kDummySegmentationKey[] = "dummy_feature";
const char kDummyFeatureUmaName[] = "DummyFeature";

// The key is used to decide whether to show Chrome Start or not.
const char kChromeStartAndroidSegmentationKey[] = "chrome_start_android";
const char kChromeStartAndroidUmaName[] = "ChromeStartAndroid";

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

// The key is used to show a contextual page action.
const char kContextualPageActionsKey[] = "contextual_page_actions";
const char kContextualPageActionsUmaName[] = "ContextualPageActions";

// The key provide a list of segment IDs, separated by commas, whose ML model
// execution results are allowed to be uploaded through UKM.
const char kSegmentIdsAllowedForReportingKey[] =
    "segment_ids_allowed_for_reporting";

const char kSubsegmentDiscreteMappingSuffix[] = "_subsegment";

// Returns an UMA display string for the given `segmentation_key`.
const char* SegmentationKeyToUmaName(const std::string& segmentation_key);

// Returns an UMA histogram variant for the given segment_id.
std::string SegmentIdToHistogramVariant(proto::SegmentId segment_id);

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONSTANTS_H_
