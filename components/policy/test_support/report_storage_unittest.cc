// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/report_storage.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(ReportStorageTest, FlattensRealtimeEventsAcrossRequests) {
  ReportStorage storage;

  ::chrome::cros::reporting::proto::UploadEventsRequest first;
  first.add_events()->set_event_id("event-1");
  first.add_events()->set_event_id("event-2");
  storage.AddRealtimeEvents(first);

  ::chrome::cros::reporting::proto::UploadEventsRequest second;
  second.add_events()->set_event_id("event-3");
  storage.AddRealtimeEvents(second);

  EXPECT_EQ(storage.GetRealtimeReports().size(), 2u);
  auto events = storage.GetRealtimeEvents();
  ASSERT_EQ(events.size(), 3u);
  EXPECT_EQ(events[0].event_id(), "event-1");
  EXPECT_EQ(events[1].event_id(), "event-2");
  EXPECT_EQ(events[2].event_id(), "event-3");
}

}  // namespace policy
