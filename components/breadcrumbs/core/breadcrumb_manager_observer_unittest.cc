// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace breadcrumbs {

namespace {

class FakeBreadcrumbManagerObserver : public BreadcrumbManagerObserver {
 public:
  FakeBreadcrumbManagerObserver() = default;
  ~FakeBreadcrumbManagerObserver() override = default;

  FakeBreadcrumbManagerObserver(const FakeBreadcrumbManagerObserver&) = delete;
  FakeBreadcrumbManagerObserver& operator=(
      const FakeBreadcrumbManagerObserver&) = delete;

  // BreadcrumbManagerObserver
  void EventAdded(const std::string& event) override {
    event_added_count_++;
    event_added_last_received_event_ = event;
  }

  void OldEventsRemoved() override { old_events_removed_count_++; }

  size_t event_added_count_ = 0u;
  std::string event_added_last_received_event_;

  size_t old_events_removed_count_ = 0u;
};

}  // namespace

class BreadcrumbManagerObserverTest : public PlatformTest {
 protected:
  BreadcrumbManagerObserverTest() = default;
  ~BreadcrumbManagerObserverTest() override = default;

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeBreadcrumbManagerObserver observer_;
};

// Tests that `BreadcrumbManagerObserver::EventAdded` is called when an event to
// added to the BreadcrumbManager.
TEST_F(BreadcrumbManagerObserverTest, EventAdded) {
  ASSERT_EQ(0u, observer_.event_added_count_);
  ASSERT_TRUE(observer_.event_added_last_received_event_.empty());

  const std::string event = "event";
  BreadcrumbManager::GetInstance().AddEvent(event);

  EXPECT_EQ(1u, observer_.event_added_count_);
  // A timestamp will be prepended to the event passed to `AddEvent`.
  EXPECT_NE(std::string::npos,
            observer_.event_added_last_received_event_.find(event));
}

// Tests that `BreadcumbManager::OldEventsRemoved` is called when old events are
// dropped from the BreadcrumbManager.
TEST_F(BreadcrumbManagerObserverTest, OldEventsRemoved) {
  ASSERT_EQ(0u, observer_.old_events_removed_count_);

  const std::string event = "event";
  BreadcrumbManager::GetInstance().AddEvent(event);
  task_env_.FastForwardBy(base::Hours(1));
  BreadcrumbManager::GetInstance().AddEvent(event);
  task_env_.FastForwardBy(base::Hours(1));
  BreadcrumbManager::GetInstance().AddEvent(event);

  EXPECT_EQ(1u, observer_.old_events_removed_count_);
}

}  // namespace breadcrumbs
