// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/work_item.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockWorkItem : public WorkItem {
 public:
  MOCK_METHOD0(DoImpl, bool());
  MOCK_METHOD0(RollbackImpl, void());
};

using StrictMockWorkItem = testing::StrictMock<MockWorkItem>;
using testing::Return;

}  // namespace

// Verify that Do() calls DoImpl() and returns its return value for a
// non-best-effort WorkItem.
TEST(WorkItemTest, Do) {
  {
    StrictMockWorkItem item_success;
    item_success.set_best_effort(false);
    EXPECT_CALL(item_success, DoImpl()).WillOnce(Return(true));
    EXPECT_TRUE(item_success.Do());
  }

  {
    StrictMockWorkItem item_failure;
    item_failure.set_best_effort(false);
    EXPECT_CALL(item_failure, DoImpl()).WillOnce(Return(false));
    EXPECT_FALSE(item_failure.Do());
  }
}

// Verify that Do() calls DoImpl() and returns true for a best-effort WorkItem.
TEST(WorkItemTest, DoBestEffort) {
  {
    StrictMockWorkItem item_success;
    item_success.set_best_effort(true);
    EXPECT_CALL(item_success, DoImpl()).WillOnce(Return(true));
    EXPECT_TRUE(item_success.Do());
  }

  {
    StrictMockWorkItem item_failure;
    item_failure.set_best_effort(true);
    EXPECT_CALL(item_failure, DoImpl()).WillOnce(Return(false));
    EXPECT_TRUE(item_failure.Do());
    testing::Mock::VerifyAndClearExpectations(&item_failure);
  }
}

// Verify that Rollback() calls RollbackImpl() for a WorkItem with rollback
// enabled.
TEST(WorkItemTest, Rollback) {
  StrictMockWorkItem item;
  item.set_rollback_enabled(true);
  EXPECT_TRUE(item.rollback_enabled());

  {
    testing::InSequence s;
    EXPECT_CALL(item, DoImpl()).WillOnce(Return(true));
    EXPECT_CALL(item, RollbackImpl());
  }

  item.Do();
  item.Rollback();
}

// Verify that Rollback() doesn't call RollbackImpl() for a WorkItem with
// rollback disabled.
TEST(WorkItemTest, RollbackNotAllowed) {
  StrictMockWorkItem item;
  item.set_rollback_enabled(false);

  EXPECT_CALL(item, DoImpl()).WillOnce(Return(true));

  item.Do();
  item.Rollback();
}

// Verify that the default value of the "best-effort" flag is false and that the
// setter works.
TEST(WorkItemTest, BestEffortDefaultValueAndSetter) {
  MockWorkItem work_item;
  EXPECT_FALSE(work_item.best_effort());
  work_item.set_best_effort(true);
  EXPECT_TRUE(work_item.best_effort());
}

// Verify that the default value of the "enable rollback" flag is true and that
// the setter works.
TEST(WorkItemTest, EnableRollbackDefaultValueAndSetter) {
  MockWorkItem work_item;
  EXPECT_TRUE(work_item.rollback_enabled());
  work_item.set_rollback_enabled(false);
  EXPECT_FALSE(work_item.rollback_enabled());
}
