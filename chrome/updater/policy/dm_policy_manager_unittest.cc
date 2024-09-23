// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/dm_policy_manager.h"

#include <optional>

#include "base/enterprise_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/test/unit_test_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

#if BUILDFLAG(IS_MAC)

// This binary array is an actual policy response from DM server for
// Mac client. The response has the following Omaha policy values:
//
//  omaha_settings = {
//    "com.google.chrome" = {
//      RollbackToTargetVersion = 1;
//      TargetVersionPrefix = "82.0.";
//    };
//  }
//
const uint8_t kOmahaPolicyResponseData[] = {
    0x08, 0xc8, 0x01, 0x1a, 0x85, 0x04, 0x0a, 0x1a, 0x67, 0x6f, 0x6f, 0x67,
    0x6c, 0x65, 0x2f, 0x6d, 0x61, 0x63, 0x68, 0x69, 0x6e, 0x65, 0x2d, 0x6c,
    0x65, 0x76, 0x65, 0x6c, 0x2d, 0x6f, 0x6d, 0x61, 0x68, 0x61, 0x10, 0xff,
    0xc1, 0x9a, 0xa2, 0xa1, 0x2e, 0x1a, 0x8c, 0x02, 0x41, 0x42, 0x6a, 0x6d,
    0x54, 0x37, 0x6b, 0x58, 0x4a, 0x71, 0x4b, 0x6e, 0x77, 0x31, 0x6d, 0x61,
    0x74, 0x62, 0x55, 0x5f, 0x6b, 0x51, 0x61, 0x43, 0x4b, 0x41, 0x54, 0x52,
    0x59, 0x38, 0x69, 0x6d, 0x44, 0x66, 0x34, 0x6d, 0x73, 0x69, 0x36, 0x31,
    0x36, 0x49, 0x59, 0x67, 0x34, 0x6a, 0x75, 0x6b, 0x79, 0x44, 0x48, 0x38,
    0x66, 0x62, 0x36, 0x45, 0x56, 0x68, 0x5f, 0x4f, 0x76, 0x46, 0x53, 0x5a,
    0x78, 0x42, 0x6f, 0x4c, 0x47, 0x79, 0x5f, 0x58, 0x70, 0x2d, 0x69, 0x55,
    0x2d, 0x35, 0x64, 0x51, 0x79, 0x73, 0x6f, 0x78, 0x2d, 0x48, 0x63, 0x67,
    0x39, 0x4a, 0x43, 0x73, 0x6f, 0x76, 0x4f, 0x57, 0x69, 0x56, 0x68, 0x42,
    0x4a, 0x39, 0x54, 0x6f, 0x6b, 0x66, 0x68, 0x5a, 0x55, 0x68, 0x48, 0x54,
    0x6e, 0x74, 0x39, 0x6b, 0x77, 0x52, 0x72, 0x4a, 0x71, 0x59, 0x69, 0x66,
    0x34, 0x61, 0x43, 0x77, 0x4e, 0x57, 0x43, 0x4c, 0x56, 0x6a, 0x45, 0x7a,
    0x32, 0x4e, 0x73, 0x54, 0x44, 0x64, 0x34, 0x6d, 0x64, 0x32, 0x52, 0x4d,
    0x53, 0x6f, 0x50, 0x39, 0x45, 0x39, 0x35, 0x45, 0x2d, 0x34, 0x47, 0x4a,
    0x45, 0x46, 0x5f, 0x47, 0x42, 0x33, 0x73, 0x58, 0x35, 0x4b, 0x48, 0x48,
    0x39, 0x76, 0x74, 0x31, 0x2d, 0x44, 0x39, 0x44, 0x65, 0x4a, 0x76, 0x37,
    0x39, 0x5f, 0x66, 0x58, 0x37, 0x67, 0x58, 0x46, 0x46, 0x76, 0x53, 0x54,
    0x71, 0x45, 0x33, 0x6c, 0x35, 0x79, 0x5a, 0x37, 0x70, 0x31, 0x66, 0x6d,
    0x68, 0x70, 0x47, 0x34, 0x48, 0x32, 0x61, 0x44, 0x78, 0x79, 0x4d, 0x44,
    0x73, 0x6b, 0x4c, 0x7a, 0x39, 0x43, 0x77, 0x4f, 0x36, 0x6e, 0x58, 0x4e,
    0x77, 0x64, 0x57, 0x6d, 0x54, 0x32, 0x50, 0x62, 0x62, 0x79, 0x38, 0x56,
    0x5f, 0x44, 0x36, 0x69, 0x5f, 0x4d, 0x45, 0x53, 0x6f, 0x61, 0x67, 0x6d,
    0x66, 0x64, 0x38, 0x63, 0x32, 0x71, 0x2d, 0x4e, 0x54, 0x73, 0x5f, 0x76,
    0x22, 0x47, 0x9a, 0x01, 0x44, 0x0a, 0x26, 0x7b, 0x38, 0x41, 0x36, 0x39,
    0x44, 0x33, 0x34, 0x35, 0x2d, 0x44, 0x35, 0x36, 0x34, 0x2d, 0x34, 0x36,
    0x33, 0x43, 0x2d, 0x41, 0x46, 0x46, 0x31, 0x2d, 0x41, 0x36, 0x39, 0x44,
    0x39, 0x45, 0x35, 0x33, 0x30, 0x46, 0x39, 0x36, 0x7d, 0x12, 0x11, 0x63,
    0x6f, 0x6d, 0x2e, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2e, 0x43, 0x68,
    0x72, 0x6f, 0x6d, 0x65, 0x2a, 0x05, 0x38, 0x32, 0x2e, 0x30, 0x2e, 0x30,
    0x01, 0x30, 0x00, 0x3a, 0x1e, 0x61, 0x64, 0x6d, 0x69, 0x6e, 0x40, 0x63,
    0x68, 0x72, 0x6f, 0x6d, 0x65, 0x65, 0x6e, 0x74, 0x65, 0x72, 0x70, 0x72,
    0x69, 0x73, 0x65, 0x74, 0x65, 0x61, 0x6d, 0x2e, 0x63, 0x6f, 0x6d, 0x42,
    0x0c, 0x43, 0x30, 0x32, 0x57, 0x37, 0x30, 0x33, 0x34, 0x48, 0x54, 0x44,
    0x44, 0x7a, 0x40, 0x61, 0x63, 0x31, 0x31, 0x65, 0x64, 0x32, 0x30, 0x30,
    0x64, 0x31, 0x33, 0x65, 0x35, 0x33, 0x61, 0x62, 0x31, 0x30, 0x30, 0x38,
    0x63, 0x37, 0x34, 0x37, 0x35, 0x65, 0x38, 0x36, 0x61, 0x33, 0x31, 0x34,
    0x34, 0x62, 0x61, 0x31, 0x35, 0x31, 0x33, 0x61, 0x66, 0x33, 0x66, 0x38,
    0x63, 0x34, 0x33, 0x32, 0x35, 0x66, 0x30, 0x65, 0x61, 0x63, 0x30, 0x62,
    0x36, 0x64, 0x35, 0x66, 0x61, 0x63, 0x37, 0xea, 0x01, 0x15, 0x31, 0x30,
    0x35, 0x32, 0x35, 0x30, 0x35, 0x30, 0x36, 0x30, 0x39, 0x37, 0x39, 0x37,
    0x39, 0x37, 0x35, 0x33, 0x39, 0x36, 0x38, 0x22, 0x80, 0x02, 0x04, 0x1a,
    0x80, 0xed, 0x73, 0x64, 0xc2, 0xe1, 0x03, 0x33, 0x2b, 0x75, 0x8d, 0x73,
    0x70, 0x8f, 0xbb, 0xe0, 0xdc, 0xca, 0xf5, 0x33, 0x2a, 0xe5, 0x89, 0x4b,
    0x8d, 0xb5, 0x72, 0x6a, 0x15, 0x00, 0x1a, 0x96, 0x23, 0xd4, 0x55, 0xfc,
    0x33, 0xfe, 0xe7, 0x03, 0xd6, 0xa6, 0x12, 0xfe, 0x15, 0x67, 0x55, 0xdd,
    0xf3, 0x8f, 0xd3, 0xab, 0xda, 0xb0, 0x81, 0x9d, 0x51, 0x24, 0x2d, 0x11,
    0xc1, 0x07, 0x8b, 0x2e, 0x02, 0x1f, 0x6c, 0xa6, 0xbd, 0x3b, 0x43, 0xa5,
    0x5a, 0xb1, 0x5e, 0x17, 0xfc, 0x2b, 0x81, 0x69, 0x64, 0xf5, 0x42, 0xa8,
    0x97, 0xe4, 0x37, 0x98, 0xfc, 0xc7, 0x49, 0x0a, 0xb8, 0x9c, 0x16, 0x18,
    0x98, 0xf9, 0xee, 0xb8, 0x36, 0xf4, 0x20, 0x5d, 0xe5, 0x54, 0x62, 0xb7,
    0xbd, 0x5c, 0x52, 0x86, 0x61, 0x4a, 0x47, 0x46, 0xbe, 0x55, 0x25, 0x0c,
    0x52, 0x52, 0x3d, 0x6d, 0xbd, 0x5d, 0x47, 0xbd, 0x46, 0xd9, 0x43, 0xca,
    0xd0, 0xe9, 0x81, 0x9b, 0x22, 0x92, 0xa3, 0xdf, 0x23, 0xb0, 0xe6, 0x64,
    0x03, 0xbc, 0x96, 0x9f, 0xe8, 0xd3, 0x0e, 0xe6, 0xcf, 0x39, 0x02, 0xa2,
    0x17, 0xeb, 0x9e, 0xa7, 0xb8, 0x7c, 0x16, 0xac, 0x7e, 0x3f, 0x37, 0x18,
    0x01, 0x53, 0x94, 0xcc, 0x38, 0xa2, 0x97, 0xbb, 0x2d, 0x7f, 0x67, 0xe8,
    0x19, 0xeb, 0x63, 0xa0, 0xd7, 0x22, 0x4d, 0x9a, 0x4e, 0x6a, 0x60, 0x80,
    0x5c, 0x1b, 0x92, 0x64, 0x3b, 0x36, 0x0d, 0x43, 0x01, 0x6d, 0x39, 0xe6,
    0xf0, 0x2c, 0xb2, 0x56, 0x11, 0x2f, 0x84, 0xf4, 0x49, 0x41, 0x21, 0xb7,
    0x2f, 0x7b, 0x0b, 0x47, 0x0a, 0x63, 0xd2, 0x19, 0x34, 0xca, 0x95, 0xd4,
    0x87, 0xc9, 0x8a, 0x2e, 0xde, 0xc3, 0xf0, 0xef, 0x04, 0x3e, 0x76, 0xba,
    0x70, 0x78, 0x08, 0x03, 0x4f, 0x96, 0x37, 0x68, 0xc0, 0xa9, 0xdd, 0x49,
    0xcc, 0xd0, 0x52, 0x1a, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2f, 0x6d,
    0x61, 0x63, 0x68, 0x69, 0x6e, 0x65, 0x2d, 0x6c, 0x65, 0x76, 0x65, 0x6c,
    0x2d, 0x6f, 0x6d, 0x61, 0x68, 0x61, 0x58, 0x01,
};

#endif  // BUILDFLAG(IS_MAC)

class TestTokenService
    : public device_management_storage::TokenServiceInterface {
 public:
  TestTokenService()
      : enrollment_token_("TestEnrollmentToken"), dm_token_("TestDMToken") {}
  ~TestTokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return "TestDeviceID"; }

  bool IsEnrollmentMandatory() const override { return false; }

  bool StoreEnrollmentToken(const std::string& enrollment_token) override {
    enrollment_token_ = enrollment_token;
    return true;
  }

  bool DeleteEnrollmentToken() override { return StoreEnrollmentToken(""); }

  std::string GetEnrollmentToken() const override { return enrollment_token_; }

  bool StoreDmToken(const std::string& dm_token) override {
    dm_token_ = dm_token;
    return true;
  }

  bool DeleteDmToken() override {
    dm_token_.clear();
    return true;
  }

  std::string GetDmToken() const override { return dm_token_; }

 private:
  std::string enrollment_token_;
  std::string dm_token_;
};

std::string CannedOmahaPolicyFetchResponse() {
  ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;

  omaha_settings.set_auto_update_check_period_minutes(111);
  omaha_settings.set_download_preference("cacheable");
  omaha_settings.mutable_updates_suppressed()->set_start_hour(8);
  omaha_settings.mutable_updates_suppressed()->set_start_minute(8);
  omaha_settings.mutable_updates_suppressed()->set_duration_min(47);
  omaha_settings.set_proxy_mode("proxy_pac_script");
  omaha_settings.set_proxy_pac_url("foo.c/proxy.pa");
  omaha_settings.set_install_default(
      ::wireless_android_enterprise_devicemanagement::INSTALL_DEFAULT_DISABLED);
  omaha_settings.set_update_default(
      ::wireless_android_enterprise_devicemanagement::MANUAL_UPDATES_ONLY);

  ::wireless_android_enterprise_devicemanagement::ApplicationSettings app;
  app.set_app_guid(test::kChromeAppId);

  app.set_install(
      ::wireless_android_enterprise_devicemanagement::INSTALL_DISABLED);
  app.set_update(
      ::wireless_android_enterprise_devicemanagement::AUTOMATIC_UPDATES_ONLY);
  app.set_target_version_prefix("3.6.55");
  app.set_rollback_to_target_version(
      ::wireless_android_enterprise_devicemanagement::
          ROLLBACK_TO_TARGET_VERSION_ENABLED);

  omaha_settings.mutable_application_settings()->Add(std::move(app));

  ::enterprise_management::PolicyData policy_data;
  policy_data.set_policy_value(omaha_settings.SerializeAsString());

  ::enterprise_management::PolicyFetchResponse response;
  response.set_policy_data(policy_data.SerializeAsString());
  return response.SerializeAsString();
}

}  // namespace

TEST(DMPolicyManager, DeviceManagementOverride) {
  ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;

  EXPECT_TRUE(base::MakeRefCounted<DMPolicyManager>(omaha_settings, true)
                  ->HasActiveDevicePolicies());
  EXPECT_FALSE(base::MakeRefCounted<DMPolicyManager>(omaha_settings, false)
                   ->HasActiveDevicePolicies());
}

TEST(DMPolicyManager, PolicyManagerFromEmptyProto) {
  ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;

  auto policy_manager(base::MakeRefCounted<DMPolicyManager>(omaha_settings));

  EXPECT_TRUE(policy_manager->HasActiveDevicePolicies());
  EXPECT_EQ(policy_manager->source(), "Device Management");

  EXPECT_EQ(policy_manager->CloudPolicyOverridesPlatformPolicy(), std::nullopt);
  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), std::nullopt);
  EXPECT_EQ(policy_manager->GetUpdatesSuppressedTimes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetDownloadPreference(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyMode(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyServer(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), std::nullopt);
  EXPECT_FALSE(
      policy_manager->GetEffectivePolicyForAppInstalls(test::kChromeAppId));
  EXPECT_FALSE(
      policy_manager->GetEffectivePolicyForAppUpdates(test::kChromeAppId));
  EXPECT_FALSE(
      policy_manager->IsRollbackToTargetVersionAllowed(test::kChromeAppId));
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(test::kChromeAppId),
            std::nullopt);
}

TEST(DMPolicyManager, PolicyManagerFromProto) {
  ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;

  // Global policies.
  omaha_settings.set_auto_update_check_period_minutes(111);
  omaha_settings.mutable_updates_suppressed()->set_start_hour(9);
  omaha_settings.mutable_updates_suppressed()->set_start_minute(30);
  omaha_settings.mutable_updates_suppressed()->set_duration_min(120);
  omaha_settings.set_download_preference("test_download_preference");
  omaha_settings.set_proxy_server("test_proxy_server");
  omaha_settings.set_proxy_mode("test_proxy_mode");
  omaha_settings.set_proxy_pac_url("foo.c/proxy.pa");
  omaha_settings.set_install_default(
      ::wireless_android_enterprise_devicemanagement::
          INSTALL_DEFAULT_ENABLED_MACHINE_ONLY);
  omaha_settings.set_update_default(
      ::wireless_android_enterprise_devicemanagement::MANUAL_UPDATES_ONLY);
  omaha_settings.set_cloud_policy_overrides_platform_policy(true);

  // Chrome specific policies.
  ::wireless_android_enterprise_devicemanagement::ApplicationSettings chrome;
  chrome.set_app_guid(test::kChromeAppId);
  chrome.set_install(
      ::wireless_android_enterprise_devicemanagement::INSTALL_DISABLED);
  chrome.set_update(
      ::wireless_android_enterprise_devicemanagement::AUTOMATIC_UPDATES_ONLY);
  chrome.set_target_version_prefix("81.");
  chrome.set_rollback_to_target_version(
      ::wireless_android_enterprise_devicemanagement::
          ROLLBACK_TO_TARGET_VERSION_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(chrome));

  // App1 policies.
  constexpr char kApp1[] = "app1.chromium.org";
  ::wireless_android_enterprise_devicemanagement::ApplicationSettings app1;
  app1.set_app_guid(kApp1);
  app1.set_bundle_identifier(kApp1);
  app1.set_install(::wireless_android_enterprise_devicemanagement::
                       INSTALL_ENABLED_MACHINE_ONLY);
  app1.set_update(
      ::wireless_android_enterprise_devicemanagement::UPDATES_DISABLED);
  app1.set_target_channel("canary");
  omaha_settings.mutable_application_settings()->Add(std::move(app1));

  // App2 policies.
  constexpr char kApp2[] = "app2.chromium.org";
  ::wireless_android_enterprise_devicemanagement::ApplicationSettings app2;
  app2.set_app_guid(kApp2);
  app2.set_install(
      ::wireless_android_enterprise_devicemanagement::INSTALL_FORCED);
  app2.set_update(
      ::wireless_android_enterprise_devicemanagement::UPDATES_ENABLED);
  app2.set_target_channel("dev");
  omaha_settings.mutable_application_settings()->Add(std::move(app2));

  auto policy_manager(base::MakeRefCounted<DMPolicyManager>(omaha_settings));

  EXPECT_TRUE(policy_manager->HasActiveDevicePolicies());
  EXPECT_EQ(policy_manager->source(), "Device Management");

  // Verify global policies
  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), base::Minutes(111));

  std::optional<UpdatesSuppressedTimes> suppressed_times =
      policy_manager->GetUpdatesSuppressedTimes();
  ASSERT_TRUE(suppressed_times);
  EXPECT_EQ(suppressed_times->start_hour_, 9);
  EXPECT_EQ(suppressed_times->start_minute_, 30);
  EXPECT_EQ(suppressed_times->duration_minute_, 120);

  EXPECT_EQ(policy_manager->GetDownloadPreference(),
            "test_download_preference");

  EXPECT_EQ(policy_manager->GetProxyServer(), "test_proxy_server");
  EXPECT_EQ(policy_manager->GetProxyMode(), "test_proxy_mode");
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), "foo.c/proxy.pa");

  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), std::nullopt);
  EXPECT_EQ(policy_manager->GetForceInstallApps(),
            std::vector<std::string>({kApp2}));
  EXPECT_EQ(policy_manager->GetAppsWithPolicy(),
            std::vector<std::string>({test::kChromeAppId, kApp1, kApp2}));
  EXPECT_TRUE(*policy_manager->CloudPolicyOverridesPlatformPolicy());

  // Verify Chrome policies.
  EXPECT_EQ(
      policy_manager->GetEffectivePolicyForAppInstalls(test::kChromeAppId),
      kPolicyDisabled);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(test::kChromeAppId),
            kPolicyAutomaticUpdatesOnly);
  EXPECT_TRUE(
      policy_manager->IsRollbackToTargetVersionAllowed(test::kChromeAppId));
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(test::kChromeAppId), "81.");
  EXPECT_EQ(policy_manager->GetTargetChannel(test::kChromeAppId), std::nullopt);

  // Verify app1 policies.
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(kApp1),
            kPolicyEnabledMachineOnly);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(kApp1),
            kPolicyDisabled);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(kApp1),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(kApp1), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel(kApp1), "canary");

  // Verify app2 policies.
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(kApp2),
            kPolicyForceInstallMachine);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(kApp2),
            kPolicyEnabled);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(kApp2),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(kApp2), std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetChannel(kApp2), "dev");

  // Verify that if no app-specific polices, fallback to global-level policies
  // or return false if no fallback is available.
  const std::string app_guid = "ArbitraryAppGuid";
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(app_guid),
            kPolicyEnabledMachineOnly);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(app_guid),
            kPolicyManualUpdatesOnly);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(app_guid),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(app_guid), std::nullopt);
}

#if BUILDFLAG(IS_MAC)

TEST(DMPolicyManager, PolicyManagerFromDMResponse) {
  enterprise_management::PolicyFetchResponse response;
  enterprise_management::PolicyData policy_data;
  ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;

  EXPECT_TRUE(response.ParseFromArray(kOmahaPolicyResponseData,
                                      sizeof(kOmahaPolicyResponseData)));
  EXPECT_TRUE(response.has_policy_data());
  EXPECT_TRUE(policy_data.ParseFromString(response.policy_data()));
  EXPECT_TRUE(policy_data.has_policy_value());
  EXPECT_TRUE(omaha_settings.ParseFromString(policy_data.policy_value()));

  auto policy_manager(base::MakeRefCounted<DMPolicyManager>(omaha_settings));

  EXPECT_TRUE(policy_manager->HasActiveDevicePolicies());
  EXPECT_EQ(policy_manager->source(), "Device Management");

  EXPECT_EQ(policy_manager->CloudPolicyOverridesPlatformPolicy(), std::nullopt);
  EXPECT_EQ(policy_manager->GetLastCheckPeriod(), std::nullopt);
  EXPECT_EQ(policy_manager->GetUpdatesSuppressedTimes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetDownloadPreference(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyMode(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyPacUrl(), std::nullopt);
  EXPECT_EQ(policy_manager->GetProxyServer(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheSizeLimitMBytes(), std::nullopt);
  EXPECT_EQ(policy_manager->GetPackageCacheExpirationTimeDays(), std::nullopt);

  const std::string chrome_guid = "com.google.Chrome";
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppInstalls(chrome_guid),
            std::nullopt);
  EXPECT_EQ(policy_manager->GetEffectivePolicyForAppUpdates(chrome_guid),
            std::nullopt);
  EXPECT_EQ(policy_manager->IsRollbackToTargetVersionAllowed(chrome_guid),
            true);

  EXPECT_EQ(policy_manager->GetTargetVersionPrefix(chrome_guid), "82.0.");
}

#endif  // BUILDFLAG(IS_MAC)

TEST(DMPolicyManager, GetOmahaPolicySettings) {
  device_management_storage::DMPolicyMap policies({
      {"google/machine-level-omaha", CannedOmahaPolicyFetchResponse()},
  });
  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());
  auto storage = CreateDMStorage(cache_root.GetPath(),
                                 std::make_unique<TestTokenService>());
  EXPECT_TRUE(storage->CanPersistPolicies());
  EXPECT_TRUE(storage->PersistPolicies(policies));

  std::optional<
      ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto>
      omaha_settings = GetOmahaPolicySettings(storage);
  ASSERT_TRUE(omaha_settings);
  EXPECT_EQ(omaha_settings->auto_update_check_period_minutes(), 111);

  EXPECT_EQ(omaha_settings->updates_suppressed().start_hour(), 8);
  EXPECT_EQ(omaha_settings->updates_suppressed().start_minute(), 8);
  EXPECT_EQ(omaha_settings->updates_suppressed().duration_min(), 47);

  EXPECT_EQ(omaha_settings->proxy_mode(), "proxy_pac_script");
  EXPECT_EQ(omaha_settings->proxy_pac_url(), "foo.c/proxy.pa");
  EXPECT_FALSE(omaha_settings->has_proxy_server());

  EXPECT_EQ(omaha_settings->download_preference(), "cacheable");

  // Chrome policies.
  const auto& chrome_settings = omaha_settings->application_settings()[0];
  EXPECT_EQ(chrome_settings.install(),
            ::wireless_android_enterprise_devicemanagement::INSTALL_DISABLED);
  EXPECT_EQ(
      chrome_settings.update(),
      ::wireless_android_enterprise_devicemanagement::AUTOMATIC_UPDATES_ONLY);
  EXPECT_EQ(chrome_settings.target_version_prefix(), "3.6.55");
  EXPECT_EQ(chrome_settings.rollback_to_target_version(),
            ::wireless_android_enterprise_devicemanagement::
                ROLLBACK_TO_TARGET_VERSION_ENABLED);

  // Verify no policy settings once device is de-registered.
  EXPECT_TRUE(storage->InvalidateDMToken());
  EXPECT_TRUE(storage->IsDeviceDeregistered());
  EXPECT_FALSE(storage->IsValidDMToken());
  ASSERT_FALSE(GetOmahaPolicySettings(storage));
}

}  // namespace updater
