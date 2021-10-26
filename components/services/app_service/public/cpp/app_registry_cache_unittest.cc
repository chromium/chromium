// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>
#include <vector>

#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class AppRegistryCacheTest : public testing::Test {
 public:
  std::unique_ptr<apps::App> MakeApp(
      const char* app_id,
      const char* name,
      apps::Readiness readiness = apps::Readiness::kUnknown,
      uint64_t timeline = 0) {
    std::unique_ptr<apps::App> app =
        std::make_unique<apps::App>(apps::AppType::kArc, app_id);
    app->readiness = readiness;
    app->name = name;
    app->icon_key =
        apps::IconKey(timeline, /*resource_id=*/0, /*icon_effects=*/0);
    return app;
  }

  void VerifyApp(const char* app_id,
                 const char* name,
                 apps::Readiness readiness = apps::Readiness::kUnknown,
                 uint64_t timeline = 0) {
    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    ASSERT_TRUE(cache.states_[app_id]->name.has_value());
    EXPECT_EQ(name, cache.states_[app_id]->name.value());
    EXPECT_EQ(readiness, cache.states_[app_id]->readiness);
    if (timeline != 0) {
      ASSERT_TRUE(cache.states_[app_id]->icon_key.has_value());
      EXPECT_EQ(timeline, cache.states_[app_id]->icon_key.value().timeline);
    }
  }

  int AppCount() { return cache.states_.size(); }

  apps::AppRegistryCache cache;
};

TEST_F(AppRegistryCacheTest, OnApps) {
  std::vector<std::unique_ptr<App>> deltas;
  deltas.push_back(MakeApp("a", "apple"));
  deltas.push_back(MakeApp("b", "banana", apps::Readiness::kReady));
  deltas.push_back(MakeApp("c", "cherry", apps::Readiness::kDisabledByPolicy,
                           /*timeline=*/10));
  cache.OnApps(std::move(deltas), apps::AppType::kUnknown,
               false /* should_notify_initialized */);

  EXPECT_EQ(3, AppCount());
  VerifyApp("a", "apple");
  VerifyApp("b", "banana", apps::Readiness::kReady);
  VerifyApp("c", "cherry", apps::Readiness::kDisabledByPolicy, /*timeline=*/10);

  deltas.clear();
  deltas.push_back(MakeApp("a", "apricot", apps::Readiness::kReady));
  deltas.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas), apps::AppType::kUnknown,
               false /* should_notify_initialized */);

  EXPECT_EQ(4, AppCount());
  VerifyApp("a", "apricot", apps::Readiness::kReady);
  VerifyApp("b", "banana", apps::Readiness::kReady);
  VerifyApp("c", "cherry", apps::Readiness::kDisabledByPolicy, /*timeline=*/10);
  VerifyApp("d", "durian");
}

TEST_F(AppRegistryCacheTest, Removed) {
  apps::AppRegistryCache cache;

  std::vector<std::unique_ptr<App>> apps;
  apps.push_back(MakeApp("app", "app", apps::Readiness::kReady));
  cache.OnApps(std::move(apps), apps::AppType::kUnknown,
               false /* should_notify_initialized */);

  // Uninstall the app, then remove it.
  apps.clear();
  apps.push_back(MakeApp("app", "app", apps::Readiness::kUninstalledByUser));
  apps.push_back(MakeApp("app", "app", apps::Readiness::kRemoved));
  cache.OnApps(std::move(apps), apps::AppType::kUnknown,
               false /* should_notify_initialized */);

  // The cache is now empty.
  EXPECT_EQ(0, AppCount());
}

}  // namespace apps
