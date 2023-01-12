// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
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

void SetPreviousSessionEvents(const std::vector<std::string>& events) {
  BreadcrumbManager::GetInstance().SetPreviousSessionEvents(events);
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

// Tests that previous session events are inserted at the start of the event
// log.
TEST_F(BreadcrumbManagerTest, SetPreviousSessionEvents) {
  const auto& events = BreadcrumbManager::GetInstance().GetEvents();
  ASSERT_EQ(0u, events.size());

  std::vector<std::string> previous_events;
  previous_events.push_back("0:00:00 event 1");
  previous_events.push_back("0:00:00 event 2");
  SetPreviousSessionEvents(previous_events);

  // The previous session events should have been added to the event log.
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ("0:00:00 event 1", events.front());
  EXPECT_EQ("0:00:00 event 2", events.back());

  previous_events.clear();
  previous_events.push_back("0:00:00 event 3");
  SetPreviousSessionEvents(previous_events);

  // The previous session events should be at the front of the event log.
  EXPECT_EQ(3u, events.size());
  EXPECT_EQ("0:00:00 event 3", events.front());
}

// Tests that no more than `kMaxBreadcrumbs` events are stored after previous
// session events are retrieved.
TEST_F(BreadcrumbManagerTest, SetPreviousSessionEventsMaxEvents) {
  const auto& events = BreadcrumbManager::GetInstance().GetEvents();
  AddEvent("current event");
  ASSERT_EQ(1u, events.size());

  // Set the previous session events to a large list of events, such that the
  // event log will become oversized when it's inserted.
  const std::string previous_event = "0:00:00 previous event ";
  std::vector<std::string> oversized_events;
  oversized_events.reserve(kMaxBreadcrumbs);
  int previous_event_num = 1;
  for (size_t i = 0u; i < kMaxBreadcrumbs; i++) {
    oversized_events.push_back(previous_event +
                               base::NumberToString(previous_event_num));
    previous_event_num++;
  }
  ASSERT_EQ(kMaxBreadcrumbs, oversized_events.size());
  SetPreviousSessionEvents(oversized_events);

  // The oldest previous event should have been removed to keep the number of
  // events limited to `kMaxBreadcrumbs`.
  EXPECT_EQ(kMaxBreadcrumbs, events.size());
  EXPECT_EQ("0:00:00 previous event 2", events.front());
  EXPECT_EQ("0:00:00 current event", events.back());
}

}  // namespace breadcrumbs
