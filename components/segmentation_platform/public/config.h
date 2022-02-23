// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_

#include <string>

#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace segmentation_platform {

// The key to be used for adaptive toolbar feature.
const char kAdaptiveToolbarSegmentationKey[] = "adaptive_toolbar";

// The key to be used for any feature that needs to collect and store data on
// client side while being built.
const char kDummySegmentationKey[] = "dummy_feature";

// The key is used to decide whether to show Chrome Start or not.
const char kChromeStartAndroidSegmentationKey[] = "chrome_start_android";

// The key is used to decide whether to show query tiles.
const char kQueryTilesSegmentationKey[] = "query_tiles";

// The key is used to decide whether a user has low user engagement with chrome.
// This is a generic model that can be used by multiple features targeting
// low-engaged users. Typically low engaged users are active in chrome below a
// certain threshold number of days over a time period. This is computed using
// Session.TotalDuration histogram.
const char kChromeLowUserEngagementSegmentationKey[] =
    "chrome_low_user_engagement";

// The key provide a list of segment IDs, separated by commas, whose ML model
// execution results are allowed to be uploaded through UKM.
const char kSegmentIdsAllowedForReportingKey[] =
    "segment_ids_allowed_for_reporting";

// Contains various finch configuration params used by the segmentation
// platform.
struct Config {
  Config();
  ~Config();

  Config(const Config& other);
  Config& operator=(const Config& other);

  // The key is used to distinguish between different types of segmentation
  // usages. Currently it is mainly used by the segment selector to find the
  // discrete mapping and writing results to prefs.
  std::string segmentation_key;

  // Time to live for a segment selection. Segment selection can't be changed
  // before this duration.
  base::TimeDelta segment_selection_ttl;

  // Time to live for an unknown segment selection. Unknown selection can't be
  // changed before this duration. Note that when this is set to 0, the unknown
  // segment selections are IGNORED by the platform when it had valid selection
  // in the past. ONLY when this value is positive unknown segments are treated
  // as output option after having served other valid segments.
  base::TimeDelta unknown_selection_ttl;

  // List of segment ids that the current config requires to be available.
  std::vector<optimization_guide::proto::OptimizationTarget> segment_ids;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_
