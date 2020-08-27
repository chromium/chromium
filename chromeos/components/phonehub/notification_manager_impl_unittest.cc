// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_manager_impl.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

enum class NotificationState { kAdded, kUpdated, kRemoved };

class FakeObserver : public NotificationManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  base::Optional<NotificationState> GetState(int64_t notification_id) const {
    const auto it = id_to_state_map_.find(notification_id);
    if (it == id_to_state_map_.end())
      return base::nullopt;
    return it->second;
  }

 private:
  // NotificationManager::Observer:
  void OnNotificationsAdded(
      const base::flat_set<int64_t>& notification_ids) override {
    for (int64_t id : notification_ids)
      id_to_state_map_[id] = NotificationState::kAdded;
  }

  void OnNotificationsUpdated(
      const base::flat_set<int64_t>& notification_ids) override {
    for (int64_t id : notification_ids)
      id_to_state_map_[id] = NotificationState::kUpdated;
  }

  void OnNotificationsRemoved(
      const base::flat_set<int64_t>& notification_ids) override {
    for (int64_t id : notification_ids)
      id_to_state_map_[id] = NotificationState::kRemoved;
  }

  base::flat_map<int64_t, NotificationState> id_to_state_map_;
};

}  // namespace

class NotificationManagerImplTest : public testing::Test {
 protected:
  NotificationManagerImplTest() = default;
  NotificationManagerImplTest(const NotificationManagerImplTest&) = delete;
  NotificationManagerImplTest& operator=(const NotificationManagerImplTest&) =
      delete;
  ~NotificationManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<NotificationManagerImpl>();
    manager_->AddObserver(&fake_observer_);
  }

  void TearDown() override { manager_->RemoveObserver(&fake_observer_); }

  NotificationManager& manager() { return *manager_; }

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<NotificationManager> manager_;
};

// TODO(khorimoto): Remove this test once we have real functionality to test.
TEST_F(NotificationManagerImplTest, Initialize) {
  EXPECT_FALSE(manager().GetNotification(/*notification_id=*/0));
}

}  // namespace phonehub
}  // namespace chromeos
