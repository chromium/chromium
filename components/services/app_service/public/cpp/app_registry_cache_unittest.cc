// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_registry_cache.h"

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

using ::testing::Eq;
using ::testing::Optional;
using ::testing::Property;

apps::AppPtr MakeApp(const char* app_id,
                     const char* name,
                     apps::AppType app_type = apps::AppType::kArc,
                     apps::Readiness readiness = apps::Readiness::kUnknown,
                     bool should_update_icon_version = false) {
  auto app = std::make_unique<apps::App>(app_type, app_id);
  app->readiness = readiness;
  app->name = name;
  app->icon_key = apps::IconKey(/*resource_id=*/0, /*icon_effects=*/0);
  app->icon_key->update_version = should_update_icon_version;
  return app;
}

class MockRegistryObserver : public apps::AppRegistryCache::Observer {
 public:
  MOCK_METHOD(void, OnAppUpdate, (const apps::AppUpdate& update), ());

  MOCK_METHOD(void,
              OnAppRegistryCacheWillBeDestroyed,
              (apps::AppRegistryCache * cache),
              ());
};

MATCHER_P(HasAppId, app_id, "Has the correct app id") {
  return arg.AppId() == app_id;
}

// RemoveObserver is used to test OnAppUpdate for the removed app.
class RemoveObserver : public apps::AppRegistryCache::Observer {
 public:
  explicit RemoveObserver(apps::AppRegistryCache* cache) {
    app_registry_cache_observer_.Observe(cache);
  }

  ~RemoveObserver() override = default;

  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& update) override {
    updated_ids_.push_back(update.AppId());
    readinesses_.push_back(update.Readiness());
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    app_registry_cache_observer_.Reset();
  }

  void Clear() {
    updated_ids_.clear();
    readinesses_.clear();
  }

  std::vector<std::string> updated_ids() const { return updated_ids_; }
  std::vector<apps::Readiness> readinesses() const { return readinesses_; }

 private:
  std::vector<std::string> updated_ids_;
  std::vector<apps::Readiness> readinesses_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

// Responds to a cache's OnAppUpdate to call back into the cache, checking that
// the cache presents a self-consistent snapshot. For example, the app names
// should match for the outer and inner AppUpdate.
//
// In the tests below, just "recursive" means that cache.OnApps calls
// observer.OnAppsUpdate which calls cache.ForApp and cache.ForAllApps.
// "Super-recursive" means that cache.OnApps calls observer.OnAppsUpdate calls
// cache.OnApps which calls observer.OnAppsUpdate.
class RecursiveObserver : public AppRegistryCache::Observer {
 public:
  explicit RecursiveObserver(AppRegistryCache* cache) : cache_(cache) {
    app_registry_cache_observer_.Observe(cache);
  }

  ~RecursiveObserver() override = default;

  void PrepareForOnApps(int expected_num_apps,
                        const std::string& expected_name_for_p,
                        std::vector<AppPtr>* super_recursive_apps = nullptr) {
    expected_name_for_p_ = expected_name_for_p;
    expected_num_apps_ = expected_num_apps;
    num_apps_seen_on_app_update_ = 0;
    old_names_.clear();

    names_snapshot_.clear();
    check_names_snapshot_ = true;
    if (super_recursive_apps) {
      check_names_snapshot_ = false;
      super_recursive_apps_.swap(*super_recursive_apps);
    }
  }

  int NumAppsSeenOnAppUpdate() { return num_apps_seen_on_app_update_; }

  AppType app_type() const { return app_type_; }

 protected:
  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const AppUpdate& outer) override {
    EXPECT_EQ(account_id_, outer.AccountId());
    int num_apps = 0;
    cache_->ForEachApp([this, &outer, &num_apps](const AppUpdate& inner) {
      if (check_names_snapshot_) {
        if (num_apps_seen_on_app_update_ == 0) {
          // If this is the first time that OnAppUpdate is called, after a
          // PrepareForOnApps call, then just populate the names_snapshot_ map.
          names_snapshot_[inner.AppId()] = inner.Name();
        } else {
          // Otherwise, check that the names found during this OnAppUpdate call
          // match those during the first OnAppUpdate call.
          auto iter = names_snapshot_.find(inner.AppId());
          EXPECT_EQ(inner.Name(),
                    (iter != names_snapshot_.end()) ? iter->second : "");
        }
      }

      if (outer.AppId() == inner.AppId()) {
        ExpectEq(outer, inner);
      }

      if (inner.AppId() == "p") {
        EXPECT_EQ(expected_name_for_p_, inner.Name());
      }

      num_apps++;
    });
    EXPECT_EQ(expected_num_apps_, num_apps);

    EXPECT_FALSE(cache_->ForOneApp(
        "no_such_app_id",
        [&outer](const AppUpdate& inner) { ExpectEq(outer, inner); }));

    EXPECT_TRUE(cache_->ForOneApp(
        outer.AppId(),
        [&outer](const AppUpdate& inner) { ExpectEq(outer, inner); }));

    if (outer.NameChanged()) {
      std::string old_name;
      auto iter = old_names_.find(outer.AppId());
      if (iter != old_names_.end()) {
        old_name = iter->second;
      }
      // The way the tests are configured, if an app's name changes, it should
      // increase (in string comparison order): e.g. from "" to "mango" or from
      // "mango" to "mulberry" and never from "mulberry" to "melon".
      EXPECT_LT(old_name, outer.Name());
    }
    old_names_[outer.AppId()] = outer.Name();

    std::vector<AppPtr> super_recursive;
    while (!super_recursive_apps_.empty()) {
      AppPtr app = std::move(super_recursive_apps_.back());
      super_recursive_apps_.pop_back();
      if (app.get() == nullptr) {
        // This is the placeholder 'punctuation'.
        break;
      }
      super_recursive.push_back(std::move(app));
    }
    if (!super_recursive.empty()) {
      cache_->OnAppsForTesting(std::move(super_recursive), AppType::kArc,
                               false /* should_notify_initialized */);
    }

    num_apps_seen_on_app_update_++;
  }

  void OnAppTypeInitialized(AppType app_type) override { app_type_ = app_type; }

  void OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) override {
    app_registry_cache_observer_.Reset();
  }

  static void ExpectEq(const AppUpdate& outer, const AppUpdate& inner) {
    EXPECT_EQ(outer.AppType(), inner.AppType());
    EXPECT_EQ(outer.AppId(), inner.AppId());
    EXPECT_EQ(outer.StateIsNull(), inner.StateIsNull());
    EXPECT_EQ(outer.Readiness(), inner.Readiness());
    EXPECT_EQ(outer.Name(), inner.Name());
  }

  raw_ptr<AppRegistryCache> cache_;
  std::string expected_name_for_p_;
  int expected_num_apps_;
  int num_apps_seen_on_app_update_;
  AccountId account_id_ = AccountId::FromUserEmail("test@gmail.com");
  AppType app_type_ = AppType::kUnknown;

  // Records previously seen app names, keyed by app_id's, so we can check
  // that, for these tests, a given app's name is always increasing (in string
  // comparison order).
  std::map<std::string, std::string> old_names_;

  // Non-empty when this.OnAppsUpdate should trigger more cache_.OnApps calls.
  //
  // During OnAppsUpdate, this vector (a stack) is popped from the back until a
  // nullptr 'punctuation' element (a group terminator) is seen. If that group
  // of popped elements (in LIFO order) is non-empty, that group forms the
  // vector of App's passed to cache_.OnApps.
  std::vector<AppPtr> super_recursive_apps_;

  // For non-super-recursive tests (i.e. for check_names_snapshot_ == true), we
  // check that the "app_id to name" mapping is consistent across every
  // OnAppsUpdate call to this observer. For super-recursive tests, that
  // mapping can change as updates are processed, so the names_snapshot_ check
  // is skipped.
  bool check_names_snapshot_ = false;
  std::map<std::string, std::string> names_snapshot_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

// InitializedObserver is used to test the OnAppTypeInitialized interface for
// AppRegistryCache::Observer.
class InitializedObserver : public apps::AppRegistryCache::Observer {
 public:
  explicit InitializedObserver(apps::AppRegistryCache* cache) : cache_(cache) {
    app_registry_cache_observer_.Observe(cache);
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
    cache_->OnAppsForTesting(std::move(deltas), AppType::kUnknown,
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
    app_registry_cache_observer_.Reset();
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
  raw_ptr<apps::AppRegistryCache> cache_ = nullptr;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

}  // namespace

class AppRegistryCacheTest : public testing::Test,
                             public AppRegistryCache::Observer {
 public:
  void CallForEachApp(AppRegistryCache& cache) {
    cache.ForEachApp([this](const AppUpdate& update) { OnAppUpdate(update); });
  }

  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const AppUpdate& update) override {
    update_count_++;

    EXPECT_EQ(account_id_, update.AccountId());

    if (update.state_ || update.delta_) {
      EXPECT_NE("", update.Name());
      if (!apps_util::IsInstalled(update.Readiness())) {
        return;
      }
      if (update.ReadinessChanged() &&
          (update.Readiness() == Readiness::kReady)) {
        num_freshly_installed_++;
      }
      updated_ids_.insert(update.AppId());
      updated_names_.insert(update.Name());
    } else {
      EXPECT_NE("", update.Name());
      if (!apps_util::IsInstalled(update.Readiness())) {
        return;
      }
      if (update.ReadinessChanged() &&
          (update.Readiness() == apps::Readiness::kReady)) {
        num_freshly_installed_++;
      }
      updated_ids_.insert(update.AppId());
      updated_names_.insert(update.Name());
    }
  }

  void OnAppTypeInitialized(AppType app_type) override { app_type_ = app_type; }

  void OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) override {
    // The test code explicitly calls both AddObserver and RemoveObserver.
    NOTREACHED_IN_MIGRATION();
  }

  std::string GetName(AppRegistryCache& cache, const std::string& app_id) {
    std::string name;
    cache.ForOneApp(app_id,
                    [&name](const AppUpdate& update) { name = update.Name(); });
    return name;
  }

  void OnApps(AppRegistryCache& cache,
              std::vector<AppPtr> deltas,
              apps::AppType app_type,
              bool should_notify_initialized) {
    cache.OnApps(std::move(deltas), app_type, should_notify_initialized);
  }

  void VerifyApp(AppRegistryCache& cache,
                 const char* app_id,
                 const char* name,
                 Readiness readiness = Readiness::kUnknown,
                 int32_t icon_update_version = IconKey::kInitVersion) {
    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    ASSERT_TRUE(cache.states_[app_id]->name.has_value());
    EXPECT_EQ(name, GetName(cache, app_id));
    EXPECT_EQ(readiness, cache.states_[app_id]->readiness);
    auto& icon_key = cache.states_[app_id]->icon_key;
    ASSERT_TRUE(icon_key.has_value());
    ASSERT_TRUE(absl::holds_alternative<int32_t>(icon_key->update_version));
    EXPECT_EQ(icon_update_version,
              absl::get<int32_t>(icon_key->update_version));
  }

  int AppCount(const AppRegistryCache& cache) { return cache.states_.size(); }

  void Clear() {
    update_count_ = 0;
    updated_ids_.clear();
    updated_names_.clear();
  }

  const AccountId& account_id() const { return account_id_; }

  void SetAppType(AppType app_type) { app_type_ = app_type; }

  AppType app_type() const { return app_type_; }

  int update_count() const { return update_count_; }

  std::set<std::string> updated_ids_;
  std::set<std::string> updated_names_;
  int num_freshly_installed_ = 0;

 private:
  AccountId account_id_ = AccountId::FromUserEmail("test@gmail.com");
  AppType app_type_ = AppType::kUnknown;
  int update_count_ = 0;
};

TEST_F(AppRegistryCacheTest, OnApps) {
  AppRegistryCache cache;
  cache.SetAccountId(account_id());

  std::vector<AppPtr> deltas;
  deltas.push_back(MakeApp("a", "apple"));
  deltas.push_back(MakeApp("b", "banana", AppType::kArc, Readiness::kReady));
  deltas.push_back(MakeApp("c", "cherry", AppType::kArc,
                           Readiness::kDisabledByPolicy,
                           /*should_update_icon_version=*/true));
  OnApps(cache, std::move(deltas), AppType::kUnknown,
         false /* should_notify_initialized */);

  EXPECT_EQ(3, AppCount(cache));
  VerifyApp(cache, "a", "apple");
  VerifyApp(cache, "b", "banana", Readiness::kReady);
  VerifyApp(cache, "c", "cherry", Readiness::kDisabledByPolicy,
            /*icon_update_version=*/IconKey::kInitVersion);

  CallForEachApp(cache);
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_EQ(2u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("banana"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("cherry"));
  Clear();

  auto all_apps = cache.GetAllApps();
  ASSERT_EQ(3u, all_apps.size());
  EXPECT_EQ("a", all_apps[0]->app_id);
  EXPECT_EQ("b", all_apps[1]->app_id);
  EXPECT_EQ("c", all_apps[2]->app_id);

  deltas.clear();
  deltas.push_back(MakeApp("a", "apricot", AppType::kArc, Readiness::kReady));
  deltas.push_back(MakeApp("d", "durian"));
  OnApps(cache, std::move(deltas), AppType::kUnknown,
         false /* should_notify_initialized */);

  EXPECT_EQ(4, AppCount(cache));
  VerifyApp(cache, "a", "apricot", Readiness::kReady);
  VerifyApp(cache, "b", "banana", Readiness::kReady);
  VerifyApp(cache, "c", "cherry", Readiness::kDisabledByPolicy,
            /*icon_update_version=*/IconKey::kInitVersion);
  VerifyApp(cache, "d", "durian");

  CallForEachApp(cache);
  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_EQ(3u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("apricot"));
  Clear();

  all_apps = cache.GetAllApps();
  ASSERT_EQ(4u, all_apps.size());
  EXPECT_EQ("a", all_apps[0]->app_id);
  EXPECT_EQ("b", all_apps[1]->app_id);
  EXPECT_EQ("c", all_apps[2]->app_id);
  EXPECT_EQ("d", all_apps[3]->app_id);

  // Test that ForOneApp succeeds for "c" and fails for "e".

  bool found_c = false;
  EXPECT_TRUE(cache.ForOneApp("c", [&found_c](const apps::AppUpdate& update) {
    found_c = true;
    EXPECT_EQ("c", update.AppId());
  }));
  EXPECT_TRUE(found_c);

  bool found_e = false;
  EXPECT_FALSE(cache.ForOneApp("e", [&found_e](const apps::AppUpdate& update) {
    found_e = true;
    EXPECT_EQ("e", update.AppId());
  }));
  EXPECT_FALSE(found_e);

  // Test that GetAppUpdate matches the behaviour of ForOneApp.
  EXPECT_THAT(cache.GetAppUpdate("c"),
              Optional(Property("AppId", &apps::AppUpdate::AppId, "c")));
  EXPECT_THAT(cache.GetAppUpdate("e"), Eq(std::nullopt));
}

TEST_F(AppRegistryCacheTest, Removed) {
  AppRegistryCache cache;
  testing::StrictMock<MockRegistryObserver> observer;
  cache.SetAccountId(account_id());

  base::ScopedObservation<AppRegistryCache, AppRegistryCache::Observer>
      observation{&observer};
  observation.Observe(&cache);

  // Starting with an empty cache.
  cache.ForEachApp([&observer](const apps::AppUpdate& update) {
    observer.OnAppUpdate(update);
  });

  // We add the app, and expect to be notified.
  EXPECT_CALL(observer, OnAppUpdate(HasAppId("app")));

  std::vector<AppPtr> apps;
  apps.push_back(MakeApp("app", "app", AppType::kArc, Readiness::kReady));
  OnApps(cache, std::move(apps), AppType::kUnknown,
         false /* should_notify_initialized */);

  CallForEachApp(cache);
  EXPECT_EQ(1u, updated_ids_.size());
  EXPECT_EQ(1u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("app"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("app"));
  Clear();

  auto all_apps = cache.GetAllApps();
  ASSERT_EQ(1u, all_apps.size());
  EXPECT_EQ("app", all_apps[0]->app_id);

  // Uninstall the app, then remove it.
  apps.clear();
  apps.push_back(
      MakeApp("app", "app", AppType::kArc, Readiness::kUninstalledByUser));
  apps.push_back(MakeApp("app", "app", AppType::kArc, Readiness::kRemoved));

  // We should see one call informing us that the app was uninstalled.
  EXPECT_CALL(observer, OnAppUpdate(HasAppId("app")))
      .WillOnce(testing::Invoke([&observer, &cache](const AppUpdate& update) {
        EXPECT_EQ(Readiness::kUninstalledByUser, update.Readiness());
        // Even though we have queued the removal, checking the cache now
        // shows the app is still present.
        EXPECT_CALL(observer, OnAppUpdate(HasAppId("app")));
        cache.ForEachApp([&observer](const AppUpdate& update) {
          observer.OnAppUpdate(update);
        });
      }));

  OnApps(cache, std::move(apps), AppType::kUnknown,
         false /* should_notify_initialized */);

  // The cache is now empty.
  EXPECT_EQ(0, AppCount(cache));
  CallForEachApp(cache);
  EXPECT_TRUE(updated_ids_.empty());
  EXPECT_TRUE(updated_names_.empty());
  Clear();
  observation.Reset();

  EXPECT_TRUE(cache.GetAllApps().empty());
}

TEST_F(AppRegistryCacheTest, RemovedAndAdded) {
  AppRegistryCache cache;
  RemoveObserver observer(&cache);
  cache.SetAccountId(account_id());

  // We add the app, and expect to be notified.
  std::vector<AppPtr> apps;
  apps.push_back(MakeApp("app", "app", AppType::kArc, Readiness::kReady));
  OnApps(cache, std::move(apps), AppType::kUnknown,
         false /* should_notify_initialized */);

  // Verify "app" is notified via OnAppUpdate.
  CallForEachApp(cache);
  EXPECT_EQ(1u, updated_ids_.size());
  EXPECT_EQ(1u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("app"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("app"));
  Clear();

  // Verify "app" is notified via OnAppUpdate as the kReady status.
  ASSERT_EQ(1u, observer.updated_ids().size());
  ASSERT_EQ(1u, observer.readinesses().size());
  EXPECT_EQ("app", observer.updated_ids()[0]);
  EXPECT_EQ(Readiness::kReady, observer.readinesses()[0]);
  observer.Clear();

  // Uninstall the app, then remove it, and add it back again.
  apps.clear();
  apps.push_back(
      MakeApp("app", "app", AppType::kArc, Readiness::kUninstalledByUser));
  apps.push_back(MakeApp("app", "app", AppType::kArc, Readiness::kRemoved));
  apps.push_back(MakeApp("app", "app", AppType::kArc, Readiness::kReady));

  OnApps(cache, std::move(apps), AppType::kUnknown,
         false /* should_notify_initialized */);

  // The cache is not empty, "app" is still saved in the cache.
  EXPECT_EQ(1, AppCount(cache));
  CallForEachApp(cache);
  EXPECT_EQ(1u, updated_ids_.size());
  EXPECT_EQ(1u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("app"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("app"));
  Clear();

  // Verify OnAppUpdate notifies the update for "app" as:
  // 1: kUninstalledByUser status
  // 2: kReady status
  ASSERT_EQ(2u, observer.updated_ids().size());
  ASSERT_EQ(2u, observer.readinesses().size());
  EXPECT_EQ("app", observer.updated_ids()[0]);
  EXPECT_EQ(Readiness::kUninstalledByUser, observer.readinesses()[0]);
  EXPECT_EQ("app", observer.updated_ids()[1]);
  EXPECT_EQ(Readiness::kReady, observer.readinesses()[1]);
}

TEST_F(AppRegistryCacheTest, RemovedAndAddMultipleApps) {
  AppRegistryCache cache;
  RemoveObserver observer(&cache);
  cache.SetAccountId(account_id());

  // We add the app, and expect to be notified.
  std::vector<AppPtr> apps;
  apps.push_back(MakeApp("app1", "app1", AppType::kArc, Readiness::kReady));
  OnApps(cache, std::move(apps), AppType::kUnknown,
         false /* should_notify_initialized */);

  // Verify "app1" is added to the cache and is notified via OnAppUpdate.
  CallForEachApp(cache);
  EXPECT_EQ(1u, updated_ids_.size());
  EXPECT_EQ(1u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("app1"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("app1"));
  Clear();

  // Verify "app1" is notified via OnAppUpdate as the kReady status.
  ASSERT_EQ(1u, observer.updated_ids().size());
  ASSERT_EQ(1u, observer.readinesses().size());
  EXPECT_EQ("app1", observer.updated_ids()[0]);
  EXPECT_EQ(Readiness::kReady, observer.readinesses()[0]);
  observer.Clear();

  // Add multiple app updates for 1 OnApps call:
  // 1. Uninstall "app1", then remove it, and add it back again.
  // 2. Add "app2" as the kDisabledByPolicy status.
  apps.clear();
  apps.push_back(
      MakeApp("app1", "app1", AppType::kArc, Readiness::kUninstalledByUser));
  apps.push_back(MakeApp("app1", "app1", AppType::kArc, Readiness::kRemoved));
  apps.push_back(MakeApp("app1", "app1", AppType::kArc, Readiness::kReady));
  apps.push_back(
      MakeApp("app2", "app2", AppType::kArc, Readiness::kDisabledByPolicy));

  OnApps(cache, std::move(apps), AppType::kUnknown,
         false /* should_notify_initialized */);

  // The cache is not empty. Verify both "app1" and "app2" exist in the cache.
  EXPECT_EQ(2, AppCount(cache));
  CallForEachApp(cache);
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_EQ(2u, updated_names_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("app1"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("app1"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("app2"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("app2"));
  Clear();

  // Verify the OnAppUpdate result:
  // 1. {app1: kUninstalledByUser}.
  // 2. {app1: kReady}.
  // 3. {app2: kDisabledByPolicy}.
  ASSERT_EQ(3u, observer.updated_ids().size());
  ASSERT_EQ(3u, observer.readinesses().size());
  EXPECT_EQ("app1", observer.updated_ids()[0]);
  EXPECT_EQ(Readiness::kUninstalledByUser, observer.readinesses()[0]);
  EXPECT_EQ("app1", observer.updated_ids()[1]);
  EXPECT_EQ(Readiness::kReady, observer.readinesses()[1]);
  EXPECT_EQ("app2", observer.updated_ids()[2]);
  EXPECT_EQ(Readiness::kDisabledByPolicy, observer.readinesses()[2]);
}

TEST_F(AppRegistryCacheTest, Observer) {
  std::vector<AppPtr> deltas;
  AppRegistryCache cache;
  cache.SetAccountId(account_id());

  base::ScopedObservation<AppRegistryCache, AppRegistryCache::Observer>
      observation{this};
  observation.Observe(&cache);

  num_freshly_installed_ = 0;
  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(
      MakeApp("a", "avocado", AppType::kArc, Readiness::kDisabledByPolicy));
  deltas.push_back(
      MakeApp("c", "cucumber", AppType::kArc, Readiness::kDisabledByPolicy));
  deltas.push_back(
      MakeApp("e", "eggfruit", AppType::kArc, Readiness::kDisabledByPolicy));
  OnApps(cache, std::move(deltas), AppType::kArc,
         true /* should_notify_initialized */);

  EXPECT_EQ(0, num_freshly_installed_);
  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("e"));
  EXPECT_EQ(AppType::kArc, app_type());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  auto all_apps = cache.GetAllApps();
  ASSERT_EQ(3u, all_apps.size());
  EXPECT_EQ("a", all_apps[0]->app_id);
  EXPECT_EQ("c", all_apps[1]->app_id);
  EXPECT_EQ("e", all_apps[2]->app_id);

  SetAppType(AppType::kUnknown);
  num_freshly_installed_ = 0;
  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(
      MakeApp("b", "blueberry", AppType::kArc, Readiness::kDisabledByPolicy));
  deltas.push_back(MakeApp("c", "cucumber", AppType::kArc, Readiness::kReady));
  OnApps(cache, std::move(deltas), AppType::kArc,
         false /* should_notify_initialized */);

  EXPECT_EQ(1, num_freshly_installed_);
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));
  EXPECT_EQ(AppType::kUnknown, app_type());

  all_apps = cache.GetAllApps();
  ASSERT_EQ(4u, all_apps.size());
  EXPECT_EQ("a", all_apps[0]->app_id);
  EXPECT_EQ("b", all_apps[1]->app_id);
  EXPECT_EQ("c", all_apps[2]->app_id);
  EXPECT_EQ("e", all_apps[3]->app_id);

  observation.Reset();

  num_freshly_installed_ = 0;
  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(MakeApp("f", "fig", AppType::kArc, Readiness::kReady));
  OnApps(cache, std::move(deltas), AppType::kUnknown,
         false /* should_notify_initialized */);

  EXPECT_EQ(0, num_freshly_installed_);
  EXPECT_EQ(0u, updated_ids_.size());
  EXPECT_EQ(AppType::kUnknown, app_type());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  all_apps = cache.GetAllApps();
  ASSERT_EQ(5u, all_apps.size());
  EXPECT_EQ("a", all_apps[0]->app_id);
  EXPECT_EQ("b", all_apps[1]->app_id);
  EXPECT_EQ("c", all_apps[2]->app_id);
  EXPECT_EQ("e", all_apps[3]->app_id);
  EXPECT_EQ("f", all_apps[4]->app_id);
}

TEST_F(AppRegistryCacheTest, Recursive) {
  std::vector<AppPtr> deltas;
  AppRegistryCache cache;
  cache.SetAccountId(account_id());
  RecursiveObserver observer(&cache);

  observer.PrepareForOnApps(2, "peach");

  deltas.clear();
  deltas.push_back(MakeApp("o", "orange"));
  deltas.push_back(MakeApp("p", "peach"));
  OnApps(cache, std::move(deltas), AppType::kArc,
         true /* should_notify_initialized */);
  EXPECT_EQ(2, observer.NumAppsSeenOnAppUpdate());

  observer.PrepareForOnApps(3, "pear");

  deltas.clear();
  deltas.push_back(MakeApp("p", "pear", AppType::kArc, Readiness::kReady));
  deltas.push_back(MakeApp("q", "quince"));
  OnApps(cache, std::move(deltas), AppType::kUnknown,
         false /* should_notify_initialized */);
  EXPECT_EQ(2, observer.NumAppsSeenOnAppUpdate());

  observer.PrepareForOnApps(3, "plum");

  deltas.clear();
  deltas.push_back(MakeApp("p", "pear"));
  deltas.push_back(MakeApp("p", "pear"));
  deltas.push_back(MakeApp("p", "plum"));
  OnApps(cache, std::move(deltas), AppType::kUnknown,
         false /* should_notify_initialized */);

  EXPECT_EQ(1, observer.NumAppsSeenOnAppUpdate());
  EXPECT_EQ(AppType::kArc, observer.app_type());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  auto all_apps = cache.GetAllApps();
  ASSERT_EQ(3u, all_apps.size());
  EXPECT_EQ("o", all_apps[0]->app_id);
  EXPECT_EQ("p", all_apps[1]->app_id);
  EXPECT_EQ("q", all_apps[2]->app_id);
}

TEST_F(AppRegistryCacheTest, SuperRecursive) {
  std::vector<AppPtr> deltas;
  AppRegistryCache cache;
  cache.SetAccountId(account_id());
  RecursiveObserver observer(&cache);

  // Set up a series of OnApps to be called during observer.OnAppUpdate:
  //  - the 1st update is {"blackberry, "coconut"}.
  //  - the 2nd update is {}.
  //  - the 3rd update is {"blackcurrant", "apricot", "blueberry"}.
  //  - the 4th update is {"avocado"}.
  //  - the 5th update is {}.
  //  - the 6th update is {"boysenberry"}.
  //
  // The vector is processed in LIFO order with nullptr punctuation to
  // terminate each group. See the comment on the
  // RecursiveObserver::super_recursive_apps_ field.
  std::vector<AppPtr> super_recursive_apps;
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(MakeApp("b", "boysenberry"));
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(MakeApp("a", "avocado"));
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(MakeApp("b", "blueberry"));
  super_recursive_apps.push_back(MakeApp("a", "apricot"));
  super_recursive_apps.push_back(MakeApp("b", "blackcurrant"));
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(nullptr);
  super_recursive_apps.push_back(MakeApp("c", "coconut"));
  super_recursive_apps.push_back(MakeApp("b", "blackberry"));

  observer.PrepareForOnApps(3, "", &super_recursive_apps);
  deltas.clear();
  deltas.push_back(MakeApp("a", "apple"));
  deltas.push_back(MakeApp("b", "banana"));
  deltas.push_back(MakeApp("c", "cherry"));
  OnApps(cache, std::move(deltas), AppType::kArc,
         true /* should_notify_initialized */);

  // After all of that, check that for each app_id, the last delta won.
  EXPECT_EQ("avocado", GetName(cache, "a"));
  EXPECT_EQ("boysenberry", GetName(cache, "b"));
  EXPECT_EQ("coconut", GetName(cache, "c"));
  EXPECT_EQ(AppType::kArc, observer.app_type());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  auto all_apps = cache.GetAllApps();
  ASSERT_EQ(3u, all_apps.size());
  EXPECT_EQ("a", all_apps[0]->app_id);
  EXPECT_EQ("b", all_apps[1]->app_id);
  EXPECT_EQ("c", all_apps[2]->app_id);
}

// Verify the OnAppTypeInitialized callback when OnApps is called.
TEST_F(AppRegistryCacheTest, OnAppTypeInitializedWithUpdateFirst) {
  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<AppPtr> deltas1;
  deltas1.push_back(MakeApp("a", "avocado"));
  deltas1.push_back(MakeApp("c", "cucumber"));
  OnApps(cache, std::move(deltas1), AppType::kArc,
         true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the Apps are added.
  EXPECT_TRUE(base::Contains(observer1.app_types(), AppType::kArc));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian"));
  OnApps(cache, std::move(deltas2), AppType::kArc,
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

// Verify the OnAppTypeInitialized callback when OnApps is called for multiple
// App types.
TEST_F(AppRegistryCacheTest, OnAppTypeInitializedWithMultipleAppTypes) {
  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<AppPtr> deltas1;
  deltas1.push_back(MakeApp("a", "avocado"));
  deltas1.push_back(MakeApp("c", "cucumber"));
  OnApps(cache, std::move(deltas1), AppType::kArc,
         true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the Apps are added.
  EXPECT_EQ(1u, observer1.app_types().size());
  EXPECT_TRUE(base::Contains(observer1.app_types(), AppType::kArc));
  EXPECT_FALSE(base::Contains(observer1.app_types(), AppType::kChromeApp));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(2, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_FALSE(cache.IsAppTypeInitialized(AppType::kChromeApp));

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian", AppType::kChromeApp));
  OnApps(cache, std::move(deltas2), AppType::kChromeApp,
         true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the Apps are added.
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
// vector.
TEST_F(AppRegistryCacheTest, OnAppTypeInitializedWithEmptyUpdate) {
  AppRegistryCache cache;
  InitializedObserver observer1(&cache);

  std::vector<AppPtr> deltas1;
  OnApps(cache, std::move(deltas1), AppType::kStandaloneBrowserChromeApp,
         true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when the Apps are initialized.
  EXPECT_TRUE(base::Contains(observer1.app_types(),
                             AppType::kStandaloneBrowserChromeApp));
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kStandaloneBrowserChromeApp));

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("d", "durian"));
  OnApps(cache, std::move(deltas2), AppType::kStandaloneBrowserChromeApp,
         true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is not called when the Apps are initialized
  // again.
  EXPECT_EQ(1, observer1.initialized_app_type_count());
  EXPECT_EQ(0, observer1.app_count_at_initialization());
  EXPECT_EQ(1u, observer1.app_types().size());
  EXPECT_EQ(1u, cache.InitializedAppTypes().size());

  std::vector<AppPtr> deltas3;
  OnApps(cache, std::move(deltas3), AppType::kRemote,
         true /* should_notify_initialized */);

  // Verify OnAppTypeInitialized is called when both the Apps are initialized.
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

TEST_F(AppRegistryCacheTest, IsAppInstalledForInstalledApp) {
  AppRegistryCache cache;
  std::vector<AppPtr> deltas;
  deltas.push_back(MakeApp("a", "avocado", AppType::kArc, Readiness::kReady));
  deltas.push_back(
      MakeApp("b", "banana", AppType::kArc, Readiness::kDisabledByUser));
  OnApps(cache, std::move(deltas), AppType::kArc,
         true /* should_notify_initialized */);

  ASSERT_TRUE(cache.IsAppInstalled("a"));
  ASSERT_TRUE(cache.IsAppInstalled("b"));
}

TEST_F(AppRegistryCacheTest, IsAppInstalledForUninstalledApp) {
  AppRegistryCache cache;
  std::vector<AppPtr> deltas;
  deltas.push_back(
      MakeApp("a", "avocado", AppType::kArc, Readiness::kUninstalledByUser));
  OnApps(cache, std::move(deltas), AppType::kArc,
         true /* should_notify_initialized */);

  ASSERT_FALSE(cache.IsAppInstalled("a"));
  // App doesn't exist in the cache.
  ASSERT_FALSE(cache.IsAppInstalled("b"));
}

TEST_F(AppRegistryCacheTest, OnAppUpdateCount) {
  AppRegistryCache cache;
  cache.SetAccountId(account_id());

  base::ScopedObservation<AppRegistryCache, AppRegistryCache::Observer>
      observation{this};
  observation.Observe(&cache);

  std::vector<AppPtr> deltas1;
  deltas1.push_back(MakeApp("a", "avocado", AppType::kArc, Readiness::kReady));
  deltas1.push_back(MakeApp("a", "avocado", AppType::kArc, Readiness::kReady));
  OnApps(cache, std::move(deltas1), AppType::kArc,
         /* should_notify_initialized=*/true);

  // Verify OnAppUpdate is called once only, as the app info is the same.
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(1u, updated_ids_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_EQ(AppType::kArc, app_type());
  EXPECT_TRUE(cache.IsAppTypeInitialized(AppType::kArc));

  Clear();

  std::vector<AppPtr> deltas2;
  deltas2.push_back(MakeApp("a", "avocado", AppType::kArc, Readiness::kReady));
  deltas2.push_back(MakeApp("b", "banana", AppType::kArc, Readiness::kReady));
  OnApps(cache, std::move(deltas2), AppType::kArc,
         /* should_notify_initialized=*/true);

  // Verify OnAppUpdate is called once only for "b", as the app "a" has been
  // published.
  EXPECT_EQ(1, update_count());
  EXPECT_EQ(1u, updated_ids_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_EQ(AppType::kArc, app_type());

  std::vector<AppPtr> deltas3;
  deltas3.push_back(MakeApp("b", "banana", AppType::kArc, Readiness::kReady));
  OnApps(cache, std::move(deltas3), AppType::kArc,
         /* should_notify_initialized=*/false);

  Clear();

  // Verify OnAppUpdate is not called, as the app "b" has been published.
  EXPECT_EQ(0, update_count());
}

}  // namespace apps
