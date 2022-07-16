// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>
#include <vector>

#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class AppRegistryCacheTest : public testing::Test {
 public:
  std::unique_ptr<App> MakeApp(const char* app_id,
                               const char* name,
                               Readiness readiness = Readiness::kUnknown,
                               uint64_t timeline = 0) {
    std::unique_ptr<App> app = std::make_unique<App>(AppType::kArc, app_id);
    app->readiness = readiness;
    app->name = name;
    app->icon_key = IconKey(timeline, /*resource_id=*/0, /*icon_effects=*/0);
    return app;
  }

  void CallForAllApps(AppRegistryCache& cache) {
    cache.ForAllApps([this](const AppUpdate& update) { OnAppUpdate(update); });
  }

  void OnAppUpdate(const AppUpdate& update) {
    EXPECT_NE("", update.GetName());
    if (!apps_util::IsInstalled(update.GetReadiness())) {
      return;
    }
    updated_ids_.insert(update.GetAppId());
    updated_names_.insert(update.GetName());
  }

  std::string GetName(const AppRegistryCache& cache,
                      const std::string& app_id) {
    std::string name;
    cache.ForApp(app_id,
                 [&name](const AppUpdate& update) { name = update.GetName(); });
    return name;
  }

  void VerifyApp(AppRegistryCache& cache,
                 const char* app_id,
                 const char* name,
                 Readiness readiness = Readiness::kUnknown,
                 uint64_t timeline = 0) {
    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    ASSERT_TRUE(cache.states_[app_id]->name.has_value());
    EXPECT_EQ(name, GetName(cache, app_id));
    EXPECT_EQ(readiness, cache.states_[app_id]->readiness);
    if (timeline != 0) {
      ASSERT_TRUE(cache.states_[app_id]->icon_key.has_value());
      EXPECT_EQ(timeline, cache.states_[app_id]->icon_key.value().timeline);
    }
  }

  int AppCount(const AppRegistryCache& cache) { return cache.states_.size(); }

  void Clear() {
    updated_ids_.clear();
    updated_names_.clear();
  }

  std::set<std::string> updated_ids_;
  std::set<std::string> updated_names_;
};

TEST_F(AppRegistryCacheTest, OnApps) {
  AppRegistryCache cache;
  std::vector<std::unique_ptr<App>> deltas;
  deltas.push_back(MakeApp("a", "apple"));
  deltas.push_back(MakeApp("b", "banana", Readiness::kReady));
  deltas.push_back(MakeApp("c", "cherry", Readiness::kDisabledByPolicy,
                           /*timeline=*/10));
  cache.OnApps(std::move(deltas), AppType::kUnknown,
               false /* should_notify_initialized */);

  EXPECT_EQ(3, AppCount(cache));
  VerifyApp(cache, "a", "apple");
  VerifyApp(cache, "b", "banana", Readiness::kReady);
  VerifyApp(cache, "c", "cherry", Readiness::kDisabledByPolicy,
            /*timeline=*/10);

  CallForAllApps(cache);
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_EQ(2u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("banana"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("cherry"));
  Clear();

  deltas.clear();
  deltas.push_back(MakeApp("a", "apricot", Readiness::kReady));
  deltas.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas), AppType::kUnknown,
               false /* should_notify_initialized */);

  EXPECT_EQ(4, AppCount(cache));
  VerifyApp(cache, "a", "apricot", Readiness::kReady);
  VerifyApp(cache, "b", "banana", Readiness::kReady);
  VerifyApp(cache, "c", "cherry", Readiness::kDisabledByPolicy,
            /*timeline=*/10);
  VerifyApp(cache, "d", "durian");

  CallForAllApps(cache);
  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_EQ(3u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("apricot"));
  Clear();
}

TEST_F(AppRegistryCacheTest, Removed) {
  AppRegistryCache cache;

  std::vector<std::unique_ptr<App>> apps;
  apps.push_back(MakeApp("app", "app", Readiness::kReady));
  cache.OnApps(std::move(apps), AppType::kUnknown,
               false /* should_notify_initialized */);

  CallForAllApps(cache);
  EXPECT_EQ(1u, updated_ids_.size());
  EXPECT_EQ(1u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("app"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("app"));
  Clear();

  // Uninstall the app, then remove it.
  apps.clear();
  apps.push_back(MakeApp("app", "app", Readiness::kUninstalledByUser));
  apps.push_back(MakeApp("app", "app", Readiness::kRemoved));
  cache.OnApps(std::move(apps), AppType::kUnknown,
               false /* should_notify_initialized */);

  // The cache is now empty.
  EXPECT_EQ(0, AppCount(cache));
  CallForAllApps(cache);
  EXPECT_TRUE(updated_ids_.empty());
  EXPECT_TRUE(updated_names_.empty());
  Clear();
}

}  // namespace apps
