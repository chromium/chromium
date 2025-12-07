// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_DEVICE_SWITCHER_RESULT_DISPATCHER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_DEVICE_SWITCHER_RESULT_DISPATCHER_H_

#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

class MockDeviceSwitcherResultDispatcher
    : public DeviceSwitcherResultDispatcher {
 public:
  MockDeviceSwitcherResultDispatcher(
      SegmentationPlatformService* segmentation_service,
      syncer::DeviceInfoTracker* device_info_tracker,
      PrefService* prefs,
      FieldTrialRegister* field_trial_register)
      : DeviceSwitcherResultDispatcher(segmentation_service,
                                       device_info_tracker,
                                       prefs,
                                       field_trial_register) {}

  ~MockDeviceSwitcherResultDispatcher() = default;

  MOCK_METHOD(ClassificationResult, GetCachedClassificationResult, ());

  MOCK_METHOD(void,
              WaitForClassificationResult,
              (base::TimeDelta, ClassificationResultCallback));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_DEVICE_SWITCHER_RESULT_DISPATCHER_H_
