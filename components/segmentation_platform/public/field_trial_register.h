// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FIELD_TRIAL_REGISTER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FIELD_TRIAL_REGISTER_H_

#include <string_view>

#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

// A delegate class that handles recording of the selected segmentation groups
// to metrics.
class FieldTrialRegister {
 public:
  FieldTrialRegister() = default;
  virtual ~FieldTrialRegister() = default;

  FieldTrialRegister(const FieldTrialRegister&) = delete;
  FieldTrialRegister& operator=(const FieldTrialRegister&) = delete;

  // Records that the current session uses `trial_name` and `group_name` as
  // segmentation groups. Calling multiple times with same `trial_name`
  // will replace the existing group with the new one, but note that the
  // previous logs already closed / staged for upload will not be changed.
  virtual void RegisterFieldTrial(std::string_view trial_name,
                                  std::string_view group_name) = 0;

  // Registers subsegments based on the `subsegment_rank` of the segment when
  // the subsegment mapping was provided by the segment. The `subsegment_rank`
  // should be computed based on the subsegment discrete mapping in the model
  // metadata.
  virtual void RegisterSubsegmentFieldTrialIfNeeded(std::string_view trial_name,
                                                    proto::SegmentId segment_id,
                                                    int subsegment_rank) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_FIELD_TRIAL_REGISTER_H_
