// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_tree_work_item.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTextContent[] = "delete me";

class DeleteTreeWorkItemTest : public testing::Test {
 public:
  DeleteTreeWorkItemTest(const DeleteTreeWorkItemTest&) = delete;
  DeleteTreeWorkItemTest& operator=(const DeleteTreeWorkItemTest&) = delete;

 protected:
  DeleteTreeWorkItemTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    dir_name_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("to_be_deleted"));
    ASSERT_TRUE(base::CreateDirectory(dir_name_));
    ASSERT_TRUE(base::PathExists(dir_name_));

    dir_name_1_ = dir_name_.Append(FILE_PATH_LITERAL("1"));
    ASSERT_TRUE(base::CreateDirectory(dir_name_1_));
    ASSERT_TRUE(base::PathExists(dir_name_1_));

    dir_name_2_ = dir_name_.Append(FILE_PATH_LITERAL("2"));
    ASSERT_TRUE(base::CreateDirectory(dir_name_2_));
    ASSERT_TRUE(base::PathExists(dir_name_2_));

    file_name_1_ = dir_name_1_.Append(FILE_PATH_LITERAL("File_1.txt"));
    ASSERT_TRUE(base::WriteFile(file_name_1_, kTextContent));
    ASSERT_TRUE(base::PathExists(file_name_1_));

    file_name_2_ = dir_name_2_.Append(FILE_PATH_LITERAL("File_2.txt"));
    ASSERT_TRUE(base::WriteFile(file_name_2_, kTextContent));
    ASSERT_TRUE(base::PathExists(file_name_2_));
  }

  void ExpectAllFilesExist() {
    EXPECT_TRUE(base::PathExists(dir_name_));
    EXPECT_TRUE(base::PathExists(dir_name_1_));
    EXPECT_TRUE(base::PathExists(dir_name_2_));
    EXPECT_TRUE(base::PathExists(file_name_1_));
    EXPECT_TRUE(base::PathExists(file_name_2_));
  }

  void ExpectAllFilesDeleted() {
    EXPECT_FALSE(base::PathExists(dir_name_));
    EXPECT_FALSE(base::PathExists(dir_name_1_));
    EXPECT_FALSE(base::PathExists(dir_name_2_));
    EXPECT_FALSE(base::PathExists(file_name_1_));
    EXPECT_FALSE(base::PathExists(file_name_2_));
  }

  // The temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;

  base::FilePath dir_name_;
  base::FilePath dir_name_1_;
  base::FilePath dir_name_2_;
  base::FilePath file_name_1_;
  base::FilePath file_name_2_;
};

}  // namespace

// Delete a tree with rollback enabled and no file in use. Do() should delete
// everything and Rollback() should bring back everything.
TEST_F(DeleteTreeWorkItemTest, Delete) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<DeleteTreeWorkItem> work_item(
      WorkItem::CreateDeleteTreeWorkItem(dir_name_, temp_dir.GetPath()));

  EXPECT_TRUE(work_item->Do());
  ExpectAllFilesDeleted();

  work_item->Rollback();
  ExpectAllFilesExist();
}

// Delete a tree with rollback disabled and no file in use. Do() should delete
// everything and Rollback() shouldn't bring back anything.
TEST_F(DeleteTreeWorkItemTest, DeleteRollbackDisabled) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<DeleteTreeWorkItem> work_item(
      WorkItem::CreateDeleteTreeWorkItem(dir_name_, temp_dir.GetPath()));
  work_item->set_rollback_enabled(false);

  EXPECT_TRUE(work_item->Do());
  ExpectAllFilesDeleted();

  work_item->Rollback();
  ExpectAllFilesDeleted();
}
