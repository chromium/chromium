// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/event_logger.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

TEST(EventLoggerTest, BasicLogging) {
  EventLogger logger;
  logger.SetHistorySize(3);  // At most 3 events are kept.
  EXPECT_EQ(0U, logger.GetHistory().size());

  logger.Log(logging::LOGGING_INFO, "first");
  logger.Log(logging::LOGGING_INFO, "%dnd", 2);
  logger.Log(logging::LOGGING_INFO, "third");

  // Events are recorded in the chronological order with sequential IDs.
  std::vector<EventLogger::Event> history = logger.GetHistory();
  ASSERT_EQ(3U, history.size());
  EXPECT_EQ(0, history[0].id);
  EXPECT_EQ("first", history[0].what);
  EXPECT_EQ(1, history[1].id);
  EXPECT_EQ("2nd", history[1].what);
  EXPECT_EQ(2, history[2].id);
  EXPECT_EQ("third", history[2].what);

  logger.Log(logging::LOGGING_INFO, "fourth");
  // It does not log events beyond the specified.
  history = logger.GetHistory();
  ASSERT_EQ(3U, history.size());
  // The oldest events is pushed out.
  EXPECT_EQ(1, history[0].id);
  EXPECT_EQ("2nd", history[0].what);
  EXPECT_EQ(2, history[1].id);
  EXPECT_EQ("third", history[1].what);
  EXPECT_EQ(3, history[2].id);
  EXPECT_EQ("fourth", history[2].what);
}

}   // namespace drive
