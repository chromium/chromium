// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/test/event_util.h"

#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {
namespace test {

void SetEventCountForDay(Event* event, uint32_t day, uint32_t count) {
  Event_Count* event_count = event->add_events();
  event_count->set_day(day);
  event_count->set_count(count);
}

void SetSnoozeCountForDay(Event* event, uint32_t day, uint32_t count) {
  Event_Count* event_count = event->add_events();
  event_count->set_day(day);
  event_count->set_snooze_count(count);
}

void VerifyEventCount(const Event* event, uint32_t day, uint32_t count) {
  bool found_day = false;
  for (const auto& event_count : event->events()) {
    if (event_count.day() == day) {
      EXPECT_FALSE(found_day);
      found_day = true;
      EXPECT_EQ(count, event_count.count());
    }
  }
  EXPECT_TRUE(found_day);
}

void VerifyEventsEqual(const Event* a, const Event* b) {
  if (!a || !b) {
    // If one of the events are nullptr, both should be nullptr.
    ASSERT_EQ(a, b);
    return;
  }

  EXPECT_EQ(a->name(), b->name());
  EXPECT_EQ(a->events_size(), b->events_size());
  for (int i = 0; i < a->events_size(); ++i) {
    VerifyEventCount(b, a->events(i).day(), a->events(i).count());
  }
}

}  // namespace test
}  // namespace feature_engagement
