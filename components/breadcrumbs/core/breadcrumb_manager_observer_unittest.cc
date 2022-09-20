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
  void EventAdded(BreadcrumbManager* manager,
                  const std::string& event) override {
    event_added_last_received_manager_ = manager;
    event_added_last_received_event_ = event;
  }

  void OldEventsRemoved(BreadcrumbManager* manager) override {
    old_events_removed_last_received_manager_ = manager;
  }

  raw_ptr<BreadcrumbManager> event_added_last_received_manager_ = nullptr;
  std::string event_added_last_received_event_;

  raw_ptr<BreadcrumbManager> old_events_removed_last_received_manager_ =
      nullptr;
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

// Tests that |BreadcrumbManagerObserver::EventAdded| is called when an event to
// added to |manager_|.
TEST_F(BreadcrumbManagerObserverTest, EventAdded) {
  ASSERT_FALSE(observer_.event_added_last_received_manager_);
  ASSERT_TRUE(observer_.event_added_last_received_event_.empty());

  const std::string event = "event";
  BreadcrumbManager::GetInstance().AddEvent(event);

  EXPECT_EQ(&BreadcrumbManager::GetInstance(),
            observer_.event_added_last_received_manager_);
  // A timestamp will be prepended to the event passed to |AddEvent|.
  EXPECT_NE(std::string::npos,
            observer_.event_added_last_received_event_.find(event));
}

// Tests that |BreadcumbManager::OldEventsRemoved| is called when old events are
// dropped from |manager_|.
TEST_F(BreadcrumbManagerObserverTest, OldEventsRemoved) {
  ASSERT_FALSE(observer_.old_events_removed_last_received_manager_);

  const std::string event = "event";
  BreadcrumbManager::GetInstance().AddEvent(event);
  task_env_.FastForwardBy(base::Hours(1));
  BreadcrumbManager::GetInstance().AddEvent(event);
  task_env_.FastForwardBy(base::Hours(1));
  BreadcrumbManager::GetInstance().AddEvent(event);

  EXPECT_EQ(&BreadcrumbManager::GetInstance(),
            observer_.old_events_removed_last_received_manager_);
}

}  // namespace breadcrumbs
