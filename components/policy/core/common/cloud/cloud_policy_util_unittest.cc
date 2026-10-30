// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_util.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "base/test/scoped_feature_list.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace policy {

class CloudPolicyUtilTest : public testing::Test {
 protected:
  void TearDown() override {
#if BUILDFLAG(IS_ANDROID)
    base::android::device_info::reset_device_name_for_testing();
#endif  // BUILDFLAG(IS_ANDROID)
  }
};

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(CloudPolicyUtilTest, GetMachineNameAndDeviceIdentifier) {
  const std::string machine_name = GetMachineName();
  EXPECT_FALSE(machine_name.empty());
  EXPECT_EQ(machine_name, GetDeviceName());

  std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
      device_identifier = GetBrowserDeviceIdentifier();
  ASSERT_TRUE(device_identifier);
  EXPECT_EQ(machine_name, device_identifier->computer_name());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
TEST_F(CloudPolicyUtilTest, GetMachineNameAndroidCustomDeviceNameSet) {
  constexpr char kCustomDeviceName[] = "Custom Pixel Device";
  base::android::device_info::set_device_name_for_testing(kCustomDeviceName);

  EXPECT_EQ(kCustomDeviceName, GetMachineName());
  EXPECT_EQ(kCustomDeviceName, GetDeviceName());

  std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
      device_identifier = GetBrowserDeviceIdentifier();
  ASSERT_TRUE(device_identifier);
  EXPECT_EQ(kCustomDeviceName, device_identifier->computer_name());
}

TEST_F(CloudPolicyUtilTest,
       GetMachineNameAndroidEmptyDeviceNameFallsBackToModel) {
  base::android::device_info::set_device_name_for_testing("");

  const std::string expected_model = base::android::android_info::model();
  EXPECT_FALSE(expected_model.empty());
  EXPECT_EQ(expected_model, GetMachineName());
  EXPECT_EQ(expected_model, GetDeviceName());

  std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
      device_identifier = GetBrowserDeviceIdentifier();
  ASSERT_TRUE(device_identifier);
  EXPECT_EQ(expected_model, device_identifier->computer_name());
}

TEST_F(CloudPolicyUtilTest, GetMachineNameAndroidFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kReportAndroidMachineName);
  base::android::device_info::set_device_name_for_testing(
      "Custom Pixel Device");

  EXPECT_TRUE(GetMachineName().empty());
  EXPECT_TRUE(GetDeviceName().empty());

  std::unique_ptr<enterprise_management::BrowserDeviceIdentifier>
      device_identifier = GetBrowserDeviceIdentifier();
  ASSERT_TRUE(device_identifier);
  EXPECT_TRUE(device_identifier->computer_name().empty());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace policy
