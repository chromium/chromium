// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_shim_registry_mac.h"

#include "base/memory/raw_ptr.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AppShimRegistryTest : public testing::Test {
 public:
  AppShimRegistryTest() = default;
  ~AppShimRegistryTest() override = default;
  AppShimRegistryTest(const AppShimRegistryTest&) = delete;
  AppShimRegistryTest& operator=(const AppShimRegistryTest&) = delete;

  void SetUp() override {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    registry_ = AppShimRegistry::Get();
    registry_->RegisterLocalPrefs(local_state_->registry());
    registry_->SetPrefServiceAndUserDataDirForTesting(local_state_.get(),
                                                      base::FilePath("/x/y/z"));
  }
  void TearDown() override {
    registry_->SetPrefServiceAndUserDataDirForTesting(nullptr,
                                                      base::FilePath());
  }

 protected:
  raw_ptr<AppShimRegistry> registry_ = nullptr;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
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

}  // namespace
