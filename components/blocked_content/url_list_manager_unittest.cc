// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/url_list_manager.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blocked_content {
namespace {

// Observer that resets its ScopedObservation when the manager is destroyed,
// mirroring what UI surfaces (e.g. content setting bubbles) that can outlive
// the manager must do.
class ResettingObserver : public UrlListManager::Observer {
 public:
  explicit ResettingObserver(UrlListManager* manager) {
    observation_.Observe(manager);
  }

  // UrlListManager::Observer:
  void BlockedUrlAdded(int32_t id, const GURL& url) override {
    added_ids_.push_back(id);
  }
  void OnUrlListManagerDestroying(UrlListManager* manager) override {
    ++destroying_count_;
    destroying_manager_address_ = reinterpret_cast<uintptr_t>(manager);
    EXPECT_TRUE(observation_.IsObservingSource(manager));
    observation_.Reset();
  }

  bool IsObserving() const { return observation_.IsObserving(); }
  int destroying_count() const { return destroying_count_; }
  // The address is recorded as an integer instead of a raw_ptr because the
  // manager is destroyed right after the notification.
  uintptr_t destroying_manager_address() const {
    return destroying_manager_address_;
  }
  const std::vector<int32_t>& added_ids() const { return added_ids_; }

 private:
  std::vector<int32_t> added_ids_;
  int destroying_count_ = 0;
  uintptr_t destroying_manager_address_ = 0;
  base::ScopedObservation<UrlListManager, UrlListManager::Observer>
      observation_{this};
};

// Observer that relies on the default (no-op) OnUrlListManagerDestroying and
// unregisters manually before the manager goes away.
class PassiveObserver : public UrlListManager::Observer {
 public:
  void BlockedUrlAdded(int32_t id, const GURL& url) override { ++added_; }
  int added() const { return added_; }

 private:
  int added_ = 0;
};

TEST(UrlListManagerTest, NotifiesBlockedUrlAdded) {
  auto manager = std::make_unique<UrlListManager>();
  ResettingObserver observer(manager.get());

  manager->NotifyObservers(1, GURL("https://a.test"));
  manager->NotifyObservers(2, GURL("https://b.test"));

  EXPECT_EQ((std::vector<int32_t>{1, 2}), observer.added_ids());
  EXPECT_EQ(0, observer.destroying_count());
}

// Tests that an observer that outlives the manager is told about the
// manager's destruction and can stop observing.
TEST(UrlListManagerTest, ObserverOutlivingManagerIsNotifiedAndCanReset) {
  auto manager = std::make_unique<UrlListManager>();
  const uintptr_t manager_address = reinterpret_cast<uintptr_t>(manager.get());
  auto observer = std::make_unique<ResettingObserver>(manager.get());
  ASSERT_TRUE(observer->IsObserving());

  manager.reset();

  EXPECT_EQ(1, observer->destroying_count());
  EXPECT_EQ(manager_address, observer->destroying_manager_address());
  EXPECT_FALSE(observer->IsObserving());

  // Destroying the observer after the manager must not touch the manager.
  observer.reset();
}

TEST(UrlListManagerTest, AllObserversNotifiedWhileResettingDuringIteration) {
  auto manager = std::make_unique<UrlListManager>();
  ResettingObserver observer1(manager.get());
  ResettingObserver observer2(manager.get());
  ResettingObserver observer3(manager.get());

  // Every observer removes itself from within the destruction notification;
  // the iteration must still reach all of them exactly once.
  manager.reset();

  for (const ResettingObserver* observer :
       {&observer1, &observer2, &observer3}) {
    EXPECT_EQ(1, observer->destroying_count());
    EXPECT_FALSE(observer->IsObserving());
  }
}

TEST(UrlListManagerTest, RemovedObserverIsNotNotifiedOfDestruction) {
  auto manager = std::make_unique<UrlListManager>();
  auto observer = std::make_unique<ResettingObserver>(manager.get());
  ResettingObserver remaining(manager.get());

  // An observer destroyed before the manager unregisters itself normally.
  observer.reset();
  manager->NotifyObservers(7, GURL("https://c.test"));
  manager.reset();

  EXPECT_EQ((std::vector<int32_t>{7}), remaining.added_ids());
  EXPECT_EQ(1, remaining.destroying_count());
}

TEST(UrlListManagerTest, DefaultDestroyingNotificationIsNoOp) {
  auto manager = std::make_unique<UrlListManager>();
  PassiveObserver passive;
  ResettingObserver resetting(manager.get());
  manager->AddObserver(&passive);

  manager->NotifyObservers(3, GURL("https://d.test"));
  EXPECT_EQ(1, passive.added());

  // The passive observer does not override OnUrlListManagerDestroying(); the
  // manager must still notify everyone else.
  manager.reset();
  EXPECT_EQ(1, resetting.destroying_count());
}

TEST(UrlListManagerTest, ManagerWithoutObserversDestroysCleanly) {
  auto manager = std::make_unique<UrlListManager>();
  manager->NotifyObservers(1, GURL("https://e.test"));
  manager.reset();
}

}  // namespace
}  // namespace blocked_content
