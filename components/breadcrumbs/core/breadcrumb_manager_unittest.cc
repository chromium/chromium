// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager.h"

#include <list>
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

// Returns the last event added to the BreadcrumbManager.
std::string GetLastEvent() {
  return BreadcrumbManager::GetInstance().GetEvents().back();
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
  const std::list<std::string>& events =
      BreadcrumbManager::GetInstance().GetEvents();
  ASSERT_EQ(1ul, events.size());
  // Events returned from |GetEvents| will have a timestamp prepended.
  EXPECT_NE(std::string::npos, events.front().find(event_message));
}

// Tests that old event buckets are dropped.
TEST_F(BreadcrumbManagerTest, OldEventsDropped) {
  // Log an event from one and two hours ago.
  AddEvent("event1");
  task_env_.FastForwardBy(base::Hours(1));
  AddEvent("event2");
  task_env_.FastForwardBy(base::Hours(1));

  // Log three events separated by three minutes to ensure they receive their
  // own event bucket. Otherwise, some old events may be returned to ensure a
  // minimum number of available events. See |MinimumEventsReturned| test below.
  AddEvent("event3");
  task_env_.FastForwardBy(base::Minutes(3));
  AddEvent("event4");
  task_env_.FastForwardBy(base::Minutes(3));
  AddEvent("event5");

  std::list<std::string> events = BreadcrumbManager::GetInstance().GetEvents();
  ASSERT_EQ(3ul, events.size());
  // Validate the three most recent events are the ones which were returned.
  EXPECT_EQ("2:00:00 event3", events.front());
  events.pop_front();
  EXPECT_EQ("2:03:00 event4", events.front());
  events.pop_front();
  EXPECT_EQ("2:06:00 event5", events.front());
}

// Tests that expired events are returned if not enough new events exist.
TEST_F(BreadcrumbManagerTest, MinimumEventsReturned) {
  // Log an event from one and two hours ago.
  AddEvent("event1");
  task_env_.FastForwardBy(base::Hours(1));
  AddEvent("event2");
  task_env_.FastForwardBy(base::Hours(1));
  AddEvent("event3");

  EXPECT_EQ(2ul, BreadcrumbManager::GetInstance().GetEvents().size());
}

// Tests that event timestamps are formatted as expected.
TEST_F(BreadcrumbManagerTest, EventTimestampsFormatted) {
  AddEvent("event1");
  EXPECT_EQ("0:00:00 event1", GetLastEvent());
  task_env_.FastForwardBy(base::Seconds(100));
  AddEvent("event2");
  EXPECT_EQ("0:01:40 event2", GetLastEvent());
  task_env_.FastForwardBy(base::Hours(100));
  AddEvent("event3");
  EXPECT_EQ("100:01:40 event3", GetLastEvent());
  task_env_.FastForwardBy(base::Minutes(100));
  AddEvent("event4");
  EXPECT_EQ("101:41:40 event4", GetLastEvent());
}

}  // namespace breadcrumbs
