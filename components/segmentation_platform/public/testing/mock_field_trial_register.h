// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_FIELD_TRIAL_REGISTER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_FIELD_TRIAL_REGISTER_H_

#include "components/segmentation_platform/public/field_trial_register.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

class MockFieldTrialRegister : public FieldTrialRegister {
 public:
  MOCK_METHOD(void,
              RegisterFieldTrial,
              (std::string_view trial_name, std::string_view group_name));
  MOCK_METHOD(void,
              RegisterSubsegmentFieldTrialIfNeeded,
              (std::string_view trial_name,
               segmentation_platform::proto::SegmentId segment_id,
               int subsegment_rank));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_FIELD_TRIAL_REGISTER_H_
