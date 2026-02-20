// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/conditional_work_item.h"

#include <memory>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/win/scoped_process_information.h"
#include "chrome/installer/util/move_tree_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Optional;
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

// Tests that a non-existent file is not "in use".
TEST(ConditionFileInUseTest, FileDoesNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ASSERT_FALSE(
      ConditionFileInUse(temp_dir.GetPath().Append(FILE_PATH_LITERAL("nofile")))
          .ShouldRun());
}

// Tests that an ordinary file is not "in use".
TEST(ConditionFileInUseTest, FileExistsNotInUse) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path(temp_dir.GetPath().Append(FILE_PATH_LITERAL("afile")));
  ASSERT_TRUE(base::WriteFile(path, "hi mom"));
  ASSERT_FALSE(ConditionFileInUse(path).ShouldRun());
}

// Tests that the currently-running executable is considered "in use".
TEST(ConditionFileInUseTest, FileExistsInUse) {
  ASSERT_TRUE(ConditionFileInUse(base::PathService::CheckedGet(base::FILE_EXE))
                  .ShouldRun());
}

// An integration test that uses the FileInUse condition to move a file to one
// place or another.
// 1. If destination file is in use, the source should be moved with the new
//    name after Do() and this new name file should be deleted after rollback.
// 2. If destination file is not in use, the source should be moved in the
//    destination folder after Do() and should be rolled back after Rollback().
TEST(ConditionFileInUseTest, IntegrationTest) {
  base::ScopedTempDir test_dir;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::vector<uint8_t> text_contents(std::from_range,
                                     base::byte_span_from_cstring("hi mom"));

  // Create source file
  base::FilePath file_name_from(test_dir.GetPath());
  file_name_from = file_name_from.AppendASCII("File_From");
  ASSERT_TRUE(base::WriteFile(file_name_from, base::span(text_contents)));

  // Create an executable in destination path by copying ourself to it.
  base::FilePath exe_full_path = base::PathService::CheckedGet(base::FILE_EXE);

  base::FilePath dir_name_to(test_dir.GetPath());
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  ASSERT_TRUE(base::CreateDirectory(dir_name_to));

  base::FilePath file_name_to = dir_name_to.AppendASCII("File_To");
  base::FilePath alternate_to = dir_name_to.AppendASCII("Alternate_To");
  base::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(base::PathExists(file_name_to));
  ASSERT_FALSE(ConditionFileInUse(file_name_to).ShouldRun());

  // Run the executable in destination path
  STARTUPINFOW si = {.cb = sizeof(si)};
  PROCESS_INFORMATION pi = {};
  ASSERT_TRUE(::CreateProcess(
      nullptr, const_cast<wchar_t*>(file_name_to.value().c_str()), nullptr,
      nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
      &si, &pi));

  base::win::ScopedProcessInformation process_info(pi);
  base::Process process(process_info.TakeProcessHandle());

  std::unique_ptr<WorkItem> in_use_item(WorkItem::CreateMoveTreeWorkItem(
      file_name_from, alternate_to, temp_dir.GetPath(),
      WorkItem::MoveTreeOptions()));

  std::unique_ptr<WorkItem> not_in_use_item(WorkItem::CreateMoveTreeWorkItem(
      file_name_from, file_name_to, temp_dir.GetPath(),
      WorkItem::MoveTreeOptions()));

  std::unique_ptr<WorkItem> work_item(WorkItem::CreateConditionalWorkItem(
      std::make_unique<ConditionFileInUse>(file_name_to),
      std::move(in_use_item), std::move(not_in_use_item)));

  // test Do().
  EXPECT_TRUE(work_item->Do());

  EXPECT_FALSE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_THAT(base::ReadFileToBytes(alternate_to), Optional(Eq(text_contents)));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, file_name_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_THAT(base::ReadFileToBytes(file_name_from),
              Optional(Eq(text_contents)));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, file_name_to));
  // the alternate file should be gone after rollback
  EXPECT_FALSE(base::PathExists(alternate_to));

  process.Terminate(/*exit_code=*/0, /*wait=*/true);
  process.Close();
  process_info.Close();

  // Now the process has terminated, lets try overwriting the file again
  in_use_item.reset(WorkItem::CreateMoveTreeWorkItem(
      file_name_from, alternate_to, temp_dir.GetPath(),
      WorkItem::MoveTreeOptions()));

  not_in_use_item.reset(WorkItem::CreateMoveTreeWorkItem(
      file_name_from, file_name_to, temp_dir.GetPath(),
      WorkItem::MoveTreeOptions()));

  work_item.reset(WorkItem::CreateConditionalWorkItem(
      std::make_unique<ConditionFileInUse>(file_name_to),
      std::move(in_use_item), std::move(not_in_use_item)));

  if (ConditionFileInUse(file_name_to).ShouldRun()) {
    base::PlatformThread::Sleep(base::Seconds(2));
  }
  // If file is still in use, the rest of the test will fail.
  ASSERT_FALSE(ConditionFileInUse(file_name_to).ShouldRun());
  EXPECT_TRUE(work_item->Do());

  EXPECT_FALSE(base::PathExists(file_name_from));
  EXPECT_THAT(base::ReadFileToBytes(file_name_to), Optional(Eq(text_contents)));
  EXPECT_FALSE(base::PathExists(alternate_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_THAT(base::ReadFileToBytes(file_name_from),
              Optional(Eq(text_contents)));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_FALSE(base::PathExists(alternate_to));
}
