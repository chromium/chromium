// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
namespace segmentation_platform {

class DeviceSwitcherModelTest : public DefaultModelTestBase {
 public:
  DeviceSwitcherModelTest()
      : DefaultModelTestBase(std::make_unique<DeviceSwitcherModel>()) {}
  ~DeviceSwitcherModelTest() override = default;
};

TEST_F(DeviceSwitcherModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(DeviceSwitcherModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();

  // Input vector empty.
  ModelProvider::Request input;
  ExpectExecutionWithInput(input, /*expected_error=*/true,
                           /*expected_result=*/{});

  // Syncing failed
  input = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0, 0, 0, 0, 0, 0, 0, 1});
  ExpectClassifierResults(input, {DeviceSwitcherModel::kNotSyncedLabel});

  // Android phone switcher.
  input = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{10, 0, 0, 0, 0, 0, 0, 0});
  ExpectClassifierResults(input, {DeviceSwitcherModel::kAndroidPhoneLabel});

  // IOS phone switcher.
  input = {0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0, 9, 0, 0, 0, 0, 0, 0});
  ExpectClassifierResults(input, {DeviceSwitcherModel::kIosPhoneChromeLabel});

  // Android tablet switcher.
  input = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0, 0, 8, 0, 0, 0, 0, 0});
  ExpectClassifierResults(input, {DeviceSwitcherModel::kAndroidTabletLabel});

  // IOS tablet switcher.
  input = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0, 0, 0, 7, 0, 0, 0, 0});
  ExpectClassifierResults(input, {DeviceSwitcherModel::kIosTabletLabel});

  // Desktop switcher.
  input = {0, 0, 0, 0, 0, 1, 1, 1, 1, 0};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0, 0, 0, 0, 6, 0, 0, 0});
  ExpectClassifierResults(input, {DeviceSwitcherModel::kDesktopLabel});

  // Other switcher.
  input = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0, 0, 0, 0, 0, 3, 0, 0});
  ExpectClassifierResults(input, {DeviceSwitcherModel::kOtherLabel});

  // Synced, no other device.
  input = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{0, 0, 0, 0, 0, 0, 2, 0});
  ExpectClassifierResults(input,
                          {DeviceSwitcherModel::kSyncedAndFirstDeviceLabel});

  // Multiple labels.
  input = {0, 1, 1, 1, 1, 1, 0, 0, 0, 1};
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           /*expected_result=*/{10, 9, 8, 7, 6, 3, 0, 0});
  ExpectClassifierResults(input, {DeviceSwitcherModel::kAndroidPhoneLabel,
                                  DeviceSwitcherModel::kIosPhoneChromeLabel,
                                  DeviceSwitcherModel::kAndroidTabletLabel,
                                  DeviceSwitcherModel::kIosTabletLabel,
                                  DeviceSwitcherModel::kDesktopLabel,
                                  DeviceSwitcherModel::kOtherLabel});
}

}  // namespace segmentation_platform
