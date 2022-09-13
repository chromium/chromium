// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager.h"

#include <list>
#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/breadcrumbs/core/breadcrumb_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace breadcrumbs {

// Test fixture for testing BreadcrumbManager class.
class BreadcrumbManagerTest : public PlatformTest {
 protected:
  BreadcrumbManagerTest() = default;

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Set the start time to the current time at the start of each test rather
  // than breadcrumbs::GetStartTime(), to ensure that timestamps start at
  // 0:00:00. Otherwise, failures in earlier tests may progress MOCK_TIME by a
  // few seconds, throwing off timestamps and causing these tests to also fail.
  BreadcrumbManager breadcrumb_manager_{base::TimeTicks::Now()};
};

// Tests that an event is logged and returned.
TEST_F(BreadcrumbManagerTest, AddEvent) {
  const std::string event_message = "event";
  breadcrumb_manager_.AddEvent(event_message);
  const std::list<std::string>& events = breadcrumb_manager_.GetEvents();
  ASSERT_EQ(1ul, events.size());
  // Events returned from |GetEvents| will have a timestamp prepended.
  EXPECT_NE(std::string::npos, events.front().find(event_message));
}

// Tests that old event buckets are dropped.
TEST_F(BreadcrumbManagerTest, OldEventsDropped) {
  // Log an event from one and two hours ago.
  breadcrumb_manager_.AddEvent("event1");
  task_env_.FastForwardBy(base::Hours(1));
  breadcrumb_manager_.AddEvent("event2");
  task_env_.FastForwardBy(base::Hours(1));

  // Log three events separated by three minutes to ensure they receive their
  // own event bucket. Otherwise, some old events may be returned to ensure a
  // minimum number of available events. See |MinimumEventsReturned| test below.
  breadcrumb_manager_.AddEvent("event3");
  task_env_.FastForwardBy(base::Minutes(3));
  breadcrumb_manager_.AddEvent("event4");
  task_env_.FastForwardBy(base::Minutes(3));
  breadcrumb_manager_.AddEvent("event5");

  std::list<std::string> events = breadcrumb_manager_.GetEvents();
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
  breadcrumb_manager_.AddEvent("event1");
  task_env_.FastForwardBy(base::Hours(1));
  breadcrumb_manager_.AddEvent("event2");
  task_env_.FastForwardBy(base::Hours(1));
  breadcrumb_manager_.AddEvent("event3");

  EXPECT_EQ(2ul, breadcrumb_manager_.GetEvents().size());
}

// Tests that event timestamps are formatted as expected.
TEST_F(BreadcrumbManagerTest, EventTimestampsFormatted) {
  breadcrumb_manager_.AddEvent("event1");
  EXPECT_EQ("0:00:00 event1", breadcrumb_manager_.GetEvents().back());
  task_env_.FastForwardBy(base::Seconds(100));
  breadcrumb_manager_.AddEvent("event2");
  EXPECT_EQ("0:01:40 event2", breadcrumb_manager_.GetEvents().back());
  task_env_.FastForwardBy(base::Hours(100));
  breadcrumb_manager_.AddEvent("event3");
  EXPECT_EQ("100:01:40 event3", breadcrumb_manager_.GetEvents().back());
  task_env_.FastForwardBy(base::Minutes(100));
  breadcrumb_manager_.AddEvent("event4");
  EXPECT_EQ("101:41:40 event4", breadcrumb_manager_.GetEvents().back());
}

}  // namespace breadcrumbs
