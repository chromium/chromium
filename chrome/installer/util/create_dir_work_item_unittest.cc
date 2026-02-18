// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/create_dir_work_item.h"

#include <windows.h>

#include <memory>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace {
class CreateDirWorkItemTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};
}  // namespace

TEST_F(CreateDirWorkItemTest, CreatePath) {
  base::FilePath parent_dir(temp_dir_.GetPath());
  parent_dir = parent_dir.AppendASCII("a");
  base::CreateDirectory(parent_dir);
  ASSERT_TRUE(base::PathExists(parent_dir));

  base::FilePath top_dir_to_create(parent_dir);
  top_dir_to_create = top_dir_to_create.AppendASCII("b");

  base::FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("c");
  dir_to_create = dir_to_create.AppendASCII("d");

  std::unique_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(dir_to_create));

  work_item->Rollback();

  // Rollback should delete all the paths up to top_dir_to_create.
  EXPECT_FALSE(base::PathExists(top_dir_to_create));
  EXPECT_TRUE(base::PathExists(parent_dir));
}

TEST_F(CreateDirWorkItemTest, CreateExistingPath) {
  base::FilePath dir_to_create(temp_dir_.GetPath());
  dir_to_create = dir_to_create.AppendASCII("aa");
  base::CreateDirectory(dir_to_create);
  ASSERT_TRUE(base::PathExists(dir_to_create));

  std::unique_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(dir_to_create));

  work_item->Rollback();

  // Rollback should not remove the path since it exists before
  // the CreateDirWorkItem is called.
  EXPECT_TRUE(base::PathExists(dir_to_create));
}

// Do fails if there is a file where the desired directory belongs.
TEST_F(CreateDirWorkItemTest, CreateExistingFile) {
  base::FilePath a_file = temp_dir_.GetPath().AppendASCII("aa");
  ASSERT_TRUE(base::WriteFile(a_file, base::as_byte_span(a_file.value())));

  base::FilePath dir_to_create = a_file;
  std::unique_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create));

  // The item fails because there's a file in the way.
  EXPECT_FALSE(work_item->Do());

  // The file has not been touched.
  EXPECT_THAT(base::ReadFileToBytes(a_file),
              Eq(base::as_byte_span(a_file.value())));

  work_item->Rollback();

  // The file has not been touched.
  EXPECT_THAT(base::ReadFileToBytes(a_file),
              Eq(base::as_byte_span(a_file.value())));
}

// Do fails if there is a file as a parent of the desired directory.
TEST_F(CreateDirWorkItemTest, CreateExistingFileAsParent) {
  base::FilePath a_file = temp_dir_.GetPath().AppendASCII("aa");
  ASSERT_TRUE(base::WriteFile(a_file, base::as_byte_span(a_file.value())));

  base::FilePath dir_to_create = a_file.AppendASCII("bb");

  std::unique_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create));

  // The item fails because there's a file in the way of a parent.
  EXPECT_FALSE(work_item->Do());

  // The file has not been touched.
  EXPECT_THAT(base::ReadFileToBytes(a_file),
              Eq(base::as_byte_span(a_file.value())));

  work_item->Rollback();

  // The file has not been touched.
  EXPECT_THAT(base::ReadFileToBytes(a_file),
              Eq(base::as_byte_span(a_file.value())));
}

TEST_F(CreateDirWorkItemTest, CreateSharedPath) {
  base::FilePath dir_to_create_1(temp_dir_.GetPath());
  dir_to_create_1 = dir_to_create_1.AppendASCII("aaa");

  base::FilePath dir_to_create_2(dir_to_create_1);
  dir_to_create_2 = dir_to_create_2.AppendASCII("bbb");

  base::FilePath dir_to_create_3(dir_to_create_2);
  dir_to_create_3 = dir_to_create_3.AppendASCII("ccc");

  std::unique_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create_3));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(dir_to_create_3));

  // Create another directory under dir_to_create_2
  base::FilePath dir_to_create_4(dir_to_create_2);
  dir_to_create_4 = dir_to_create_4.AppendASCII("ddd");
  base::CreateDirectory(dir_to_create_4);
  ASSERT_TRUE(base::PathExists(dir_to_create_4));

  work_item->Rollback();

  // Rollback should delete dir_to_create_3.
  EXPECT_FALSE(base::PathExists(dir_to_create_3));

  // Rollback should not delete dir_to_create_2 as it is shared.
  EXPECT_TRUE(base::PathExists(dir_to_create_2));
  EXPECT_TRUE(base::PathExists(dir_to_create_4));
}

TEST_F(CreateDirWorkItemTest, RollbackWithMissingDir) {
  base::FilePath dir_to_create_1(temp_dir_.GetPath());
  dir_to_create_1 = dir_to_create_1.AppendASCII("aaaa");

  base::FilePath dir_to_create_2(dir_to_create_1);
  dir_to_create_2 = dir_to_create_2.AppendASCII("bbbb");

  base::FilePath dir_to_create_3(dir_to_create_2);
  dir_to_create_3 = dir_to_create_3.AppendASCII("cccc");

  std::unique_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create_3));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(dir_to_create_3));

  RemoveDirectory(dir_to_create_3.value().c_str());
  ASSERT_FALSE(base::PathExists(dir_to_create_3));

  work_item->Rollback();

  // dir_to_create_3 has already been deleted, Rollback should delete
  // the rest.
  EXPECT_FALSE(base::PathExists(dir_to_create_1));
}
