// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "chrome/updater/device_management/dm_cached_policy_info.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "chrome/updater/util/unittest_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

constexpr char kTestDmToken[] = "TestDMToken";

class TestTokenService : public TokenServiceInterface {
 public:
  TestTokenService()
      : enrollment_token_("TestEnrollmentToken"), dm_token_(kTestDmToken) {}
  ~TestTokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return "TestDeviceID"; }

  bool IsEnrollmentMandatory() const override { return false; }

  bool StoreEnrollmentToken(const std::string& enrollment_token) override {
    enrollment_token_ = enrollment_token;
    return true;
  }

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
      ::wireless_android_enterprise_devicemanagement::INSTALL_DISABLED);
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

#if BUILDFLAG(IS_MAC)

TEST(DMStorage, LoadDeviceID) {
  auto storage = base::MakeRefCounted<DMStorage>(
      base::FilePath(FILE_PATH_LITERAL("/TestPolicyCacheRoot")));
  EXPECT_FALSE(storage->GetDeviceID().empty());
}

#endif  // BUILDFLAG(IS_MAC)

TEST(DMStorage, DMToken) {
  base::ScopedTempDir cache_root;
  ASSERT_TRUE(cache_root.CreateUniqueTempDir());
  auto storage = base::MakeRefCounted<DMStorage>(
      cache_root.GetPath(), std::make_unique<TestTokenService>());
  EXPECT_TRUE(storage->IsValidDMToken());
  EXPECT_FALSE(storage->GetDmToken().empty());
  EXPECT_FALSE(storage->IsDeviceDeregistered());

  // Deregister using DM token invalidation.
  storage->InvalidateDMToken();
  EXPECT_FALSE(storage->IsValidDMToken());
  EXPECT_FALSE(storage->GetDmToken().empty());
  EXPECT_TRUE(storage->IsDeviceDeregistered());

  storage->StoreDmToken(kTestDmToken);
  EXPECT_TRUE(storage->IsValidDMToken());
  EXPECT_FALSE(storage->GetDmToken().empty());
  EXPECT_FALSE(storage->IsDeviceDeregistered());

  // Deregister using DM token deletion.
  storage->DeleteDMToken();
  EXPECT_FALSE(storage->IsValidDMToken());
  EXPECT_TRUE(storage->GetDmToken().empty());
  // Although the device is deregistered, it is not treated as deregistered due
  // to potential re-registration. Instead, it is treated as having an empty DM
  // token.
  EXPECT_FALSE(storage->IsDeviceDeregistered());
}

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
  EXPECT_TRUE(storage->CanPersistPolicies());
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
  policy_data.set_request_token(kTestDmToken);
  policy_data.set_timestamp(12340000);
  policy_data.set_device_id(kTestDmToken);
  policy_data.set_request_token(kTestDmToken);

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
  EXPECT_TRUE(storage->CanPersistPolicies());
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
  EXPECT_TRUE(storage->CanPersistPolicies());
  EXPECT_TRUE(storage->PersistPolicies(policies));

  std::unique_ptr<
      ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto>
      omaha_settings = storage->GetOmahaPolicySettings();
  ASSERT_NE(omaha_settings, nullptr);
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
  ASSERT_EQ(storage->GetOmahaPolicySettings(), nullptr);
}

}  // namespace updater
