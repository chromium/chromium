// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/policy/core/common/cloud/cloud_policy_util_ios.h"

#import <Foundation/Foundation.h>

#import "base/ios/device_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/policy/core/common/cloud/cloud_policy_util.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace policy {

namespace {

// Installs `app_config` as the Managed App Configuration.
void SetManagedAppConfiguration(NSDictionary* app_config) {
  [[NSUserDefaults standardUserDefaults]
      setObject:app_config
         forKey:kPolicyLoaderIOSConfigurationKey];
}

// Removes the Managed App Configuration.
void ClearManagedAppConfiguration() {
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

}  // namespace

class CloudPolicyUtilIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    // Make sure there is no pre-existing configuration present.
    ClearManagedAppConfiguration();
  }

  void TearDown() override {
    // NSUserDefaults persist across tests, so always clean up.
    ClearManagedAppConfiguration();
    PlatformTest::TearDown();
  }
};

TEST_F(CloudPolicyUtilIOSTest, NoManagedAppConfiguration) {
  EXPECT_TRUE(GetIOSManagedDeviceName().empty());
  // The machine name falls back to the Vendor ID.
  EXPECT_EQ(GetMachineName(), ios::device_util::GetVendorId());
  EXPECT_EQ(GetDeviceName(), ios::device_util::GetVendorId());
}

TEST_F(CloudPolicyUtilIOSTest, NoDeviceNameInManagedAppConfiguration) {
  SetManagedAppConfiguration(@{@"SomePolicy" : @"some_value"});

  EXPECT_TRUE(GetIOSManagedDeviceName().empty());
  EXPECT_EQ(GetMachineName(), ios::device_util::GetVendorId());
  EXPECT_EQ(GetDeviceName(), ios::device_util::GetVendorId());
}

TEST_F(CloudPolicyUtilIOSTest, DeviceName) {
  SetManagedAppConfiguration(
      @{kPolicyLoaderIOSDeviceNameKey : @"Michał's iPhone"});

  EXPECT_EQ(GetIOSManagedDeviceName(), "Michał's iPhone");
  EXPECT_EQ(GetMachineName(), "Michał's iPhone");
  EXPECT_EQ(GetDeviceName(), "Michał's iPhone");
  EXPECT_EQ(GetBrowserDeviceIdentifier()->computer_name(), "Michał's iPhone");
}

TEST_F(CloudPolicyUtilIOSTest, EmptyDeviceNameIsIgnored) {
  SetManagedAppConfiguration(@{kPolicyLoaderIOSDeviceNameKey : @""});

  EXPECT_TRUE(GetIOSManagedDeviceName().empty());
  EXPECT_EQ(GetMachineName(), ios::device_util::GetVendorId());
  EXPECT_EQ(GetDeviceName(), ios::device_util::GetVendorId());
}

TEST_F(CloudPolicyUtilIOSTest, NonStringDeviceNameIsIgnored) {
  // The value is controlled by the MDM administrator and may be of any
  // property list type.
  for (id non_string_value in @[ @42, @[ @"Device Name" ], @{@"key" : @"v"} ]) {
    SetManagedAppConfiguration(
        @{kPolicyLoaderIOSDeviceNameKey : non_string_value});

    EXPECT_TRUE(GetIOSManagedDeviceName().empty());
    EXPECT_EQ(GetMachineName(), ios::device_util::GetVendorId());
    EXPECT_EQ(GetDeviceName(), ios::device_util::GetVendorId());
  }
}

TEST_F(CloudPolicyUtilIOSTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kIOSReportMachineName);
  SetManagedAppConfiguration(@{kPolicyLoaderIOSDeviceNameKey : @"Device Name"});

  // The configuration is still readable, but isn't used as the machine name.
  EXPECT_EQ(GetIOSManagedDeviceName(), "Device Name");
  EXPECT_EQ(GetMachineName(), ios::device_util::GetVendorId());
  EXPECT_EQ(GetDeviceName(), ios::device_util::GetVendorId());
  EXPECT_EQ(GetBrowserDeviceIdentifier()->computer_name(),
            ios::device_util::GetVendorId());
}

}  // namespace policy
