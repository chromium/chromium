// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/conditional_work_item_list.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;
using testing::Return;

// Execute a ConditionalWorkItemList whose condition is met and then rollback.
TEST(ConditionalWorkItemListTest, ExecutionSuccess) {
  std::unique_ptr<StrictMockCondition> condition(new StrictMockCondition);
  EXPECT_CALL(*condition, ShouldRun()).WillOnce(Return(true));
  std::unique_ptr<WorkItemList> list(
      WorkItem::CreateConditionalWorkItemList(condition.release()));

  // Create the mock work items.
  std::unique_ptr<StrictMockWorkItem> item1(new StrictMockWorkItem);
  std::unique_ptr<StrictMockWorkItem> item2(new StrictMockWorkItem);
  std::unique_ptr<StrictMockWorkItem> item3(new StrictMockWorkItem);

  {
    // Expect all three items to be done in order then undone.
    InSequence s;

    EXPECT_CALL(*item1, DoImpl()).WillOnce(Return(true));
    EXPECT_CALL(*item2, DoImpl()).WillOnce(Return(true));
    EXPECT_CALL(*item3, DoImpl()).WillOnce(Return(true));
    EXPECT_CALL(*item3, RollbackImpl());
    EXPECT_CALL(*item2, RollbackImpl());
    EXPECT_CALL(*item1, RollbackImpl());
  }

  // Add the items to the list.
  list->AddWorkItem(item1.release());
  list->AddWorkItem(item2.release());
  list->AddWorkItem(item3.release());

  // Do and rollback the list.
  EXPECT_TRUE(list->Do());
  list->Rollback();
}

// Execute a ConditionalWorkItemList whose condition is met. Fail in the middle.
// Rollback what has been done.
TEST(ConditionalWorkItemListTest, ExecutionFailAndRollback) {
  std::unique_ptr<StrictMockCondition> condition(new StrictMockCondition);
  EXPECT_CALL(*condition, ShouldRun()).WillOnce(Return(true));
  std::unique_ptr<WorkItemList> list(
      WorkItem::CreateConditionalWorkItemList(condition.release()));

  // Create the mock work items.
  std::unique_ptr<StrictMockWorkItem> item1(new StrictMockWorkItem);
  std::unique_ptr<StrictMockWorkItem> item2(new StrictMockWorkItem);
  std::unique_ptr<StrictMockWorkItem> item3(new StrictMockWorkItem);

  {
    // Expect the two first work items to be done in order then undone.
    InSequence s;

    EXPECT_CALL(*item1, DoImpl()).WillOnce(Return(true));
    EXPECT_CALL(*item2, DoImpl()).WillOnce(Return(false));
    EXPECT_CALL(*item2, RollbackImpl());
    EXPECT_CALL(*item1, RollbackImpl());
  }

  // Add the items to the list.
  list->AddWorkItem(item1.release());
  list->AddWorkItem(item2.release());
  list->AddWorkItem(item3.release());

  // Do and rollback the list.
  EXPECT_FALSE(list->Do());
  list->Rollback();
}

// Execute a ConditionalWorkItemList whose condition isn't met.
TEST(ConditionalWorkItemListTest, ConditionFailure) {
  std::unique_ptr<StrictMockCondition> condition(new StrictMockCondition);
  EXPECT_CALL(*condition, ShouldRun()).WillOnce(Return(false));
  std::unique_ptr<WorkItemList> list(
      WorkItem::CreateConditionalWorkItemList(condition.release()));

  // Create the mock work items.
  std::unique_ptr<StrictMockWorkItem> item1(new StrictMockWorkItem);
  std::unique_ptr<StrictMockWorkItem> item2(new StrictMockWorkItem);
  std::unique_ptr<StrictMockWorkItem> item3(new StrictMockWorkItem);

  // Don't expect any call to the methods of the work items.

  // Add the items to the list.
  list->AddWorkItem(item1.release());
  list->AddWorkItem(item2.release());
  list->AddWorkItem(item3.release());

  // Do and rollback the list.
  EXPECT_TRUE(list->Do());
  list->Rollback();
}

TEST(ConditionalWorkItemListTest, ConditionNot) {
  std::unique_ptr<StrictMockCondition> condition_true(new StrictMockCondition);
  EXPECT_CALL(*condition_true, ShouldRun()).WillOnce(Return(true));
  EXPECT_FALSE(Not(condition_true.release()).ShouldRun());

  std::unique_ptr<StrictMockCondition> condition_false(new StrictMockCondition);
  EXPECT_CALL(*condition_false, ShouldRun()).WillOnce(Return(false));
  EXPECT_TRUE(Not(condition_false.release()).ShouldRun());
}

TEST(ConditionalWorkItemListTest, ConditionRunIfFileExists) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EXPECT_TRUE(ConditionRunIfFileExists(temp_dir.GetPath()).ShouldRun());
  EXPECT_FALSE(ConditionRunIfFileExists(
                   temp_dir.GetPath().Append(FILE_PATH_LITERAL("DoesNotExist")))
                   .ShouldRun());
}
