// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestAppShimRegistry : public AppShimRegistry {
 public:
  explicit TestAppShimRegistry(PrefService* pref_service) : AppShimRegistry() {
    SetPrefServiceAndUserDataDirForTesting(pref_service,
                                           base::FilePath("/x/y/z"));
  }
  ~TestAppShimRegistry() = default;
};

class AppShimRegistryTest : public testing::Test {
 public:
  AppShimRegistryTest() = default;
  ~AppShimRegistryTest() override = default;
  AppShimRegistryTest(const AppShimRegistryTest&) = delete;
  AppShimRegistryTest& operator=(const AppShimRegistryTest&) = delete;

  void SetUp() override {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    registry_ = std::make_unique<TestAppShimRegistry>(local_state_.get());
    registry_->RegisterLocalPrefs(local_state_->registry());
    OSCryptMocker::SetUp();
  }
  void TearDown() override { OSCryptMocker::TearDown(); }

 protected:
  os_crypt_async::Encryptor GetEncryptor() {
    base::test::TestFuture<os_crypt_async::Encryptor> future;
    g_browser_process->os_crypt_async()->GetInstance(future.GetCallback());
    return future.Take();
  }

  bool VerifyCdHashForAppSync(const std::string& app_id,
                              base::span<const uint8_t> cd_hash) {
    base::test::TestFuture<bool> future;
    registry_->VerifyCdHashForApp(app_id, cd_hash, future.GetCallback());
    return future.Get();
  }

  void SaveCdHashForAppSync(const std::string& app_id,
                            base::span<const uint8_t> cd_hash) {
    base::test::TestFuture<void> future;
    registry_->SaveCdHashForApp(app_id, cd_hash, future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<TestAppShimRegistry> registry_;
};

TEST_F(AppShimRegistryTest, Lifetime) {
  const std::string app_id_a("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const std::string app_id_b("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  base::FilePath profile_path_a("/x/y/z/Profile A");
  base::FilePath profile_path_b("/x/y/z/Profile B");
  base::FilePath profile_path_c("/x/y/z/Profile C");
  std::set<base::FilePath> profiles;

  EXPECT_EQ(0u, registry_->GetInstalledProfilesForApp(app_id_a).size());
  EXPECT_EQ(0u, registry_->GetInstalledProfilesForApp(app_id_b).size());

  // Ensure that OnAppUninstalledForProfile with no profiles installed is a
  // no-op, and reports that the app is installed for no profiles.
  EXPECT_TRUE(registry_->OnAppUninstalledForProfile(app_id_a, profile_path_a));
  EXPECT_EQ(0u, registry_->GetInstalledProfilesForApp(app_id_a).size());

  // Ensure that SaveLastActiveProfilesForApp with no profiles installed is a
  // no-op.
  profiles.insert(profile_path_a);
  registry_->SaveLastActiveProfilesForApp(app_id_a, profiles);
  EXPECT_EQ(0u, registry_->GetInstalledProfilesForApp(app_id_a).size());
  EXPECT_EQ(0u, registry_->GetLastActiveProfilesForApp(app_id_a).size());

  // Test installing for profile a.
  registry_->OnAppInstalledForProfile(app_id_a, profile_path_a);
  profiles = registry_->GetInstalledProfilesForApp(app_id_a);
  EXPECT_EQ(profiles.size(), 1u);
  EXPECT_TRUE(profiles.count(profile_path_a));
  EXPECT_EQ(0u, registry_->GetInstalledProfilesForApp(app_id_b).size());

  // And installing for profile b.
  registry_->OnAppInstalledForProfile(app_id_a, profile_path_b);
  profiles = registry_->GetInstalledProfilesForApp(app_id_a);
  EXPECT_EQ(profiles.size(), 2u);
  EXPECT_TRUE(profiles.count(profile_path_a));
  EXPECT_TRUE(profiles.count(profile_path_b));
  EXPECT_EQ(0u, registry_->GetInstalledProfilesForApp(app_id_b).size());

  // Test SaveLastActiveProfilesForApp with a valid profile.
  profiles.clear();
  profiles.insert(profile_path_b);
  registry_->SaveLastActiveProfilesForApp(app_id_a, profiles);
  profiles = registry_->GetLastActiveProfilesForApp(app_id_a);
  EXPECT_EQ(profiles.size(), 1u);
  EXPECT_TRUE(profiles.count(profile_path_b));

  // Test SaveLastActiveProfilesForApp with an invalid profile.
  profiles.clear();
  profiles.insert(profile_path_c);
  registry_->SaveLastActiveProfilesForApp(app_id_a, profiles);
  profiles = registry_->GetLastActiveProfilesForApp(app_id_a);
  EXPECT_EQ(0u, registry_->GetLastActiveProfilesForApp(app_id_a).size());

  // Test SaveLastActiveProfilesForApp with a valid and invalid profile. The
  // invalid profile should be discarded.
  profiles.clear();
  profiles.insert(profile_path_a);
  profiles.insert(profile_path_c);
  registry_->SaveLastActiveProfilesForApp(app_id_a, profiles);
  profiles = registry_->GetLastActiveProfilesForApp(app_id_a);
  EXPECT_EQ(profiles.size(), 1u);
  EXPECT_TRUE(profiles.count(profile_path_a));

  // Uninstall for profile a. It should return false because it is still
  // installed for profile b. The list of last active profiles should now
  // be empty.
  EXPECT_FALSE(registry_->OnAppUninstalledForProfile(app_id_a, profile_path_a));
  EXPECT_EQ(0u, registry_->GetLastActiveProfilesForApp(app_id_a).size());
  profiles = registry_->GetInstalledProfilesForApp(app_id_a);
  EXPECT_EQ(profiles.size(), 1u);
  EXPECT_TRUE(profiles.count(profile_path_b));

  // Uninstall for profile b. It should return true because all profiles are
  // gone.
  EXPECT_TRUE(registry_->OnAppUninstalledForProfile(app_id_a, profile_path_b));
  EXPECT_EQ(0u, registry_->GetInstalledProfilesForApp(app_id_a).size());
  EXPECT_EQ(0u, registry_->GetLastActiveProfilesForApp(app_id_a).size());
}

TEST_F(AppShimRegistryTest, InstalledAppsForProfile) {
  const std::string app_id_a("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const std::string app_id_b("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  const base::FilePath profile_path_a("/x/y/z/Profile A");
  const base::FilePath profile_path_b("/x/y/z/Profile B");
  const base::FilePath profile_path_c("/x/y/z/Profile C");
  std::set<std::string> apps;

  // App A is installed for profiles B and C.
  registry_->OnAppInstalledForProfile(app_id_a, profile_path_b);
  registry_->OnAppInstalledForProfile(app_id_a, profile_path_c);
  EXPECT_EQ(2u, registry_->GetInstalledProfilesForApp(app_id_a).size());
  apps = registry_->GetInstalledAppsForProfile(profile_path_a);
  EXPECT_TRUE(apps.empty());
  apps = registry_->GetInstalledAppsForProfile(profile_path_b);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ(1u, apps.count(app_id_a));
  apps = registry_->GetInstalledAppsForProfile(profile_path_c);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ(1u, apps.count(app_id_a));

  // App B is installed for profiles A and C.
  registry_->OnAppInstalledForProfile(app_id_b, profile_path_a);
  registry_->OnAppInstalledForProfile(app_id_b, profile_path_c);
  apps = registry_->GetInstalledAppsForProfile(profile_path_a);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ(1u, apps.count(app_id_b));
  apps = registry_->GetInstalledAppsForProfile(profile_path_b);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ(1u, apps.count(app_id_a));
  apps = registry_->GetInstalledAppsForProfile(profile_path_c);
  EXPECT_EQ(2u, apps.size());
  EXPECT_EQ(1u, apps.count(app_id_a));
  EXPECT_EQ(1u, apps.count(app_id_b));

  // Uninstall app A for profile B.
  EXPECT_FALSE(registry_->OnAppUninstalledForProfile(app_id_a, profile_path_b));
  apps = registry_->GetInstalledAppsForProfile(profile_path_b);
  EXPECT_TRUE(apps.empty());
  apps = registry_->GetInstalledAppsForProfile(profile_path_c);
  EXPECT_EQ(2u, apps.size());
  EXPECT_EQ(1u, apps.count(app_id_a));
  EXPECT_EQ(1u, apps.count(app_id_b));

  // Uninstall app A for profile C.
  EXPECT_TRUE(registry_->OnAppUninstalledForProfile(app_id_a, profile_path_c));
  apps = registry_->GetInstalledAppsForProfile(profile_path_c);
  EXPECT_EQ(1u, apps.size());
  EXPECT_EQ(1u, apps.count(app_id_b));

  // Uninstall app B for profile C.
  EXPECT_FALSE(registry_->OnAppUninstalledForProfile(app_id_b, profile_path_c));
  apps = registry_->GetInstalledAppsForProfile(profile_path_c);
  EXPECT_TRUE(apps.empty());
}

TEST_F(AppShimRegistryTest, FileHandlers) {
  const std::string app_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  base::FilePath profile_path_a("/x/y/z/Profile A");
  base::FilePath profile_path_b("/x/y/z/Profile B");

  std::set<std::string> extensions;
  extensions.insert(".jpg");
  extensions.insert(".jpeg");
  std::set<std::string> mime_types;
  mime_types.insert("text/plain");

  auto handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_TRUE(handlers.empty());

  // Updating handlers for an app that isn't installed in any profile should be
  // a noop.
  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_a,
                                              extensions, mime_types);
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_TRUE(handlers.empty());

  // Install app A in profile B.
  registry_->OnAppInstalledForProfile(app_id, profile_path_b);

  // Verify updating handlers in profile A.
  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_a,
                                              extensions, mime_types);
  handlers = registry_->GetHandlersForApp(app_id);
  ASSERT_EQ(1u, handlers.size());
  EXPECT_EQ(profile_path_a, handlers.begin()->first);
  EXPECT_EQ(extensions, handlers.begin()->second.file_handler_extensions);
  EXPECT_EQ(mime_types, handlers.begin()->second.file_handler_mime_types);

  // Also update handlers in profile B.
  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_b,
                                              extensions, mime_types);
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_EQ(2u, handlers.size());
  EXPECT_EQ(1u, handlers.count(profile_path_a));
  EXPECT_EQ(1u, handlers.count(profile_path_b));

  // Verify updating handlers to be empty removes them.
  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_a, {}, {});
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_EQ(1u, handlers.size());
  EXPECT_EQ(1u, handlers.count(profile_path_b));

  // Only setting mime types to empty should not remove extensions and vice
  // versa.
  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_b,
                                              extensions, {});
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_EQ(1u, handlers.size());
  EXPECT_EQ(1u, handlers.count(profile_path_b));
  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_b, {},
                                              mime_types);
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_EQ(1u, handlers.size());
  EXPECT_EQ(1u, handlers.count(profile_path_b));

  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_b, {}, {});
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_TRUE(handlers.empty());

  // Verify that updating protocol handlers does not change file handlers.
  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_a,
                                              extensions, mime_types);
  registry_->SaveProtocolHandlersForAppAndProfile(app_id, profile_path_a,
                                                  {"ftp"});
  handlers = registry_->GetHandlersForApp(app_id);
  ASSERT_EQ(1u, handlers.size());
  EXPECT_EQ(profile_path_a, handlers.begin()->first);
  EXPECT_EQ(extensions, handlers.begin()->second.file_handler_extensions);
  EXPECT_EQ(mime_types, handlers.begin()->second.file_handler_mime_types);

  registry_->SaveProtocolHandlersForAppAndProfile(app_id, profile_path_a, {});
  handlers = registry_->GetHandlersForApp(app_id);
  ASSERT_EQ(1u, handlers.size());
  EXPECT_EQ(profile_path_a, handlers.begin()->first);
  EXPECT_EQ(extensions, handlers.begin()->second.file_handler_extensions);
  EXPECT_EQ(mime_types, handlers.begin()->second.file_handler_mime_types);

  // Verify uninstalling an app also removes handler information.
  EXPECT_FALSE(registry_->GetHandlersForApp(app_id).empty());
  EXPECT_TRUE(registry_->OnAppUninstalledForProfile(app_id, profile_path_b));
  EXPECT_TRUE(registry_->GetHandlersForApp(app_id).empty());
}

TEST_F(AppShimRegistryTest, ProtocolHandlers) {
  const std::string app_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  base::FilePath profile_path_a("/x/y/z/Profile A");
  base::FilePath profile_path_b("/x/y/z/Profile B");

  std::set<std::string> protocols;
  protocols.insert("myprotocol");

  auto handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_TRUE(handlers.empty());

  // Updating handlers for an app that isn't installed in any profile should be
  // a noop.
  registry_->SaveProtocolHandlersForAppAndProfile(app_id, profile_path_a,
                                                  protocols);
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_TRUE(handlers.empty());

  // Install app A in profile B.
  registry_->OnAppInstalledForProfile(app_id, profile_path_b);

  // Verify updating handlers in profile A.
  registry_->SaveProtocolHandlersForAppAndProfile(app_id, profile_path_a,
                                                  protocols);
  handlers = registry_->GetHandlersForApp(app_id);
  ASSERT_EQ(1u, handlers.size());
  EXPECT_EQ(profile_path_a, handlers.begin()->first);
  EXPECT_EQ(protocols, handlers.begin()->second.protocol_handlers);

  // Also update handlers in profile B.
  registry_->SaveProtocolHandlersForAppAndProfile(app_id, profile_path_b,
                                                  protocols);
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_EQ(2u, handlers.size());
  EXPECT_EQ(1u, handlers.count(profile_path_a));
  EXPECT_EQ(1u, handlers.count(profile_path_b));

  // Verify updating handlers to be empty removes them.
  registry_->SaveProtocolHandlersForAppAndProfile(app_id, profile_path_a, {});
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_EQ(1u, handlers.size());
  EXPECT_EQ(1u, handlers.count(profile_path_b));
  registry_->SaveProtocolHandlersForAppAndProfile(app_id, profile_path_b, {});
  handlers = registry_->GetHandlersForApp(app_id);
  EXPECT_TRUE(handlers.empty());

  // Verify that updating file handlers does not change protocol handlers.
  registry_->SaveProtocolHandlersForAppAndProfile(app_id, profile_path_a,
                                                  protocols);
  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_a, {".jpg"},
                                              {});
  handlers = registry_->GetHandlersForApp(app_id);
  ASSERT_EQ(1u, handlers.size());
  EXPECT_EQ(profile_path_a, handlers.begin()->first);
  EXPECT_EQ(protocols, handlers.begin()->second.protocol_handlers);

  registry_->SaveFileHandlersForAppAndProfile(app_id, profile_path_a, {}, {});
  handlers = registry_->GetHandlersForApp(app_id);
  ASSERT_EQ(1u, handlers.size());
  EXPECT_EQ(profile_path_a, handlers.begin()->first);
  EXPECT_EQ(protocols, handlers.begin()->second.protocol_handlers);

  // Verify uninstalling an app also removes handler information.
  EXPECT_TRUE(registry_->OnAppUninstalledForProfile(app_id, profile_path_b));
  EXPECT_TRUE(registry_->GetHandlersForApp(app_id).empty());
}

TEST_F(AppShimRegistryTest, CodeDirectoryHashes) {
  const std::string app_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const uint8_t cd_hash[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  const uint8_t other_cd_hash[] = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
  base::FilePath profile_path("/x/y/z/Profile");

  EXPECT_FALSE(VerifyCdHashForAppSync(app_id, cd_hash));

  // Saving code directory hash for an app that isn't in any profile should
  // be a noop.
  SaveCdHashForAppSync(app_id, cd_hash);
  EXPECT_FALSE(VerifyCdHashForAppSync(app_id, cd_hash));

  // Install app in profile.
  registry_->OnAppInstalledForProfile(app_id, profile_path);
  EXPECT_FALSE(VerifyCdHashForAppSync(app_id, cd_hash));

  // Verify saving code directory hash.
  SaveCdHashForAppSync(app_id, cd_hash);
  EXPECT_TRUE(VerifyCdHashForAppSync(app_id, cd_hash));

  // Ensure that a different code directory hash is invalid for this app.
  EXPECT_FALSE(VerifyCdHashForAppSync(app_id, other_cd_hash));

  // Verify uninstalling an app removes its code directory hash.
  EXPECT_TRUE(registry_->OnAppUninstalledForProfile(app_id, profile_path));
  EXPECT_FALSE(VerifyCdHashForAppSync(app_id, cd_hash));
}

TEST_F(AppShimRegistryTest, CodeDirectoryHashesAsync) {
  const std::string app_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const uint8_t cd_hash[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  base::FilePath profile_path("/x/y/z/Profile");

  // Install app in profile.
  registry_->OnAppInstalledForProfile(app_id, profile_path);

  // Verify saving code directory hash and immediately verifying works.
  registry_->SaveCdHashForApp(app_id, cd_hash, base::DoNothing());
  EXPECT_TRUE(VerifyCdHashForAppSync(app_id, cd_hash));
}

TEST_F(AppShimRegistryTest, CodeDirectoryHashesInvalidData) {
  const std::string app_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const uint8_t cd_hash[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  base::FilePath profile_path("/x/y/z/Profile");

  // Install app in profile.
  registry_->OnAppInstalledForProfile(app_id, profile_path);
  SaveCdHashForAppSync(app_id, cd_hash);
  EXPECT_TRUE(VerifyCdHashForAppSync(app_id, cd_hash));

  // Overwrite the HMAC key with data that cannot be decoded as base64.
  local_state_->SetString("app_shims_cdhash_hmac_key",
                          "this-is-not-valid-base64");

  // Simulate a restart to clear the cached HMAC key.
  registry_ = std::make_unique<TestAppShimRegistry>(local_state_.get());

  // The existing code directory hash should fail to verify after altering the
  // HMAC key.
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(VerifyCdHashForAppSync(app_id, cd_hash));
    histogram_tester.ExpectBucketCount(
        "Apps.AppShimRegistry.HmacKeyStore.LoadResult",
        AppShimRegistry::GetHmacKeyResult::kBase64DecodeFailed, 1);
  }

  // Verify that saving the code directory hash again makes it possible to
  // verify the hash once more.
  SaveCdHashForAppSync(app_id, cd_hash);
  EXPECT_TRUE(VerifyCdHashForAppSync(app_id, cd_hash));

  // Overwrite the HMAC key with valid base64 data that cannot be decrypted
  // via OSCrypt.
  local_state_->SetString("app_shims_cdhash_hmac_key",
                          base::Base64Encode("this-is-not-a-valid-key"));

  // Simulate a restart.
  registry_ = std::make_unique<TestAppShimRegistry>(local_state_.get());

  // The existing code directory hash should fail to verify after altering the
  // HMAC key.
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(VerifyCdHashForAppSync(app_id, cd_hash));
    histogram_tester.ExpectBucketCount(
        "Apps.AppShimRegistry.HmacKeyStore.LoadResult",
        AppShimRegistry::GetHmacKeyResult::kDecryptFailed_Permanent, 1);
  }

  // Verify that saving the code directory hash again makes it possible to
  // verify the hash once more.
  SaveCdHashForAppSync(app_id, cd_hash);
  EXPECT_TRUE(VerifyCdHashForAppSync(app_id, cd_hash));
}

TEST_F(AppShimRegistryTest, CodeDirectoryHashesInvalidLength) {
  const std::string app_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const uint8_t cd_hash[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  base::FilePath profile_path("/x/y/z/Profile");

  // Install app in profile.
  registry_->OnAppInstalledForProfile(app_id, profile_path);
  SaveCdHashForAppSync(app_id, cd_hash);

  // Overwrite the HMAC key with a key that is the wrong length.
  std::string wrong_length_key = "wrong_length";
  std::string encrypted_wrong_length_key;
  auto encryptor = GetEncryptor();
  EXPECT_TRUE(
      encryptor.EncryptString(wrong_length_key, &encrypted_wrong_length_key));
  local_state_->SetString("app_shims_cdhash_hmac_key",
                          base::Base64Encode(encrypted_wrong_length_key));

  // Simulate a restart.
  registry_ = std::make_unique<TestAppShimRegistry>(local_state_.get());

  // The existing code directory hash should fail to verify after altering the
  // HMAC key.
  {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(VerifyCdHashForAppSync(app_id, cd_hash));
    histogram_tester.ExpectBucketCount(
        "Apps.AppShimRegistry.HmacKeyStore.LoadResult",
        AppShimRegistry::GetHmacKeyResult::kInvalidLength, 1);
  }
}

TEST_F(AppShimRegistryTest, CodeDirectoryHashesCaching) {
  const std::string app_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const uint8_t cd_hash[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  base::FilePath profile_path("/x/y/z/Profile");

  // Install app in profile.
  registry_->OnAppInstalledForProfile(app_id, profile_path);

  // The first time the key is requested, a new key will be generated, and
  // saved.
  {
    base::HistogramTester histogram_tester;
    SaveCdHashForAppSync(app_id, cd_hash);
    EXPECT_TRUE(VerifyCdHashForAppSync(app_id, cd_hash));
    histogram_tester.ExpectBucketCount(
        "Apps.AppShimRegistry.HmacKeyStore.LoadResult",
        AppShimRegistry::GetHmacKeyResult::kNotFound, 1);
    histogram_tester.ExpectBucketCount(
        "Apps.AppShimRegistry.HmacKeyStore.SaveResult",
        AppShimRegistry::SaveHmacKeyResult::kSuccess, 1);
  }

  // Simulate a restart.
  registry_ = std::make_unique<TestAppShimRegistry>(local_state_.get());

  // The first time the key is requested, it is loaded from prefs.
  {
    base::HistogramTester histogram_tester;
    SaveCdHashForAppSync(app_id, cd_hash);
    EXPECT_TRUE(VerifyCdHashForAppSync(app_id, cd_hash));
    histogram_tester.ExpectBucketCount(
        "Apps.AppShimRegistry.HmacKeyStore.LoadResult",
        AppShimRegistry::GetHmacKeyResult::kSuccess, 1);
  }
}

TEST_F(AppShimRegistryTest, CdHashKnownAnswers) {
  const std::string app_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const auto cd_hash = std::to_array<uint8_t>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
  base::FilePath profile_path("/x/y/z/Profile");

  const auto fixed_key = std::to_array<uint8_t>({
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
      0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
      0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  });

  const auto encrypted = *GetEncryptor().EncryptString(
      std::string(base::as_string_view(fixed_key)));
  local_state_->SetString("app_shims_cdhash_hmac_key",
                          base::Base64Encode(encrypted));

  // Pull the HMAC key back out and ensure it matches; this is required for the
  // known answers below to be correct.
  const auto decrypted = *GetEncryptor().DecryptData(*base::Base64Decode(
      local_state_->GetString("app_shims_cdhash_hmac_key")));
  EXPECT_EQ(base::as_byte_span(fixed_key), base::as_byte_span(decrypted));

  // Now save an app's CdHash, then pull the saved CdHash out of the backing
  // dict and check that the HMAC has the expected value:
  registry_->OnAppInstalledForProfile(app_id, profile_path);
  SaveCdHashForAppSync(app_id, cd_hash);

  const auto dict = registry_->AsDebugDict();
  const auto* app_dict = dict.FindDict(app_id);
  ASSERT_TRUE(app_dict);
  const auto* app_hmac = app_dict->FindString("cdhash_hmac");
  ASSERT_TRUE(app_hmac);

  // Since this test installed a fixed HMAC key above, and the data being signed
  // is also fixed, the base64ed HMAC should always exactly match this, which is
  // just base64(HMAC(fixed_key, cd_hash)).
  EXPECT_EQ(*app_hmac, "Do/4zxXTbETfH7WtoKyq+ffhSfgFt1M61QNE/YLB+bk=");
}

}  // namespace
