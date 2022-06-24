// Copyright 2019 The Chromium Authors. All rights reserved.
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

  // Ensure that OnAppQuit with no profiles installed is a no-op.
  profiles.insert(profile_path_a);
  registry_->OnAppQuit(app_id_a, profiles);
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

  // Test OnAppQuit with a valid profile.
  profiles.clear();
  profiles.insert(profile_path_b);
  registry_->OnAppQuit(app_id_a, profiles);
  profiles = registry_->GetLastActiveProfilesForApp(app_id_a);
  EXPECT_EQ(profiles.size(), 1u);
  EXPECT_TRUE(profiles.count(profile_path_b));

  // Test OnAppQuit with an invalid profile.
  profiles.clear();
  profiles.insert(profile_path_c);
  registry_->OnAppQuit(app_id_a, profiles);
  profiles = registry_->GetLastActiveProfilesForApp(app_id_a);
  EXPECT_EQ(0u, registry_->GetLastActiveProfilesForApp(app_id_a).size());

  // Test OnAppQuit with a valid and invalid profile. The invalid profile
  // should be discarded.
  profiles.clear();
  profiles.insert(profile_path_a);
  profiles.insert(profile_path_c);
  registry_->OnAppQuit(app_id_a, profiles);
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

}  // namespace
