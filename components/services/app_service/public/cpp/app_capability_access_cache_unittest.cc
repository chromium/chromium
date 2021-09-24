// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/memory/raw_ptr.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class AppCapabilityAccessCacheTest
    : public testing::Test,
      public apps::AppCapabilityAccessCache::Observer {
 protected:
  static apps::mojom::CapabilityAccessPtr MakeCapabilityAccess(
      const char* app_id,
      apps::mojom::OptionalBool camera,
      apps::mojom::OptionalBool microphone) {
    apps::mojom::CapabilityAccessPtr access =
        apps::mojom::CapabilityAccess::New();
    access->app_id = app_id;
    access->camera = camera;
    access->microphone = microphone;
    return access;
  }

  void CallForEachApp(apps::AppCapabilityAccessCache& cache) {
    cache.ForEachApp([this](const apps::CapabilityAccessUpdate& update) {
      OnCapabilityAccessUpdate(update);
    });
  }

  // apps::AppCapabilityAccessCache::Observer overrides.
  void OnCapabilityAccessUpdate(
      const apps::CapabilityAccessUpdate& update) override {
    EXPECT_EQ(account_id_, update.AccountId());
    updated_ids_.insert(update.AppId());
    if (update.Camera() == apps::mojom::OptionalBool::kTrue) {
      accessing_camera_apps_.insert(update.AppId());
    } else {
      accessing_camera_apps_.erase(update.AppId());
    }
    if (update.Microphone() == apps::mojom::OptionalBool::kTrue) {
      accessing_microphone_apps_.insert(update.AppId());
    } else {
      accessing_microphone_apps_.erase(update.AppId());
    }
  }

  void OnAppCapabilityAccessCacheWillBeDestroyed(
      apps::AppCapabilityAccessCache* cache) override {
    // The test code explicitly calls both AddObserver and RemoveObserver.
    NOTREACHED();
  }

  const AccountId& account_id() const { return account_id_; }

  std::set<std::string> updated_ids_;
  std::set<std::string> accessing_camera_apps_;
  std::set<std::string> accessing_microphone_apps_;
  AccountId account_id_ = AccountId::FromUserEmail("test@gmail.com");
};

// Responds to a app_capability_access's OnCapabilityAccessUpdate to call back
// into AppCapabilityAccessCache, checking that AppCapabilityAccessCache
// presents a self-consistent snapshot. For example, the camera should match for
// the outer and inner CapabilityAccessUpdate.
//
// In the tests below, just "recursive" means that
// app_capability_access.OnCapabilityAccesses calls
// observer.OnCapabilityAccessUpdate which calls
// app_capability_access.ForEachApp and app_capability_access.ForOneApp.
// "Super-recursive" means that app_capability_access.OnCapabilityAccesses calls
// observer.OnCapabilityAccessUpdate calls
// app_capability_access.OnCapabilityAccesses which calls
// observer.OnCapabilityAccessUpdate.
class CapabilityAccessRecursiveObserver
    : public apps::AppCapabilityAccessCache::Observer {
 public:
  explicit CapabilityAccessRecursiveObserver(
      apps::AppCapabilityAccessCache* cache)
      : cache_(cache) {
    Observe(cache);
  }

  ~CapabilityAccessRecursiveObserver() override = default;

  void PrepareForOnCapabilityAccesses(
      int expected_num_apps,
      std::vector<apps::mojom::CapabilityAccessPtr>* super_recursive_accesses =
          nullptr) {
    expected_num_apps_ = expected_num_apps;
    num_apps_seen_on_capability_access_update_ = 0;

    if (super_recursive_accesses) {
      super_recursive_accesses_.swap(*super_recursive_accesses);
    }
  }

  int NumAppsSeenOnCapabilityAccessUpdate() {
    return num_apps_seen_on_capability_access_update_;
  }

  const std::set<std::string>& accessing_camera_apps() {
    return accessing_camera_apps_;
  }

  const std::set<std::string>& accessing_microphone_apps() {
    return accessing_microphone_apps_;
  }

 protected:
  // apps::AppCapabilityAccessCache::Observer overrides.
  void OnCapabilityAccessUpdate(
      const apps::CapabilityAccessUpdate& outer) override {
    EXPECT_EQ(account_id_, outer.AccountId());
    int num_apps = 0;
    cache_->ForEachApp(
        [this, &outer, &num_apps](const apps::CapabilityAccessUpdate& inner) {
          if (inner.Camera() == apps::mojom::OptionalBool::kTrue) {
            accessing_camera_apps_.insert(inner.AppId());
          } else {
            accessing_camera_apps_.erase(inner.AppId());
          }
          if (inner.Microphone() == apps::mojom::OptionalBool::kTrue) {
            accessing_microphone_apps_.insert(inner.AppId());
          } else {
            accessing_microphone_apps_.erase(inner.AppId());
          }

          if (outer.AppId() == inner.AppId()) {
            ExpectEq(outer, inner);
          }

          num_apps++;
        });
    EXPECT_EQ(expected_num_apps_, num_apps);

    EXPECT_FALSE(cache_->ForOneApp(
        "no_such_app_id", [&outer](const apps::CapabilityAccessUpdate& inner) {
          ExpectEq(outer, inner);
        }));

    EXPECT_TRUE(cache_->ForOneApp(
        outer.AppId(), [&outer](const apps::CapabilityAccessUpdate& inner) {
          ExpectEq(outer, inner);
        }));

    std::vector<apps::mojom::CapabilityAccessPtr> super_recursive;
    while (!super_recursive_accesses_.empty()) {
      apps::mojom::CapabilityAccessPtr access =
          std::move(super_recursive_accesses_.back());
      super_recursive_accesses_.pop_back();
      if (access.get() == nullptr) {
        // This is the placeholder 'punctuation'.
        break;
      }
      super_recursive.push_back(std::move(access));
    }
    if (!super_recursive.empty()) {
      cache_->OnCapabilityAccesses(std::move(super_recursive));
    }

    num_apps_seen_on_capability_access_update_++;
  }

  void OnAppCapabilityAccessCacheWillBeDestroyed(
      apps::AppCapabilityAccessCache* cache) override {
    Observe(nullptr);
  }

  static void ExpectEq(const apps::CapabilityAccessUpdate& outer,
                       const apps::CapabilityAccessUpdate& inner) {
    EXPECT_EQ(outer.AppId(), inner.AppId());
    EXPECT_EQ(outer.StateIsNull(), inner.StateIsNull());
    EXPECT_EQ(outer.Camera(), inner.Camera());
    EXPECT_EQ(outer.Microphone(), inner.Microphone());
  }

 private:
  raw_ptr<apps::AppCapabilityAccessCache> cache_;
  AccountId account_id_ = AccountId::FromUserEmail("test@gmail.com");
  std::set<std::string> accessing_camera_apps_;
  std::set<std::string> accessing_microphone_apps_;

  int expected_num_apps_;
  int num_apps_seen_on_capability_access_update_;

  // Non-empty when this.OnCapabilityAccessUpdate should trigger more
  // app_capability_access_.OnCapabilityAccesses calls.
  //
  // During OnCapabilityAccessUpdate, this vector (a stack) is popped from the
  // back until a nullptr 'punctuation' element (a group terminator) is seen. If
  // that group of popped elements (in LIFO order) is non-empty, that group
  // forms the vector of CapabilityAccess's passed to
  // app_capability_access_.OnCapabilityAccesses.
  std::vector<apps::mojom::CapabilityAccessPtr> super_recursive_accesses_;
};

TEST_F(AppCapabilityAccessCacheTest, ForEachApp) {
  std::vector<apps::mojom::CapabilityAccessPtr> deltas;
  apps::AppCapabilityAccessCache cache;
  cache.SetAccountId(account_id());

  CallForEachApp(cache);

  EXPECT_EQ(0u, updated_ids_.size());

  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("a", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kTrue));
  deltas.push_back(MakeCapabilityAccess("b", apps::mojom::OptionalBool::kFalse,
                                        apps::mojom::OptionalBool::kFalse));
  deltas.push_back(MakeCapabilityAccess("c", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kFalse));
  cache.OnCapabilityAccesses(std::move(deltas));

  updated_ids_.clear();
  CallForEachApp(cache);

  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_EQ(2u, accessing_camera_apps_.size());
  EXPECT_EQ(cache.GetAppsAccessingCamera(), accessing_camera_apps_);
  EXPECT_EQ(1u, accessing_microphone_apps_.size());
  EXPECT_EQ(cache.GetAppsAccessingMicrophone(), accessing_microphone_apps_);

  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("a", apps::mojom::OptionalBool::kFalse,
                                        apps::mojom::OptionalBool::kFalse));
  deltas.push_back(MakeCapabilityAccess("d", apps::mojom::OptionalBool::kFalse,
                                        apps::mojom::OptionalBool::kTrue));
  cache.OnCapabilityAccesses(std::move(deltas));

  updated_ids_.clear();
  CallForEachApp(cache);

  EXPECT_EQ(4u, updated_ids_.size());
  EXPECT_EQ(1u, accessing_camera_apps_.size());
  EXPECT_EQ(cache.GetAppsAccessingCamera(), accessing_camera_apps_);
  EXPECT_EQ(1u, accessing_microphone_apps_.size());
  EXPECT_EQ(cache.GetAppsAccessingMicrophone(), accessing_microphone_apps_);

  // Test that ForOneApp succeeds for "c" and fails for "e".

  bool found_c = false;
  EXPECT_TRUE(cache.ForOneApp(
      "c", [&found_c](const apps::CapabilityAccessUpdate& update) {
        found_c = true;
        EXPECT_EQ("c", update.AppId());
        EXPECT_EQ(apps::mojom::OptionalBool::kTrue, update.Camera());
        EXPECT_EQ(apps::mojom::OptionalBool::kFalse, update.Microphone());
      }));
  EXPECT_TRUE(found_c);

  bool found_e = false;
  EXPECT_FALSE(cache.ForOneApp(
      "e", [&found_e](const apps::CapabilityAccessUpdate& update) {
        found_e = true;
        EXPECT_EQ("e", update.AppId());
      }));
  EXPECT_FALSE(found_e);
}

TEST_F(AppCapabilityAccessCacheTest, Observer) {
  std::vector<apps::mojom::CapabilityAccessPtr> deltas;
  apps::AppCapabilityAccessCache cache;
  cache.SetAccountId(account_id());

  cache.AddObserver(this);

  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("a", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kTrue));
  deltas.push_back(MakeCapabilityAccess("b", apps::mojom::OptionalBool::kFalse,
                                        apps::mojom::OptionalBool::kFalse));
  deltas.push_back(MakeCapabilityAccess("c", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kFalse));
  cache.OnCapabilityAccesses(std::move(deltas));

  EXPECT_EQ(3u, updated_ids_.size());
  EXPECT_EQ(2u, accessing_camera_apps_.size());
  EXPECT_EQ(cache.GetAppsAccessingCamera(), accessing_camera_apps_);
  EXPECT_EQ(1u, accessing_microphone_apps_.size());
  EXPECT_EQ(cache.GetAppsAccessingMicrophone(), accessing_microphone_apps_);

  updated_ids_.clear();
  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("b", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kTrue));
  deltas.push_back(MakeCapabilityAccess("c", apps::mojom::OptionalBool::kFalse,
                                        apps::mojom::OptionalBool::kTrue));
  cache.OnCapabilityAccesses(std::move(deltas));

  EXPECT_EQ(2u, updated_ids_.size());
  EXPECT_EQ(2u, accessing_camera_apps_.size());
  EXPECT_EQ(cache.GetAppsAccessingCamera(), accessing_camera_apps_);
  EXPECT_EQ(3u, accessing_microphone_apps_.size());
  EXPECT_EQ(cache.GetAppsAccessingMicrophone(), accessing_microphone_apps_);

  cache.RemoveObserver(this);

  updated_ids_.clear();
  accessing_camera_apps_.clear();
  accessing_microphone_apps_.clear();
  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("d", apps::mojom::OptionalBool::kFalse,
                                        apps::mojom::OptionalBool::kTrue));
  cache.OnCapabilityAccesses(std::move(deltas));

  EXPECT_EQ(0u, accessing_camera_apps_.size());
  EXPECT_EQ(0u, accessing_microphone_apps_.size());
}

TEST_F(AppCapabilityAccessCacheTest, Recursive) {
  std::vector<apps::mojom::CapabilityAccessPtr> deltas;
  apps::AppCapabilityAccessCache cache;
  cache.SetAccountId(account_id());
  CapabilityAccessRecursiveObserver observer(&cache);

  observer.PrepareForOnCapabilityAccesses(2);
  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("a", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kFalse));
  deltas.push_back(MakeCapabilityAccess("b", apps::mojom::OptionalBool::kFalse,
                                        apps::mojom::OptionalBool::kTrue));
  cache.OnCapabilityAccesses(std::move(deltas));
  EXPECT_EQ(2, observer.NumAppsSeenOnCapabilityAccessUpdate());

  observer.PrepareForOnCapabilityAccesses(3);
  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("b", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kFalse));
  deltas.push_back(MakeCapabilityAccess("c", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kTrue));
  cache.OnCapabilityAccesses(std::move(deltas));
  EXPECT_EQ(2, observer.NumAppsSeenOnCapabilityAccessUpdate());

  observer.PrepareForOnCapabilityAccesses(3);
  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("b", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kFalse));
  deltas.push_back(MakeCapabilityAccess("b", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kFalse));
  deltas.push_back(MakeCapabilityAccess("b", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kTrue));
  cache.OnCapabilityAccesses(std::move(deltas));
  EXPECT_EQ(1, observer.NumAppsSeenOnCapabilityAccessUpdate());

  EXPECT_EQ(cache.GetAppsAccessingCamera(), observer.accessing_camera_apps());
  EXPECT_EQ(cache.GetAppsAccessingMicrophone(),
            observer.accessing_microphone_apps());
}

TEST_F(AppCapabilityAccessCacheTest, SuperRecursive) {
  std::vector<apps::mojom::CapabilityAccessPtr> deltas;
  apps::AppCapabilityAccessCache cache;
  cache.SetAccountId(account_id());
  CapabilityAccessRecursiveObserver observer(&cache);

  // Set up a series of OnCapabilityAccesses to be called during
  // observer.OnCapabilityAccessUpdate:
  //  - the 1st update is {"b, "c"}.
  //  - the 2nd update is {}.
  //  - the 3rd update is {"b", "c", "b"}.
  //  - the 4th update is {"a"}.
  //  - the 5th update is {}.
  //  - the 6th update is {"b"}.
  //
  // The vector is processed in LIFO order with nullptr punctuation to
  // terminate each group. See the comment on the
  // RecursiveObserver::super_recursive_accesses_ field.
  std::vector<apps::mojom::CapabilityAccessPtr> super_recursive_accesses;
  super_recursive_accesses.push_back(nullptr);
  super_recursive_accesses.push_back(MakeCapabilityAccess(
      "b", apps::mojom::OptionalBool::kTrue, apps::mojom::OptionalBool::kTrue));
  super_recursive_accesses.push_back(nullptr);
  super_recursive_accesses.push_back(nullptr);
  super_recursive_accesses.push_back(MakeCapabilityAccess(
      "a", apps::mojom::OptionalBool::kTrue, apps::mojom::OptionalBool::kTrue));
  super_recursive_accesses.push_back(nullptr);
  super_recursive_accesses.push_back(
      MakeCapabilityAccess("b", apps::mojom::OptionalBool::kTrue,
                           apps::mojom::OptionalBool::kFalse));
  super_recursive_accesses.push_back(
      MakeCapabilityAccess("a", apps::mojom::OptionalBool::kFalse,
                           apps::mojom::OptionalBool::kFalse));
  super_recursive_accesses.push_back(
      MakeCapabilityAccess("b", apps::mojom::OptionalBool::kFalse,
                           apps::mojom::OptionalBool::kFalse));
  super_recursive_accesses.push_back(nullptr);
  super_recursive_accesses.push_back(nullptr);
  super_recursive_accesses.push_back(MakeCapabilityAccess(
      "c", apps::mojom::OptionalBool::kTrue, apps::mojom::OptionalBool::kTrue));
  super_recursive_accesses.push_back(MakeCapabilityAccess(
      "b", apps::mojom::OptionalBool::kTrue, apps::mojom::OptionalBool::kTrue));

  observer.PrepareForOnCapabilityAccesses(3, &super_recursive_accesses);
  deltas.clear();
  deltas.push_back(MakeCapabilityAccess("a", apps::mojom::OptionalBool::kTrue,
                                        apps::mojom::OptionalBool::kFalse));
  deltas.push_back(MakeCapabilityAccess("b", apps::mojom::OptionalBool::kFalse,
                                        apps::mojom::OptionalBool::kTrue));
  deltas.push_back(MakeCapabilityAccess("c", apps::mojom::OptionalBool::kFalse,
                                        apps::mojom::OptionalBool::kFalse));
  cache.OnCapabilityAccesses(std::move(deltas));

  // After all of that, check that for each app_id, the last delta won.
  EXPECT_EQ(3u, observer.accessing_camera_apps().size());
  EXPECT_NE(observer.accessing_camera_apps().end(),
            observer.accessing_camera_apps().find("a"));
  EXPECT_NE(observer.accessing_camera_apps().end(),
            observer.accessing_camera_apps().find("b"));
  EXPECT_NE(observer.accessing_camera_apps().end(),
            observer.accessing_camera_apps().find("c"));

  EXPECT_EQ(3u, observer.accessing_microphone_apps().size());
  EXPECT_NE(observer.accessing_microphone_apps().end(),
            observer.accessing_microphone_apps().find("a"));
  EXPECT_NE(observer.accessing_microphone_apps().end(),
            observer.accessing_microphone_apps().find("b"));
  EXPECT_NE(observer.accessing_microphone_apps().end(),
            observer.accessing_microphone_apps().find("c"));

  EXPECT_EQ(cache.GetAppsAccessingCamera(), observer.accessing_camera_apps());
  EXPECT_EQ(cache.GetAppsAccessingMicrophone(),
            observer.accessing_microphone_apps());
}
