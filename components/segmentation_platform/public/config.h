// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/trigger.h"

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

// The key is used to decide whether the user likes to use Feed.
const char kFeedUserSegmentationKey[] = "feed_user_segment";

// The key is used to show a contextual page action.
const char kContextualPageActionsKey[] = "contextual_page_actions";

// The key provide a list of segment IDs, separated by commas, whose ML model
// execution results are allowed to be uploaded through UKM.
const char kSegmentIdsAllowedForReportingKey[] =
    "segment_ids_allowed_for_reporting";

const char kSubsegmentDiscreteMappingSuffix[] = "_subsegment";

// Contains various finch configuration params used by the segmentation
// platform.
struct Config {
  Config();
  ~Config();

  // Disallow copy/assign.
  Config(const Config& other) = delete;
  Config& operator=(const Config& other) = delete;

  // The key is used to distinguish between different types of segmentation
  // usages. Currently it is mainly used by the segment selector to find the
  // discrete mapping and writing results to prefs.
  std::string segmentation_key;

  // The name used for the segmentation key in UMA filters.
  std::string segmentation_uma_name;

  // The trigger event type that triggers segment selection. If trigger is
  // non-none, |on_demand_execution| must be true.
  TriggerType trigger = TriggerType::kNone;

  // Time to live for a segment selection. Segment selection can't be changed
  // before this duration.
  base::TimeDelta segment_selection_ttl;

  // Time to live for an unknown segment selection. Unknown selection can't be
  // changed before this duration. Note that when this is set to 0, the unknown
  // segment selections are IGNORED by the platform when it had valid selection
  // in the past. ONLY when this value is positive unknown segments are treated
  // as output option after having served other valid segments.
  base::TimeDelta unknown_selection_ttl;

  // List of segments needed to make a selection.
  struct SegmentMetadata {
    explicit SegmentMetadata(const std::string& uma_name);
    SegmentMetadata(const std::string& uma_name,
                    std::unique_ptr<ModelProvider> default_provider);
    SegmentMetadata(SegmentMetadata&&);

    ~SegmentMetadata();

    bool operator==(const SegmentMetadata& other) const;

    // The name used for this segment in UMA filters.
    std::string uma_name;

    // The default model or score used when server provided model is
    // unavailable.
    std::unique_ptr<ModelProvider> default_provider;
  };
  base::flat_map<proto::SegmentId, std::unique_ptr<SegmentMetadata>> segments;

  // The selection only supports returning results from on-demand model
  // executions instead of returning result from previous sessions. The
  // selection TTLs are ignored in this config.
  bool on_demand_execution = false;

  // List of custom  inputs provided for running the segments. The delegate will
  // be invoked for input based on the model metadata's input processing config.
  // Note: 2 configs cannot provide input delegates for the same FillPolicy. To
  // share the delegate implementation, the delegates need to be provided by
  // `SegmentationPlatformServiceFactory`.
  base::flat_map<proto::CustomInput::FillPolicy,
                 std::unique_ptr<processing::InputDelegate>>
      input_delegates;

  // Returns the filter name that will be shown in the metrics for this
  // segmentation config.
  std::string GetSegmentationFilterName() const;

  // Returns the segment name for the `segment` used by the metrics.
  std::string GetSegmentUmaName(proto::SegmentId segment) const;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_
