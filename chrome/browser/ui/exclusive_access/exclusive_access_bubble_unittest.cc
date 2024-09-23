// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockExclusiveAccessBubble : public ExclusiveAccessBubble {
 public:
  explicit MockExclusiveAccessBubble(ExclusiveAccessBubbleParams params)
      : ExclusiveAccessBubble(params) {}
  ~MockExclusiveAccessBubble() override = default;
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, Show, (), (override));

  using ExclusiveAccessBubble::hide_timeout_;
  using ExclusiveAccessBubble::ShowAndStartTimers;
  using ExclusiveAccessBubble::snooze_until_;
};

class ExclusiveAccessBubbleTest : public testing::Test {
 public:
  ExclusiveAccessBubbleTest()
      : bubble_(
            {.type =
                 EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION}) {}
  ~ExclusiveAccessBubbleTest() override = default;
  MockExclusiveAccessBubble bubble_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ExclusiveAccessBubbleTest, ShowAndStartTimers) {
  EXPECT_FALSE(bubble_.hide_timeout_.IsRunning());
  EXPECT_TRUE(bubble_.snooze_until_.is_null());

  EXPECT_CALL(bubble_, Show()).Times(1);
  bubble_.ShowAndStartTimers();
  EXPECT_TRUE(bubble_.hide_timeout_.IsRunning());
  EXPECT_FALSE(bubble_.snooze_until_.is_null());
}

TEST_F(ExclusiveAccessBubbleTest, HideTimeout) {
  EXPECT_CALL(bubble_, Show()).Times(1);
  EXPECT_CALL(bubble_, Hide()).Times(1);
  bubble_.ShowAndStartTimers();
  task_environment_.FastForwardBy(base::Seconds(5));
}

TEST_F(ExclusiveAccessBubbleTest, DoesNotReshowOnUserInputAfterHide) {
  EXPECT_CALL(bubble_, Show()).Times(1);
  bubble_.ShowAndStartTimers();
  task_environment_.FastForwardBy(base::Seconds(5));
  bubble_.OnUserInput();
}

TEST_F(ExclusiveAccessBubbleTest, DoesReshowOnUserInputAfterSnooze) {
  EXPECT_CALL(bubble_, Show()).Times(2);
  bubble_.ShowAndStartTimers();
  task_environment_.FastForwardBy(base::Minutes(16));
  bubble_.OnUserInput();
}
