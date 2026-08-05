// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_paths.h"
#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/test_support/fake_iwa_runtime_data_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

// Creates a dictionary to represent a kiosk account for testing.
base::DictValue CreateIwaKioskAccountDict(
    const policy::DeviceLocalAccount& account) {
  return base::DictValue()
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId, account.account_id)
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
           static_cast<int>(account.type))
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
           static_cast<int>(account.ephemeral_mode))
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskBundleId,
           account.kiosk_iwa_info.web_bundle_id())
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskUpdateUrl,
           account.kiosk_iwa_info.update_manifest_url())
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskUpdateChannel,
           account.kiosk_iwa_info.update_channel())
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskPinnedVersion,
           account.kiosk_iwa_info.pinned_version())
      .Set(ash::kAccountsPrefDeviceLocalAccountsKeyIwaKioskAllowDowngrades,
           account.kiosk_iwa_info.allow_downgrades());
}

void SetKioskAccounts(ash::ScopedTestingCrosSettings& cros_settings,
                      const std::vector<policy::DeviceLocalAccount>& accounts) {
  base::ListValue accounts_list;
  for (const auto& account : accounts) {
    CHECK_EQ(account.type,
             policy::DeviceLocalAccountType::kKioskIsolatedWebApp);
    accounts_list.Append(CreateIwaKioskAccountDict(account));
  }

  cros_settings.device_settings()->Set(ash::kAccountsPrefDeviceLocalAccounts,
                                       base::Value(std::move(accounts_list)));
}

policy::DeviceLocalAccount CreateIwaKioskAccount(
    const web_package::SignedWebBundleId& bundle_id,
    std::string_view pinned_version,
    bool allow_downgrades = false,
    std::string_view account_id = "account_id",
    std::string_view update_manifest_url = "https://example.com/update.json") {
  return policy::DeviceLocalAccount(
      policy::DeviceLocalAccount::EphemeralMode::kEnable,
      policy::IsolatedWebAppKioskBasicInfo(
          bundle_id.id(), std::string(update_manifest_url),
          /*update_channel=*/"", std::string(pinned_version), allow_downgrades),
      std::string(account_id));
}

}  // namespace

class IwaBundleCacheManagerTest : public WebAppTest {
 public:
  IwaBundleCacheManagerTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        runtime_data_provider_reset_(
            IwaRuntimeDataProvider::SetInstanceForTesting(
                &fake_runtime_data_provider_)) {}

  void SetUp() override {
    ASSERT_TRUE(cache_root_dir_.CreateUniqueTempDir());
    cache_root_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE, cache_root_dir_.GetPath());

    WebAppTest::SetUp();

    fake_runtime_data_provider_.Update(
        [this](FakeIwaRuntimeDataProvider::ScopedIwaRuntimeDataUpdate& update) {
          update.AddToManagedAllowlist(kWebBundleId1);
          update.AddToManagedAllowlist(kWebBundleId2);
        });
  }

 protected:
  void CreateKioskCacheDir(const web_package::SignedWebBundleId& bundle_id) {
    ASSERT_TRUE(base::CreateDirectory(
        cache_root_dir_.GetPath().AppendASCII("kiosk").AppendASCII(
            bundle_id.id())));
  }

  bool IsBundleCachedInKioskDir(
      const web_package::SignedWebBundleId& bundle_id) {
    return base::PathExists(
        cache_root_dir_.GetPath().AppendASCII("kiosk").AppendASCII(
            bundle_id.id()));
  }

  WebAppProvider& provider() { return *FakeWebAppProvider::Get(profile()); }

  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;

  const web_package::SignedWebBundleId kWebBundleId1 =
      web_package::SignedWebBundleId::CreateRandomForProxyMode();
  const web_package::SignedWebBundleId kWebBundleId2 =
      web_package::SignedWebBundleId::CreateRandomForProxyMode();

  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;

  FakeIwaRuntimeDataProvider fake_runtime_data_provider_;
  base::AutoReset<IwaRuntimeDataProvider*> runtime_data_provider_reset_;
};

TEST_F(IwaBundleCacheManagerTest, PolicyChangeClearsCache) {
  CreateKioskCacheDir(kWebBundleId1);
  SetKioskAccounts(
      scoped_testing_cros_settings_,
      {CreateIwaKioskAccount(kWebBundleId1, /*pinned_version=*/"1.0.0")});

  test::AwaitStartWebAppProviderAndSubsystems(profile());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(IsBundleCachedInKioskDir(kWebBundleId1));

  SetKioskAccounts(
      scoped_testing_cros_settings_,
      {CreateIwaKioskAccount(kWebBundleId1, /*pinned_version=*/"2.0.0")});
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(IsBundleCachedInKioskDir(kWebBundleId1));
}

TEST_F(IwaBundleCacheManagerTest, PolicyUpdateManifestUrlChangeClearsCache) {
  CreateKioskCacheDir(kWebBundleId1);
  SetKioskAccounts(
      scoped_testing_cros_settings_,
      {CreateIwaKioskAccount(kWebBundleId1, /*pinned_version=*/"1.0.0",
                             /*allow_downgrades=*/false,
                             /*account_id=*/"account_id",
                             /*update_manifest_url=*/
                             "https://example.com/update_v1.json")});

  test::AwaitStartWebAppProviderAndSubsystems(profile());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(IsBundleCachedInKioskDir(kWebBundleId1));

  SetKioskAccounts(
      scoped_testing_cros_settings_,
      {CreateIwaKioskAccount(kWebBundleId1, /*pinned_version=*/"1.0.0",
                             /*allow_downgrades=*/false,
                             /*account_id=*/"account_id",
                             /*update_manifest_url=*/
                             "https://example.com/update_v2.json")});
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(IsBundleCachedInKioskDir(kWebBundleId1));
}

TEST_F(IwaBundleCacheManagerTest, PolicyAllowDowngradesChangeClearsCache) {
  CreateKioskCacheDir(kWebBundleId1);
  SetKioskAccounts(
      scoped_testing_cros_settings_,
      {CreateIwaKioskAccount(kWebBundleId1, /*pinned_version=*/"1.0.0",
                             /*allow_downgrades=*/false)});

  test::AwaitStartWebAppProviderAndSubsystems(profile());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(IsBundleCachedInKioskDir(kWebBundleId1));

  SetKioskAccounts(
      scoped_testing_cros_settings_,
      {CreateIwaKioskAccount(kWebBundleId1, /*pinned_version=*/"1.0.0",
                             /*allow_downgrades=*/true)});
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(IsBundleCachedInKioskDir(kWebBundleId1));
}

TEST_F(IwaBundleCacheManagerTest, MultipleAppsPolicyChange) {
  CreateKioskCacheDir(kWebBundleId1);
  CreateKioskCacheDir(kWebBundleId2);
  SetKioskAccounts(
      scoped_testing_cros_settings_,
      {CreateIwaKioskAccount(kWebBundleId1, /*pinned_version=*/"1.0.0",
                             /*allow_downgrades=*/false,
                             /*account_id=*/"account_id_1"),
       CreateIwaKioskAccount(kWebBundleId2, /*pinned_version=*/"1.0.0",
                             /*allow_downgrades=*/false,
                             /*account_id=*/"account_id_2")});

  test::AwaitStartWebAppProviderAndSubsystems(profile());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(IsBundleCachedInKioskDir(kWebBundleId1));
  EXPECT_TRUE(IsBundleCachedInKioskDir(kWebBundleId2));

  SetKioskAccounts(
      scoped_testing_cros_settings_,
      {CreateIwaKioskAccount(kWebBundleId1, /*pinned_version=*/"2.0.0",
                             /*allow_downgrades=*/false,
                             /*account_id=*/"account_id_1"),
       CreateIwaKioskAccount(kWebBundleId2, /*pinned_version=*/"1.0.0",
                             /*allow_downgrades=*/false,
                             /*account_id=*/"account_id_2")});
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(IsBundleCachedInKioskDir(kWebBundleId1));
  EXPECT_TRUE(IsBundleCachedInKioskDir(kWebBundleId2));
}

TEST_F(IwaBundleCacheManagerTest, StartupClearsCacheIfLocalStatePolicyDiffers) {
  CreateKioskCacheDir(kWebBundleId1);

  // Populate local_state with an old policy state (pinned_version = "1.0.0").
  base::DictValue old_info;
  old_info.Set("update_manifest_url", "https://example.com/update.json");
  old_info.Set("pinned_version", "1.0.0");
  old_info.Set("allow_downgrades", false);
  old_info.Set("update_channel", "");

  base::DictValue local_state_dict;
  local_state_dict.Set(kWebBundleId1.id(), std::move(old_info));
  g_browser_process->local_state()->SetDict(prefs::kKioskIwaCachePolicyState,
                                            std::move(local_state_dict));

  // Configure CrosSettings policy with a new pinned_version = "2.0.0".
  SetKioskAccounts(
      scoped_testing_cros_settings_,
      {CreateIwaKioskAccount(kWebBundleId1, /*pinned_version=*/"2.0.0")});

  // Start the manager. It loads old policy from local_state, compares with
  // CrosSettings, detects the difference, and clears the cache.
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(IsBundleCachedInKioskDir(kWebBundleId1));

  // Verify local_state was updated to the new policy state.
  const base::DictValue& updated_dict =
      g_browser_process->local_state()->GetDict(
          prefs::kKioskIwaCachePolicyState);
  const base::DictValue* app_dict = updated_dict.FindDict(kWebBundleId1.id());
  ASSERT_TRUE(app_dict);
  EXPECT_EQ(*app_dict->FindString("pinned_version"), "2.0.0");
}

}  // namespace web_app
