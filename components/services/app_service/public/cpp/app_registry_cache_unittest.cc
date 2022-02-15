// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_registry_cache.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

apps::AppPtr MakeApp(const char* app_id,
                     const char* name,
                     apps::AppType app_type = apps::AppType::kArc,
                     apps::Readiness readiness = apps::Readiness::kUnknown,
                     uint64_t timeline = 0) {
  auto app = std::make_unique<apps::App>(app_type, app_id);
  app->readiness = readiness;
  app->name = name;
  app->icon_key =
      apps::IconKey(timeline, /*resource_id=*/0, /*icon_effects=*/0);
  return app;
}

apps::mojom::AppPtr MakeMojomApp(
    const char* app_id,
    const char* name,
    apps::mojom::AppType app_type = apps::mojom::AppType::kArc) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = app_type;
  app->app_id = app_id;
  app->name = name;
  return app;
}

// InitializedObserver is used to test the OnAppTypeInitialized interface for
// AppRegistryCache::Observer.
class InitializedObserver : public apps::AppRegistryCache::Observer {
 public:
  explicit InitializedObserver(apps::AppRegistryCache* cache) {
    cache_ = cache;
    Observe(cache);
  }

  ~InitializedObserver() override = default;

  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& update) override {
    updated_ids_.insert(update.AppId());
  }

  void UpdateApps() {
    std::vector<AppPtr> deltas;
    deltas.push_back(MakeApp("n", "noodle", AppType::kArc));
    deltas.push_back(MakeApp("s", "salmon", AppType::kChromeApp));
    cache_->OnApps(std::move(deltas), AppType::kUnknown,
                   false /* should_notify_initialized */);

    std::vector<apps::mojom::AppPtr> mojom_deltas;
    mojom_deltas.push_back(
        MakeMojomApp("n", "noodle", apps::mojom::AppType::kArc));
    mojom_deltas.push_back(
        MakeMojomApp("s", "salmon", apps::mojom::AppType::kChromeApp));
    cache_->OnApps(std::move(mojom_deltas), apps::mojom::AppType::kUnknown,
                   false /* should_notify_initialized */);
  }

  void OnAppTypeInitialized(apps::AppType app_type) override {
    app_types_.insert(app_type);
    ++initialized_app_type_count_;
    app_count_at_initialization_ = updated_ids_.size();
    UpdateApps();
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    Observe(nullptr);
  }

  std::set<apps::AppType> app_types() const { return app_types_; }

  int initialized_app_type_count() const { return initialized_app_type_count_; }

  int app_count_at_initialization() const {
    return app_count_at_initialization_;
  }

 private:
  std::set<std::string> updated_ids_;
  std::set<apps::AppType> app_types_;
  int initialized_app_type_count_ = 0;
  int app_count_at_initialization_ = 0;
  apps::AppRegistryCache* cache_ = nullptr;
};

}  // namespace

class AppRegistryCacheTest : public testing::Test {
 public:
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

  void DisableOnAppTypeInitializedFlag() {
    scoped_feature_list_.InitAndDisableFeature(
        kAppServiceOnAppTypeInitializedWithoutMojom);
  }

  void EnableOnAppTypeInitializedFlag() {
    scoped_feature_list_.InitAndEnableFeature(
        kAppServiceOnAppTypeInitializedWithoutMojom);
  }

  std::set<std::string> updated_ids_;
  std::set<std::string> updated_names_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppRegistryCacheTest, OnApps) {
  AppRegistryCache cache;
  std::vector<AppPtr> deltas;
  deltas.push_back(MakeApp("a", "apple"));
  deltas.push_back(MakeApp("b", "banana", AppType::kArc, Readiness::kReady));
  deltas.push_back(MakeApp("c", "cherry", AppType::kArc,
                           Readiness::kDisabledByPolicy,
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
  deltas.push_back(MakeApp("a", "apricot", AppType::kArc, Readiness::kReady));
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

  std::vector<AppPtr> apps;
  apps.push_back(MakeApp("app", "app", AppType::kArc, Readiness::kReady));
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
  apps.push_back(
      MakeApp("app", "app", AppType::kArc, Readiness::kUninstalledByUser));
  apps.push_back(MakeApp("app", "app", AppType::kArc, Readiness::kRemoved));
  cache.OnApps(std::move(apps), AppType::kUnknown,
               false /* should_notify_initialized */);

  // The cache is now empty.
  EXPECT_EQ(0, AppCount(cache));
  CallForAllApps(cache);
  EXPECT_TRUE(updated_ids_.empty());
  EXPECT_TRUE(updated_names_.empty());
  Clear();
}

// Verify the OnAppTypeInitialized callback when OnApps is called for the non
// mojom App type first, with the disabled flag.
TEST_F(AppRegistryCacheTest,
       OnAppTypeInitializedWithDisableFlagNonMojomUpdateFirst) {
  DisableOnAppTypeInitializedFlag();

  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<AppPtr> deltas1;
  deltas1.push_back(MakeApp("a", "avocado"));
  deltas1.push_back(MakeApp("c", "cucumber"));
  cache.OnApps(std::move(deltas1), AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // added.
  EXPECT_TRUE(observer1.app_types().empty());
  EXPECT_EQ(0, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_TRUE(cache.InitializedAppTypes().empty());
  EXPECT_FALSE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<apps::mojom::AppPtr> mojom_deltas1;
  mojom_deltas1.push_back(MakeMojomApp("a", "avocado"));
  mojom_deltas1.push_back(MakeMojomApp("c", "cucumber"));
  cache.OnApps(std::move(mojom_deltas1), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the mojom Apps are added.
  EXPECT_TRUE(base::Contains(observer1.app_types(), apps::AppType::kArc));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas2), AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // added.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  std::vector<apps::mojom::AppPtr> mojom_deltas2;
  mojom_deltas2.push_back(MakeMojomApp("d", "durian"));
  cache.OnApps(std::move(mojom_deltas2), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the mojom Apps are
  // initialized again.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  // Verify the new observers should not have OnAppTypeInitialized called.
  InitializedObserver observer2(&cache);
  EXPECT_TRUE(observer2.app_types().empty());
  EXPECT_EQ(0, observer2.initialized_app_type_count());
  EXPECT_EQ(0, observer2.app_count_at_initialization());
}

// Verify the OnAppTypeInitialized callback when OnApps is called for the mojom
// App type first, with the disabled flag.
TEST_F(AppRegistryCacheTest,
       OnAppTypeInitializedWithDisableFlagMojomUpdateFirst) {
  DisableOnAppTypeInitializedFlag();

  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<apps::mojom::AppPtr> mojom_deltas1;
  mojom_deltas1.push_back(MakeMojomApp("a", "avocado"));
  mojom_deltas1.push_back(MakeMojomApp("c", "cucumber"));
  cache.OnApps(std::move(mojom_deltas1), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the mojom Apps are added.
  EXPECT_TRUE(base::Contains(observer1.app_types(), apps::AppType::kArc));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<AppPtr> deltas1;
  deltas1.push_back(MakeApp("a", "avocado"));
  deltas1.push_back(MakeApp("c", "cucumber"));
  cache.OnApps(std::move(deltas1), AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // added.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());

  std::vector<apps::mojom::AppPtr> mojom_deltas2;
  mojom_deltas2.push_back(MakeMojomApp("d", "durian"));
  cache.OnApps(std::move(mojom_deltas2), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas2), AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the Apps are added.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  // Verify the new observers should not have OnAppTypeInitialized called.
  InitializedObserver observer2(&cache);
  EXPECT_TRUE(observer2.app_types().empty());
  EXPECT_EQ(0, observer2.initialized_app_type_count());
  EXPECT_EQ(0, observer2.app_count_at_initialization());
}

// Verify the OnAppTypeInitialized callback when OnApps is called for multiple
// App types, with the disabled flag.
TEST_F(AppRegistryCacheTest,
       OnAppTypeInitializedWithDisableFlagMultipleAppTypes) {
  DisableOnAppTypeInitializedFlag();

  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<AppPtr> deltas1;
  deltas1.push_back(MakeApp("a", "avocado"));
  deltas1.push_back(MakeApp("c", "cucumber"));
  cache.OnApps(std::move(deltas1), AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // added.
  EXPECT_TRUE(observer1.app_types().empty());
  EXPECT_EQ(0, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_TRUE(cache.InitializedAppTypes().empty());

  std::vector<apps::mojom::AppPtr> mojom_deltas1;
  mojom_deltas1.push_back(MakeMojomApp("a", "avocado"));
  mojom_deltas1.push_back(MakeMojomApp("c", "cucumber"));
  cache.OnApps(std::move(mojom_deltas1), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the mojom Apps are added.
  EXPECT_TRUE(base::Contains(observer1.app_types(), apps::AppType::kArc));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<apps::mojom::AppPtr> mojom_deltas2;
  mojom_deltas2.push_back(
      MakeMojomApp("d", "durian", apps::mojom::AppType::kChromeApp));
  cache.OnApps(std::move(mojom_deltas2), apps::mojom::AppType::kChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the mojom Apps are added.
  EXPECT_EQ(2u, observer1.app_types().size());
  EXPECT_TRUE(base::Contains(observer1.app_types(), apps::AppType::kChromeApp));
  EXPECT_EQ(2, observer1.initialized_app_type_count());
  EXPECT_EQ(5, observer1.app_count_at_initialization());
  EXPECT_EQ(2u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kChromeApp));

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian", AppType::kChromeApp));
  cache.OnApps(std::move(deltas2), AppType::kChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // added.
  EXPECT_EQ(2, observer1.initialized_app_type_count());
  EXPECT_EQ(5, observer1.app_count_at_initialization());
  EXPECT_EQ(2u, cache.InitializedAppTypes().size());

  // Verify the new observers should not have OnAppTypeInitialized called.
  InitializedObserver observer2(&cache);
  EXPECT_TRUE(observer2.app_types().empty());
  EXPECT_EQ(0, observer2.initialized_app_type_count());
  EXPECT_EQ(0, observer2.app_count_at_initialization());
}

// Verify the OnAppTypeInitialized callback when OnApps is called for empty apps
// vector, with the disabled flag.
TEST_F(AppRegistryCacheTest, OnAppTypeInitializedWithDisableFlagEmptyUpdate) {
  DisableOnAppTypeInitializedFlag();

  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<AppPtr> deltas1;
  cache.OnApps(std::move(deltas1), AppType::kStandaloneBrowserChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // initialized.
  EXPECT_TRUE(observer1.app_types().empty());
  EXPECT_EQ(0, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_TRUE(cache.InitializedAppTypes().empty());
  EXPECT_FALSE(
      cache.IsAppTypeInitialized(AppType::kStandaloneBrowserChromeApp));

  std::vector<apps::mojom::AppPtr> mojom_deltas1;
  cache.OnApps(std::move(mojom_deltas1),
               apps::mojom::AppType::kStandaloneBrowserChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the mojom Apps are initialized.
  EXPECT_TRUE(base::Contains(observer1.app_types(),
                             apps::AppType::kStandaloneBrowserChromeApp));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kStandaloneBrowserChromeApp));

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas2), AppType::kStandaloneBrowserChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // initialized again.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  std::vector<apps::mojom::AppPtr> mojom_deltas2;
  mojom_deltas2.push_back(MakeMojomApp("d", "durian"));
  cache.OnApps(std::move(mojom_deltas2),
               apps::mojom::AppType::kStandaloneBrowserChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the mojom Apps are
  // initialized again.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  std::vector<apps::mojom::AppPtr> mojom_deltas3;
  cache.OnApps(std::move(mojom_deltas3), apps::mojom::AppType::kRemote,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the mojom Apps are initialized.
  EXPECT_EQ(2u, observer1.app_types().size());
  EXPECT_TRUE(base::Contains(observer1.app_types(), apps::AppType::kRemote));
  EXPECT_EQ(2, observer1.initialized_app_type_count());
  EXPECT_EQ(2u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kRemote));

  std::vector<AppPtr> deltas3;
  cache.OnApps(std::move(deltas3), AppType::kRemote,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // initialized.
  EXPECT_EQ(2, observer1.initialized_app_type_count());
  EXPECT_EQ(2u, cache.InitializedAppTypes().size());

  // Verify the new observers should not have OnAppTypeInitialized called.
  InitializedObserver observer2(&cache);
  EXPECT_TRUE(observer2.app_types().empty());
  EXPECT_EQ(0, observer2.initialized_app_type_count());
  EXPECT_EQ(0, observer2.app_count_at_initialization());
}

// Verify the OnAppTypeInitialized callback when OnApps is called for the non
// mojom App type first, with the enabled flag.
TEST_F(AppRegistryCacheTest,
       OnAppTypeInitializedWithEnableFlagNonMojomUpdateFirst) {
  EnableOnAppTypeInitializedFlag();

  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<AppPtr> deltas1;
  deltas1.push_back(MakeApp("a", "avocado"));
  deltas1.push_back(MakeApp("c", "cucumber"));
  cache.OnApps(std::move(deltas1), AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // added.
  EXPECT_TRUE(observer1.app_types().empty());
  EXPECT_EQ(0, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(0u, cache.InitializedAppTypes().size());
  EXPECT_FALSE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<apps::mojom::AppPtr> mojom_deltas1;
  mojom_deltas1.push_back(MakeMojomApp("a", "avocado"));
  mojom_deltas1.push_back(MakeMojomApp("c", "cucumber"));
  cache.OnApps(std::move(mojom_deltas1), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when both the non mojom and mojom
  // Apps are added.
  EXPECT_TRUE(base::Contains(observer1.app_types(), AppType::kArc));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas2), AppType::kArc,
               true /* should_notify_initialized */);

  std::vector<apps::mojom::AppPtr> mojom_deltas2;
  mojom_deltas2.push_back(MakeMojomApp("d", "durian"));
  cache.OnApps(std::move(mojom_deltas2), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when more Apps are
  // added.
  EXPECT_TRUE(base::Contains(observer1.app_types(), AppType::kArc));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  // Verify the new observers should not have OnAppTypeInitialized called.
  InitializedObserver observer2(&cache);
  EXPECT_TRUE(observer2.app_types().empty());
  EXPECT_EQ(0, observer2.initialized_app_type_count());
  EXPECT_EQ(0, observer2.app_count_at_initialization());
}

// Verify the OnAppTypeInitialized callback when OnApps is called for the mojom
// App type first, with the enabled flag.
TEST_F(AppRegistryCacheTest,
       OnAppTypeInitializedWithEnableFlagMojomUpdateFirst) {
  EnableOnAppTypeInitializedFlag();

  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<apps::mojom::AppPtr> mojom_deltas1;
  mojom_deltas1.push_back(MakeMojomApp("a", "avocado"));
  mojom_deltas1.push_back(MakeMojomApp("c", "cucumber"));
  cache.OnApps(std::move(mojom_deltas1), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the mojom Apps are added.
  EXPECT_TRUE(observer1.app_types().empty());
  EXPECT_EQ(0, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(0u, cache.InitializedAppTypes().size());
  EXPECT_FALSE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<AppPtr> deltas1;
  deltas1.push_back(MakeApp("a", "avocado"));
  deltas1.push_back(MakeApp("c", "cucumber"));
  cache.OnApps(std::move(deltas1), AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when both the non mojom and mojom
  // Apps are added.
  EXPECT_TRUE(base::Contains(observer1.app_types(), AppType::kArc));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<apps::mojom::AppPtr> mojom_deltas2;
  mojom_deltas2.push_back(MakeMojomApp("d", "durian"));
  cache.OnApps(std::move(mojom_deltas2), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas2), AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the Apps are added.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  // Verify the new observers should not have OnAppTypeInitialized called.
  InitializedObserver observer2(&cache);
  EXPECT_TRUE(observer2.app_types().empty());
  EXPECT_EQ(0, observer2.initialized_app_type_count());
  EXPECT_EQ(0, observer2.app_count_at_initialization());
}

// Verify the OnAppTypeInitialized callback when OnApps is called for multiple
// App types, with the enabled flag.
TEST_F(AppRegistryCacheTest,
       OnAppTypeInitializedWithEnableFlagMultipleAppTypes) {
  EnableOnAppTypeInitializedFlag();

  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<AppPtr> deltas1;
  deltas1.push_back(MakeApp("a", "avocado"));
  deltas1.push_back(MakeApp("c", "cucumber"));
  cache.OnApps(std::move(deltas1), AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // added.
  EXPECT_TRUE(observer1.app_types().empty());
  EXPECT_EQ(0, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_TRUE(cache.InitializedAppTypes().empty());

  std::vector<apps::mojom::AppPtr> mojom_deltas1;
  mojom_deltas1.push_back(MakeMojomApp("a", "avocado"));
  mojom_deltas1.push_back(MakeMojomApp("c", "cucumber"));
  cache.OnApps(std::move(mojom_deltas1), apps::mojom::AppType::kArc,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when both the non mojom and mojom
  // Apps are added.
  EXPECT_TRUE(base::Contains(observer1.app_types(), AppType::kArc));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<apps::mojom::AppPtr> mojom_deltas2;
  mojom_deltas2.push_back(
      MakeMojomApp("d", "durian", apps::mojom::AppType::kChromeApp));
  cache.OnApps(std::move(mojom_deltas2), apps::mojom::AppType::kChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the mojom Apps are added.
  EXPECT_EQ(1u, observer1.app_types().size());
  EXPECT_FALSE(base::Contains(observer1.app_types(), AppType::kChromeApp));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_FALSE(cache.IsAppTypeInitialized(AppType::kChromeApp));

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian", AppType::kChromeApp));
  cache.OnApps(std::move(deltas2), AppType::kChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when both the non mojom and mojom
  // Apps are added.
  EXPECT_EQ(2u, observer1.app_types().size());
  EXPECT_TRUE(base::Contains(observer1.app_types(), AppType::kChromeApp));
  EXPECT_EQ(2, observer1.initialized_app_type_count());
  EXPECT_EQ(5, observer1.app_count_at_initialization());
  EXPECT_EQ(2u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kChromeApp));

  // Verify the new observers should not have OnAppTypeInitialized called.
  InitializedObserver observer2(&cache);
  EXPECT_TRUE(observer2.app_types().empty());
  EXPECT_EQ(0, observer2.initialized_app_type_count());
  EXPECT_EQ(0, observer2.app_count_at_initialization());
}

// Verify the OnAppTypeInitialized callback when OnApps is called for empty apps
// vector, with the enabled flag.
TEST_F(AppRegistryCacheTest, OnAppTypeInitializedWithEnableFlagEmptyUpdate) {
  EnableOnAppTypeInitializedFlag();

  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<AppPtr> deltas1;
  cache.OnApps(std::move(deltas1), AppType::kStandaloneBrowserChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // initialized.
  EXPECT_TRUE(observer1.app_types().empty());
  EXPECT_EQ(0, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_TRUE(cache.InitializedAppTypes().empty());

  std::vector<apps::mojom::AppPtr> mojom_deltas1;
  cache.OnApps(std::move(mojom_deltas1),
               apps::mojom::AppType::kStandaloneBrowserChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when both the mojom and non mojom
  // Apps are initialized.
  EXPECT_TRUE(base::Contains(observer1.app_types(),
                             AppType::kStandaloneBrowserChromeApp));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kStandaloneBrowserChromeApp));

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas2), AppType::kStandaloneBrowserChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the non mojom Apps are
  // initialized again.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  std::vector<apps::mojom::AppPtr> mojom_deltas2;
  mojom_deltas2.push_back(MakeMojomApp("d", "durian"));
  cache.OnApps(std::move(mojom_deltas2),
               apps::mojom::AppType::kStandaloneBrowserChromeApp,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the mojom Apps are
  // initialized again.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  std::vector<apps::mojom::AppPtr> mojom_deltas3;
  cache.OnApps(std::move(mojom_deltas3), apps::mojom::AppType::kRemote,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the mojom Apps are
  // initialized.
  EXPECT_EQ(1u, observer1.app_types().size());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  std::vector<AppPtr> deltas3;
  cache.OnApps(std::move(deltas3), AppType::kRemote,
               true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when both the mojom and non mojom
  // Apps are initialized.
  EXPECT_EQ(2u, observer1.app_types().size());
  EXPECT_TRUE(base::Contains(observer1.app_types(), AppType::kRemote));
  EXPECT_EQ(2, observer1.initialized_app_type_count());
  EXPECT_EQ(2u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kRemote));

  // Verify the new observers should not have OnAppTypeInitialized called.
  InitializedObserver observer2(&cache);
  EXPECT_TRUE(observer2.app_types().empty());
  EXPECT_EQ(0, observer2.initialized_app_type_count());
  EXPECT_EQ(0, observer2.app_count_at_initialization());
}

}  // namespace apps
