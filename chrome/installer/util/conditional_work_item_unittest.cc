// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/conditional_work_item.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;

// Execute a ConditionalWorkItem whose condition is met and then rollback.
TEST(ConditionalWorkItemTest, ConditionTrueSuccess) {
  std::unique_ptr<StrictMockCondition> condition(new StrictMockCondition);
  EXPECT_CALL(*condition, ShouldRun()).WillOnce(Return(true));

  // The `if` item.
  auto if_item = std::make_unique<StrictMockWorkItem>();

  {
    // Expect it to be run and then rolled back, in that order.
    InSequence s;

    EXPECT_CALL(*if_item, DoImpl()).WillOnce(Return(true));
    EXPECT_CALL(*if_item, RollbackImpl());
  }

  // The `else` item will not be run.
  auto else_item = std::make_unique<StrictMockWorkItem>();

  std::unique_ptr<WorkItem> item(WorkItem::CreateConditionalWorkItem(
      std::move(condition), std::move(if_item), std::move(else_item)));

  // Do and rollback the item.
  EXPECT_TRUE(item->Do());
  item->Rollback();
}

// Execute a ConditionalWorkItem whose condition is met and then rollback.
TEST(ConditionalWorkItemTest, ConditionTrueSuccessNoElse) {
  std::unique_ptr<StrictMockCondition> condition(new StrictMockCondition);
  EXPECT_CALL(*condition, ShouldRun()).WillOnce(Return(true));

  // The `if` item.
  auto if_item = std::make_unique<StrictMockWorkItem>();

  {
    // Expect it to be run and then rolled back, in that order.
    InSequence s;

    EXPECT_CALL(*if_item, DoImpl()).WillOnce(Return(true));
    EXPECT_CALL(*if_item, RollbackImpl());
  }

  // No `else_item`.
  std::unique_ptr<WorkItem> item(WorkItem::CreateConditionalWorkItem(
      std::move(condition), std::move(if_item), nullptr));

  // Do and rollback the item.
  EXPECT_TRUE(item->Do());
  item->Rollback();
}

// Execute a ConditionalWorkItem whose condition is true and the `if_item`
// fails.  Rollback what has been done.
TEST(ConditionalWorkItemTest, ConditionTrueFailAndRollback) {
  std::unique_ptr<StrictMockCondition> condition(new StrictMockCondition);
  EXPECT_CALL(*condition, ShouldRun()).WillOnce(Return(true));

  // The `if` item.
  auto if_item = std::make_unique<StrictMockWorkItem>();

  {
    // Expect it to be run and then rolled back, in that order.
    InSequence s;

    EXPECT_CALL(*if_item, DoImpl()).WillOnce(Return(false));
    EXPECT_CALL(*if_item, RollbackImpl());
  }

  // The `else` item will not be run.
  auto else_item = std::make_unique<StrictMockWorkItem>();

  std::unique_ptr<WorkItem> item(WorkItem::CreateConditionalWorkItem(
      std::move(condition), std::move(if_item), std::move(else_item)));

  // Do and rollback the item.
  EXPECT_FALSE(item->Do());
  item->Rollback();
}

// Execute a ConditionalWorkItem whose condition is not met and then rollback.
TEST(ConditionalWorkItemTest, ConditionFalseSuccess) {
  std::unique_ptr<StrictMockCondition> condition(new StrictMockCondition);
  EXPECT_CALL(*condition, ShouldRun()).WillOnce(Return(false));

  // The `if` item will not be run.
  auto if_item = std::make_unique<StrictMockWorkItem>();

  // The `else` item.
  auto else_item = std::make_unique<StrictMockWorkItem>();
  {
    // Expect it to be run and then rolled back, in that order.
    InSequence s;

    EXPECT_CALL(*else_item, DoImpl()).WillOnce(Return(true));
    EXPECT_CALL(*else_item, RollbackImpl());
  }

  std::unique_ptr<WorkItem> item(WorkItem::CreateConditionalWorkItem(
      std::move(condition), std::move(if_item), std::move(else_item)));

  // Do and rollback the item.
  EXPECT_TRUE(item->Do());
  item->Rollback();
}

// Execute a ConditionalWorkItem with no `else` item whose condition is not
// metand then rollback.
TEST(ConditionalWorkItemTest, ConditionFalseNoElseSuccess) {
  std::unique_ptr<StrictMockCondition> condition(new StrictMockCondition);
  EXPECT_CALL(*condition, ShouldRun()).WillOnce(Return(false));

  // The `if` item will not be run.
  auto if_item = std::make_unique<StrictMockWorkItem>();

  std::unique_ptr<WorkItem> item(WorkItem::CreateConditionalWorkItem(
      std::move(condition), std::move(if_item), nullptr));

  // Do and rollback the item.
  EXPECT_TRUE(item->Do());
  item->Rollback();
}

TEST(ConditionalWorkItemTest, ConditionFileExists) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EXPECT_TRUE(ConditionFileExists(temp_dir.GetPath()).ShouldRun());
  EXPECT_FALSE(ConditionFileExists(
                   temp_dir.GetPath().Append(FILE_PATH_LITERAL("DoesNotExist")))
                   .ShouldRun());
}
