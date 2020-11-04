// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_cached_policy_info.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/policy_manager.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "chrome/updater/unittest_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

class TestTokenService : public TokenServiceInterface {
 public:
  TestTokenService()
      : enrollment_token_("TestEnrollmentToken"), dm_token_("TestDMToken") {}
  ~TestTokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return "TestDeviceID"; }

  bool StoreEnrollmentToken(const std::string& enrollment_token) override {
    enrollment_token_ = enrollment_token;
    return true;
  }

  std::string GetEnrollmentToken() const override { return enrollment_token_; }

  bool StoreDmToken(const std::string& dm_token) override {
    dm_token_ = dm_token;
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
      ::wireless_android_enterprise_devicemanagement::INSTALL_DISABLED);
  omaha_settings.set_update_default(
      ::wireless_android_enterprise_devicemanagement::MANUAL_UPDATES_ONLY);

  ::wireless_android_enterprise_devicemanagement::ApplicationSettings app;
  app.set_app_guid(kChromeAppId);

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

#if defined(OS_MAC)

TEST(DMStorage, LoadDeviceID) {
  auto storage = base::MakeRefCounted<DMStorage>(
      base::FilePath(FILE_PATH_LITERAL("/TestPolicyCacheRoot")));
  EXPECT_FALSE(storage->GetDeviceID().empty());
}

#endif  // OS_MAC

TEST(DMStorage, PersistPolicies) {
  DMPolicyMap policies({
      {"google/machine-level-omaha", "serialized-omaha-policy-data"},
      {"foobar", "serialized-foobar-policy-data"},
  });
  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());

  // Mock stale policy files
  base::FilePath stale_poliy =
      cache_root.GetPath().Append(FILE_PATH_LITERAL("stale_policy_dir"));
  EXPECT_TRUE(base::CreateDirectory(stale_poliy));
  EXPECT_TRUE(base::DirectoryExists(stale_poliy));

  auto storage = base::MakeRefCounted<DMStorage>(cache_root.GetPath());
  EXPECT_TRUE(storage->PersistPolicies(policies));
  base::FilePath policy_info_file =
      cache_root.GetPath().AppendASCII("CachedPolicyInfo");
  EXPECT_FALSE(base::PathExists(policy_info_file));

  base::FilePath omaha_policy_file =
      cache_root.GetPath()
          .AppendASCII("Z29vZ2xlL21hY2hpbmUtbGV2ZWwtb21haGE=")
          .AppendASCII("PolicyFetchResponse");
  EXPECT_TRUE(base::PathExists(omaha_policy_file));
  std::string omaha_policy;
  EXPECT_TRUE(base::ReadFileToString(omaha_policy_file, &omaha_policy));
  EXPECT_EQ(omaha_policy, "serialized-omaha-policy-data");

  base::FilePath foobar_policy_file = cache_root.GetPath()
                                          .AppendASCII("Zm9vYmFy")
                                          .AppendASCII("PolicyFetchResponse");
  EXPECT_TRUE(base::PathExists(foobar_policy_file));
  std::string foobar_policy;
  EXPECT_TRUE(base::ReadFileToString(foobar_policy_file, &foobar_policy));
  EXPECT_EQ(foobar_policy, "serialized-foobar-policy-data");

  // Stale policies should be purged.
  EXPECT_FALSE(base::DirectoryExists(stale_poliy));
}

TEST(DMStorage, GetCachedPolicyInfo) {
  enterprise_management::PolicyData policy_data;
  policy_data.set_policy_value("SerializedProtobufDataFromPolicy");
  policy_data.set_policy_type("TestPolicyType1");
  policy_data.set_request_token("TestDMToken");
  policy_data.set_timestamp(12340000);
  policy_data.set_device_id("TestDMToken");
  policy_data.set_request_token("TestDMToken");

  std::string new_public_key = "SampleNewPublicKeyData";
  enterprise_management::PublicKeyVerificationData key_verif_data;
  key_verif_data.set_new_public_key(new_public_key);
  key_verif_data.set_new_public_key_version(15);

  enterprise_management::PolicyFetchResponse response;
  response.set_policy_data(policy_data.SerializeAsString());
  response.set_new_public_key(new_public_key);
  response.set_new_public_key_verification_data(
      key_verif_data.SerializeAsString());

  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());
  auto storage = base::MakeRefCounted<DMStorage>(
      cache_root.GetPath(), std::make_unique<TestTokenService>());
  EXPECT_TRUE(storage->PersistPolicies({
      {"sample-policy-type", response.SerializeAsString()},
  }));

  auto policy_info = storage->GetCachedPolicyInfo();
  ASSERT_NE(policy_info, nullptr);
  EXPECT_EQ(policy_info->public_key(), "SampleNewPublicKeyData");
  EXPECT_TRUE(policy_info->has_key_version());
  EXPECT_EQ(policy_info->key_version(), 15);
  EXPECT_EQ(policy_info->timestamp(), 12340000);
}

TEST(DMStorage, ReadCachedOmahaPolicy) {
  DMPolicyMap policies({
      {"google/machine-level-omaha", CannedOmahaPolicyFetchResponse()},
  });
  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());
  auto storage = base::MakeRefCounted<DMStorage>(
      cache_root.GetPath(), std::make_unique<TestTokenService>());
  EXPECT_TRUE(storage->PersistPolicies(policies));

  auto policy_manager = storage->GetOmahaPolicyManager();
  ASSERT_NE(policy_manager, nullptr);

  int check_interval = 0;
  EXPECT_TRUE(policy_manager->GetLastCheckPeriodMinutes(&check_interval));
  EXPECT_EQ(check_interval, 111);

  UpdatesSuppressedTimes suppressed_times;
  EXPECT_TRUE(policy_manager->GetUpdatesSuppressedTimes(&suppressed_times));
  EXPECT_EQ(suppressed_times.start_hour, 8);
  EXPECT_EQ(suppressed_times.start_minute, 8);
  EXPECT_EQ(suppressed_times.duration_minute, 47);

  // Proxy policies.
  std::string proxy_mode;
  EXPECT_TRUE(policy_manager->GetProxyMode(&proxy_mode));
  EXPECT_EQ(proxy_mode, "proxy_pac_script");
  std::string proxy_pac_url;
  EXPECT_TRUE(policy_manager->GetProxyPacUrl(&proxy_pac_url));
  EXPECT_EQ(proxy_pac_url, "foo.c/proxy.pa");
  std::string proxy_server;
  EXPECT_FALSE(policy_manager->GetProxyServer(&proxy_server));

  // Download preference.
  std::string download_preference;
  EXPECT_TRUE(
      policy_manager->GetDownloadPreferenceGroupPolicy(&download_preference));
  EXPECT_EQ(download_preference, "cacheable");

  // Cache policies.
  int cache_size = 0;
  EXPECT_FALSE(policy_manager->GetPackageCacheSizeLimitMBytes(&cache_size));
  int cache_life = 0;
  EXPECT_FALSE(policy_manager->GetPackageCacheExpirationTimeDays(&cache_life));

  // Chrome policies.
  int chrome_install_policy = -1;
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppInstalls(
      kChromeAppId, &chrome_install_policy));
  EXPECT_EQ(chrome_install_policy, kPolicyDisabled);
  int chrome_update_policy = -1;
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppUpdates(
      kChromeAppId, &chrome_update_policy));
  EXPECT_EQ(chrome_update_policy, kPolicyAutomaticUpdatesOnly);
  std::string target_version_prefix;
  EXPECT_TRUE(policy_manager->GetTargetVersionPrefix(kChromeAppId,
                                                     &target_version_prefix));
  EXPECT_EQ(target_version_prefix, "3.6.55");
  bool rollback_allowed = false;
  EXPECT_TRUE(policy_manager->IsRollbackToTargetVersionAllowed(
      kChromeAppId, &rollback_allowed));
  EXPECT_TRUE(rollback_allowed);

  // No app-specific policy should fallback to global.
  const std::string non_exist_appid = "{00000000-1111-2222-3333-444444444444}";
  int app_install_policy = -1;
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppInstalls(
      non_exist_appid, &app_install_policy));
  EXPECT_EQ(app_install_policy, kPolicyDisabled);
  int app_update_policy = -1;
  EXPECT_TRUE(policy_manager->GetEffectivePolicyForAppUpdates(
      non_exist_appid, &app_update_policy));
  EXPECT_EQ(app_update_policy, kPolicyManualUpdatesOnly);
  std::string app_target_version_prefix;
  EXPECT_FALSE(policy_manager->GetTargetVersionPrefix(
      non_exist_appid, &app_target_version_prefix));
  bool app_rollback_allowed = false;
  EXPECT_FALSE(policy_manager->IsRollbackToTargetVersionAllowed(
      non_exist_appid, &app_rollback_allowed));

  // Verify no policy manager once device is deregistered.
  EXPECT_TRUE(storage->DeregisterDevice());
  EXPECT_FALSE(storage->IsValidDMToken());
  ASSERT_EQ(storage->GetOmahaPolicyManager(), nullptr);
}

}  // namespace updater
