// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

class AppRegistryCacheTest : public testing::Test,
                             public apps::AppRegistryCache::Observer {
 protected:
  static apps::mojom::AppPtr MakeApp(
      const char* app_id,
      const char* name,
      apps::mojom::Readiness readiness = apps::mojom::Readiness::kUnknown) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_type = apps::mojom::AppType::kArc;
    app->app_id = app_id;
    app->readiness = readiness;
    app->name = name;
    return app;
  }

  void CallForEachApp(apps::AppRegistryCache& cache) {
    cache.ForEachApp(
        [this](const apps::AppUpdate& update) { OnAppUpdate(update); });
  }

  std::string GetName(apps::AppRegistryCache& cache,
                      const std::string& app_id) {
    std::string name;
    cache.ForOneApp(app_id, [&name](const apps::AppUpdate& update) {
      name = update.Name();
    });
    return name;
  }

  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& update) override {
    EXPECT_NE("", update.Name());
    if (update.ReadinessChanged() &&
        (update.Readiness() == apps::mojom::Readiness::kReady)) {
      num_freshly_installed_++;
    }
    updated_ids_.insert(update.AppId());
    updated_names_.insert(update.Name());
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    // The test code explicitly calls both AddObserver and RemoveObserver.
    NOTREACHED();
  }

  int num_freshly_installed_ = 0;
  std::set<std::string> updated_ids_;
  std::set<std::string> updated_names_;
};

// Responds to a cache's OnAppUpdate to call back into the cache, checking that
// the cache presents a self-consistent snapshot. For example, the app names
// should match for the outer and inner AppUpdate.
//
// In the tests below, just "recursive" means that cache.OnApps calls
// observer.OnAppsUpdate which calls cache.ForEachApp and cache.ForOneApp.
// "Super-recursive" means that cache.OnApps calls observer.OnAppsUpdate calls
// cache.OnApps which calls observer.OnAppsUpdate.
class RecursiveObserver : public apps::AppRegistryCache::Observer {
 public:
  explicit RecursiveObserver(apps::AppRegistryCache* cache) : cache_(cache) {
    Observe(cache);
  }

  ~RecursiveObserver() override = default;

  void PrepareForOnApps(
      int expected_num_apps,
      const std::string& expected_name_for_p,
      std::vector<apps::mojom::AppPtr>* super_recursive_apps = nullptr) {
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

 protected:
  // apps::AppRegistryCache::Observer overrides.
  void OnAppUpdate(const apps::AppUpdate& outer) override {
    int num_apps = 0;
    cache_->ForEachApp([this, &outer, &num_apps](const apps::AppUpdate& inner) {
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
        [&outer](const apps::AppUpdate& inner) { ExpectEq(outer, inner); }));

    EXPECT_TRUE(cache_->ForOneApp(
        outer.AppId(),
        [&outer](const apps::AppUpdate& inner) { ExpectEq(outer, inner); }));

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

    std::vector<apps::mojom::AppPtr> super_recursive;
    while (!super_recursive_apps_.empty()) {
      apps::mojom::AppPtr app = std::move(super_recursive_apps_.back());
      super_recursive_apps_.pop_back();
      if (app.get() == nullptr) {
        // This is the placeholder 'punctuation'.
        break;
      }
      super_recursive.push_back(std::move(app));
    }
    if (!super_recursive.empty()) {
      cache_->OnApps(std::move(super_recursive));
    }

    num_apps_seen_on_app_update_++;
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    Observe(nullptr);
  }

  static void ExpectEq(const apps::AppUpdate& outer,
                       const apps::AppUpdate& inner) {
    EXPECT_EQ(outer.AppType(), inner.AppType());
    EXPECT_EQ(outer.AppId(), inner.AppId());
    EXPECT_EQ(outer.StateIsNull(), inner.StateIsNull());
    EXPECT_EQ(outer.Readiness(), inner.Readiness());
    EXPECT_EQ(outer.Name(), inner.Name());
  }

  apps::AppRegistryCache* cache_;
  std::string expected_name_for_p_;
  int expected_num_apps_;
  int num_apps_seen_on_app_update_;

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
  std::vector<apps::mojom::AppPtr> super_recursive_apps_;

  // For non-super-recursive tests (i.e. for check_names_snapshot_ == true), we
  // check that the "app_id to name" mapping is consistent across every
  // OnAppsUpdate call to this observer. For super-recursive tests, that
  // mapping can change as updates are processed, so the names_snapshot_ check
  // is skipped.
  bool check_names_snapshot_ = false;
  std::map<std::string, std::string> names_snapshot_;
};

TEST_F(AppRegistryCacheTest, ForEachApp) {
  std::vector<apps::mojom::AppPtr> deltas;
  apps::AppRegistryCache cache;

  updated_names_.clear();
  CallForEachApp(cache);

  EXPECT_EQ(0u, updated_names_.size());

  deltas.clear();
  deltas.push_back(MakeApp("a", "apple"));
  deltas.push_back(MakeApp("b", "banana"));
  deltas.push_back(MakeApp("c", "cherry"));
  cache.OnApps(std::move(deltas));

  updated_names_.clear();
  CallForEachApp(cache);

  EXPECT_EQ(3u, updated_names_.size());
  EXPECT_NE(updated_names_.end(), updated_names_.find("apple"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("banana"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("cherry"));

  deltas.clear();
  deltas.push_back(MakeApp("a", "apricot"));
  deltas.push_back(MakeApp("d", "durian"));
  cache.OnApps(std::move(deltas));

  updated_names_.clear();
  CallForEachApp(cache);

  EXPECT_EQ(4u, updated_names_.size());
  EXPECT_NE(updated_names_.end(), updated_names_.find("apricot"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("banana"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("cherry"));
  EXPECT_NE(updated_names_.end(), updated_names_.find("durian"));

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
}

TEST_F(AppRegistryCacheTest, Observer) {
  std::vector<apps::mojom::AppPtr> deltas;
  apps::AppRegistryCache cache;

  cache.AddObserver(this);

  num_freshly_installed_ = 0;
  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(MakeApp("a", "avocado"));
  deltas.push_back(MakeApp("c", "cucumber"));
  deltas.push_back(MakeApp("e", "eggfruit"));
  cache.OnApps(std::move(deltas));

  EXPECT_EQ(0, num_freshly_installed_);
  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("a"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("e"));

  num_freshly_installed_ = 0;
  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(MakeApp("b", "blueberry"));
  deltas.push_back(MakeApp("c", "cucumber", apps::mojom::Readiness::kReady));
  cache.OnApps(std::move(deltas));

  EXPECT_EQ(1, num_freshly_installed_);
  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("b"));
  EXPECT_NE(updated_ids_.end(), updated_ids_.find("c"));

  cache.RemoveObserver(this);

  num_freshly_installed_ = 0;
  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(MakeApp("f", "fig"));
  cache.OnApps(std::move(deltas));

  EXPECT_EQ(0, num_freshly_installed_);
  EXPECT_EQ(0u, updated_ids_.size());
}

TEST_F(AppRegistryCacheTest, Recursive) {
  std::vector<apps::mojom::AppPtr> deltas;
  apps::AppRegistryCache cache;
  RecursiveObserver observer(&cache);

  observer.PrepareForOnApps(2, "peach");
  deltas.clear();
  deltas.push_back(MakeApp("o", "orange"));
  deltas.push_back(MakeApp("p", "peach"));
  cache.OnApps(std::move(deltas));
  EXPECT_EQ(2, observer.NumAppsSeenOnAppUpdate());

  observer.PrepareForOnApps(3, "pear");
  deltas.clear();
  deltas.push_back(MakeApp("p", "pear", apps::mojom::Readiness::kReady));
  deltas.push_back(MakeApp("q", "quince"));
  cache.OnApps(std::move(deltas));
  EXPECT_EQ(2, observer.NumAppsSeenOnAppUpdate());

  observer.PrepareForOnApps(3, "plum");
  deltas.clear();
  deltas.push_back(MakeApp("p", "pear"));
  deltas.push_back(MakeApp("p", "pear"));
  deltas.push_back(MakeApp("p", "plum"));
  cache.OnApps(std::move(deltas));
  EXPECT_EQ(1, observer.NumAppsSeenOnAppUpdate());
}

TEST_F(AppRegistryCacheTest, SuperRecursive) {
  std::vector<apps::mojom::AppPtr> deltas;
  apps::AppRegistryCache cache;
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
  std::vector<apps::mojom::AppPtr> super_recursive_apps;
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
  cache.OnApps(std::move(deltas));

  // After all of that, check that for each app_id, the last delta won.
  EXPECT_EQ("avocado", GetName(cache, "a"));
  EXPECT_EQ("boysenberry", GetName(cache, "b"));
  EXPECT_EQ("coconut", GetName(cache, "c"));
}
