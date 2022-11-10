// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager.h"

#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace breadcrumbs {

namespace {

// Adds `event` to the BreadcrumbManager.
void AddEvent(const std::string& event) {
  BreadcrumbManager::GetInstance().AddEvent(event);
}

}  // namespace

// Test fixture for testing BreadcrumbManager class.
class BreadcrumbManagerTest : public PlatformTest {
 protected:
  BreadcrumbManagerTest() = default;

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that an event is logged and returned.
TEST_F(BreadcrumbManagerTest, AddEvent) {
  const std::string event_message = "event";
  AddEvent(event_message);
  const auto& events = BreadcrumbManager::GetInstance().GetEvents();
  ASSERT_EQ(1u, events.size());
  // Events returned from `GetEvents` will have a timestamp prepended.
  EXPECT_EQ("0:00:00 event", events.front());
}

// Tests that no more than `kMaxBreadcrumbs` events are stored.
TEST_F(BreadcrumbManagerTest, MaxEvents) {
  const auto& events = BreadcrumbManager::GetInstance().GetEvents();
  ASSERT_EQ(0u, events.size());

  // Add `kMaxBreadcrumbs` events to fill the event log.
  AddEvent("first event");
  for (size_t i = 0u; i < kMaxBreadcrumbs - 1; i++) {
    AddEvent("middle event");
  }
  ASSERT_EQ(kMaxBreadcrumbs, events.size());

  // Add one more event; the oldest event should be removed to keep the number
  // of events limited to `kMaxBreadcrumbs`.
  AddEvent("last event");
  EXPECT_EQ(kMaxBreadcrumbs, events.size());
  EXPECT_EQ("0:00:00 middle event", events.front());
  EXPECT_EQ("0:00:00 last event", events.back());
}

// Tests that event timestamps are formatted as expected.
TEST_F(BreadcrumbManagerTest, EventTimestampsFormatted) {
  const auto& events = BreadcrumbManager::GetInstance().GetEvents();
  AddEvent("event1");
  EXPECT_EQ("0:00:00 event1", events.back());
  task_env_.FastForwardBy(base::Seconds(100));
  AddEvent("event2");
  EXPECT_EQ("0:01:40 event2", events.back());
  task_env_.FastForwardBy(base::Hours(100));
  AddEvent("event3");
  EXPECT_EQ("100:01:40 event3", events.back());
  task_env_.FastForwardBy(base::Minutes(100));
  AddEvent("event4");
  EXPECT_EQ("101:41:40 event4", events.back());
}

}  // namespace breadcrumbs
