// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/trigger.h"

namespace segmentation_platform {

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
                    std::unique_ptr<DefaultModelProvider> default_provider);
    SegmentMetadata(SegmentMetadata&&);

    ~SegmentMetadata();

    bool operator==(const SegmentMetadata& other) const;

    // The name used for this segment in UMA filters.
    std::string uma_name;

    // The default model or score used when server provided model is
    // unavailable.
    std::unique_ptr<DefaultModelProvider> default_provider;
  };
  base::flat_map<proto::SegmentId, std::unique_ptr<SegmentMetadata>> segments;

  // The service will run models in the background and keep results ready for
  // use at all times. The TTL settings in the model metadata should be used to
  // specify how often to refresh results.
  bool auto_execute_and_cache = false;

  // List of custom  inputs provided for running the segments. The delegate will
  // be invoked for input based on the model metadata's input processing config.
  // Note: 2 configs cannot provide input delegates for the same FillPolicy. To
  // share the delegate implementation, the delegates need to be provided by
  // `SegmentationPlatformServiceFactory`.
  base::flat_map<proto::CustomInput::FillPolicy,
                 std::unique_ptr<processing::InputDelegate>>
      input_delegates;

  // Helper methods to add segments to `segments`:
  void AddSegmentId(proto::SegmentId segment_id);
  void AddSegmentId(proto::SegmentId segment_id,
                    std::unique_ptr<DefaultModelProvider> default_provider);

  // Returns the filter name that will be shown in the metrics for this
  // segmentation config.
  std::string GetSegmentationFilterName() const;

  // Returns the segment name for the `segment` used by the metrics.
  std::string GetSegmentUmaName(proto::SegmentId segment) const;

  // Whether the segment is a boolean model.
  // TODO(haileywang): update config_parser to include this field.
  bool is_boolean_segment = false;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_
