// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/move_tree_work_item.h"

#include <windows.h>

#include <fstream>
#include <memory>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MoveTreeWorkItemTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_from_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_to_dir_.CreateUniqueTempDir());
  }

  base::ScopedTempDir temp_from_dir_;
  base::ScopedTempDir temp_to_dir_;
};

// Simple function to dump some text into a new file.
void CreateTextFile(const std::wstring& filename,
                    const std::wstring& contents) {
  std::wofstream file;
  file.open(base::WideToASCII(filename).c_str());
  ASSERT_TRUE(file.is_open());
  file << contents;
  file.close();
}

// Simple function to read text from a file.
std::wstring ReadTextFile(const base::FilePath& path) {
  WCHAR contents[64];
  std::wifstream file;
  file.open(base::WideToASCII(path.value()).c_str());
  EXPECT_TRUE(file.is_open());
  file.getline(contents, std::size(contents));
  file.close();
  return std::wstring(contents);
}

const wchar_t kTextContent1[] = L"Gooooooooooooooooooooogle";
const wchar_t kTextContent2[] = L"Overwrite Me";
}  // namespace

// Move one directory from source to destination when destination does not
// exist.
TEST_F(MoveTreeWorkItemTest, MoveDirectory) {
  // Create two level deep source dir
  base::FilePath from_dir1(temp_from_dir_.GetPath());
  from_dir1 = from_dir1.AppendASCII("From_Dir1");
  base::CreateDirectory(from_dir1);
  ASSERT_TRUE(base::PathExists(from_dir1));

  base::FilePath from_dir2(from_dir1);
  from_dir2 = from_dir2.AppendASCII("From_Dir2");
  base::CreateDirectory(from_dir2);
  ASSERT_TRUE(base::PathExists(from_dir2));

  base::FilePath from_file(from_dir2);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), kTextContent1);
  ASSERT_TRUE(base::PathExists(from_file));

  // Generate destination path
  base::FilePath to_dir(temp_from_dir_.GetPath());
  to_dir = to_dir.AppendASCII("To_Dir");
  ASSERT_FALSE(base::PathExists(to_dir));

  base::FilePath to_file(to_dir);
  to_file = to_file.AppendASCII("From_Dir2");
  to_file = to_file.AppendASCII("From_File");
  ASSERT_FALSE(base::PathExists(to_file));

  // test Do()
  std::unique_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_dir1, to_dir, temp_to_dir_.GetPath(), WorkItem::ALWAYS_MOVE));
  EXPECT_TRUE(work_item->Do());

  EXPECT_FALSE(base::PathExists(from_dir1));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::PathExists(to_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(from_dir1));
  EXPECT_TRUE(base::PathExists(from_file));
  EXPECT_FALSE(base::PathExists(to_dir));
}

// Move one directory from source to destination when destination already
// exists.
TEST_F(MoveTreeWorkItemTest, MoveDirectoryDestExists) {
  // Create two level deep source dir
  base::FilePath from_dir1(temp_from_dir_.GetPath());
  from_dir1 = from_dir1.AppendASCII("From_Dir1");
  base::CreateDirectory(from_dir1);
  ASSERT_TRUE(base::PathExists(from_dir1));

  base::FilePath from_dir2(from_dir1);
  from_dir2 = from_dir2.AppendASCII("From_Dir2");
  base::CreateDirectory(from_dir2);
  ASSERT_TRUE(base::PathExists(from_dir2));

  base::FilePath from_file(from_dir2);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), kTextContent1);
  ASSERT_TRUE(base::PathExists(from_file));

  // Create destination path
  base::FilePath to_dir(temp_from_dir_.GetPath());
  to_dir = to_dir.AppendASCII("To_Dir");
  base::CreateDirectory(to_dir);
  ASSERT_TRUE(base::PathExists(to_dir));

  base::FilePath orig_to_file(to_dir);
  orig_to_file = orig_to_file.AppendASCII("To_File");
  CreateTextFile(orig_to_file.value(), kTextContent2);
  ASSERT_TRUE(base::PathExists(orig_to_file));

  base::FilePath new_to_file(to_dir);
  new_to_file = new_to_file.AppendASCII("From_Dir2");
  new_to_file = new_to_file.AppendASCII("From_File");
  ASSERT_FALSE(base::PathExists(new_to_file));

  // test Do(), don't check for duplicates.
  std::unique_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_dir1, to_dir, temp_to_dir_.GetPath(), WorkItem::ALWAYS_MOVE));
  EXPECT_TRUE(work_item->Do());

  EXPECT_FALSE(base::PathExists(from_dir1));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::PathExists(new_to_file));
  EXPECT_FALSE(base::PathExists(orig_to_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(from_dir1));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_FALSE(base::PathExists(new_to_file));
  EXPECT_TRUE(base::PathExists(orig_to_file));
  EXPECT_EQ(0, ReadTextFile(orig_to_file).compare(kTextContent2));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(kTextContent1));
}

// Move one file from source to destination when destination does not
// exist.
TEST_F(MoveTreeWorkItemTest, MoveAFile) {
  // Create a file inside source dir
  base::FilePath from_dir(temp_from_dir_.GetPath());
  from_dir = from_dir.AppendASCII("From_Dir");
  base::CreateDirectory(from_dir);
  ASSERT_TRUE(base::PathExists(from_dir));

  base::FilePath from_file(from_dir);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), kTextContent1);
  ASSERT_TRUE(base::PathExists(from_file));

  // Generate destination file name
  base::FilePath to_file(temp_from_dir_.GetPath());
  to_file = to_file.AppendASCII("To_File");
  ASSERT_FALSE(base::PathExists(to_file));

  // test Do()
  std::unique_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_file, to_file, temp_to_dir_.GetPath(), WorkItem::ALWAYS_MOVE));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(from_dir));
  EXPECT_FALSE(base::PathExists(from_file));
  EXPECT_TRUE(base::PathExists(to_file));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(kTextContent1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(from_dir));
  EXPECT_TRUE(base::PathExists(from_file));
  EXPECT_FALSE(base::PathExists(to_file));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(kTextContent1));
}

// Move one file from source to destination when destination already
// exists.
TEST_F(MoveTreeWorkItemTest, MoveFileDestExists) {
  // Create a file inside source dir
  base::FilePath from_dir(temp_from_dir_.GetPath());
  from_dir = from_dir.AppendASCII("From_Dir");
  base::CreateDirectory(from_dir);
  ASSERT_TRUE(base::PathExists(from_dir));

  base::FilePath from_file(from_dir);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), kTextContent1);
  ASSERT_TRUE(base::PathExists(from_file));

  // Create destination path
  base::FilePath to_dir(temp_from_dir_.GetPath());
  to_dir = to_dir.AppendASCII("To_Dir");
  base::CreateDirectory(to_dir);
  ASSERT_TRUE(base::PathExists(to_dir));

  base::FilePath to_file(to_dir);
  to_file = to_file.AppendASCII("To_File");
  CreateTextFile(to_file.value(), kTextContent2);
  ASSERT_TRUE(base::PathExists(to_file));

  // test Do()
  std::unique_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_file, to_dir, temp_to_dir_.GetPath(), WorkItem::ALWAYS_MOVE));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(from_dir));
  EXPECT_FALSE(base::PathExists(from_file));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_FALSE(base::PathExists(to_file));
  EXPECT_EQ(0, ReadTextFile(to_dir).compare(kTextContent1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(from_dir));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(kTextContent1));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(kTextContent2));
}

// Move one file from source to destination when destination already
// exists and is in use.
TEST_F(MoveTreeWorkItemTest, MoveFileDestInUse) {
  // Create a file inside source dir
  base::FilePath from_dir(temp_from_dir_.GetPath());
  from_dir = from_dir.AppendASCII("From_Dir");
  base::CreateDirectory(from_dir);
  ASSERT_TRUE(base::PathExists(from_dir));

  base::FilePath from_file(from_dir);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), kTextContent1);
  ASSERT_TRUE(base::PathExists(from_file));

  // Create an executable in destination path by copying ourself to it.
  base::FilePath to_dir(temp_from_dir_.GetPath());
  to_dir = to_dir.AppendASCII("To_Dir");
  base::CreateDirectory(to_dir);
  ASSERT_TRUE(base::PathExists(to_dir));

  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(nullptr, exe_full_path_str, MAX_PATH);
  base::FilePath exe_full_path(exe_full_path_str);
  base::FilePath to_file(to_dir);
  to_file = to_file.AppendASCII("To_File");
  base::CopyFile(exe_full_path, to_file);
  ASSERT_TRUE(base::PathExists(to_file));

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(::CreateProcess(
      nullptr, const_cast<wchar_t*>(to_file.value().c_str()), nullptr, nullptr,
      FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi));

  // test Do()
  std::unique_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_file, to_file, temp_to_dir_.GetPath(), WorkItem::ALWAYS_MOVE));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(from_dir));
  EXPECT_FALSE(base::PathExists(from_file));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(kTextContent1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(from_dir));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(kTextContent1));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, to_file));

  TerminateProcess(pi.hProcess, 0);
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Move one file that is in use to destination.
TEST_F(MoveTreeWorkItemTest, MoveFileInUse) {
  // Create an executable for source by copying ourself to a new source dir.
  base::FilePath from_dir(temp_from_dir_.GetPath());
  from_dir = from_dir.AppendASCII("From_Dir");
  base::CreateDirectory(from_dir);
  ASSERT_TRUE(base::PathExists(from_dir));

  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(nullptr, exe_full_path_str, MAX_PATH);
  base::FilePath exe_full_path(exe_full_path_str);
  base::FilePath from_file(from_dir);
  from_file = from_file.AppendASCII("From_File");
  base::CopyFile(exe_full_path, from_file);
  ASSERT_TRUE(base::PathExists(from_file));

  // Create a destination source dir and generate destination file name.
  base::FilePath to_dir(temp_from_dir_.GetPath());
  to_dir = to_dir.AppendASCII("To_Dir");
  base::CreateDirectory(to_dir);
  ASSERT_TRUE(base::PathExists(to_dir));

  base::FilePath to_file(to_dir);
  to_file = to_file.AppendASCII("To_File");
  CreateTextFile(to_file.value(), kTextContent1);
  ASSERT_TRUE(base::PathExists(to_file));

  // Run the executable in source path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(::CreateProcess(
      nullptr, const_cast<wchar_t*>(from_file.value().c_str()), nullptr,
      nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
      &si, &pi));

  // test Do()
  std::unique_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_file, to_file, temp_to_dir_.GetPath(), WorkItem::ALWAYS_MOVE));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(from_dir));
  EXPECT_FALSE(base::PathExists(from_file));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, to_file));

  // Close the process and make sure all the conditions after Do() are
  // still true.
  TerminateProcess(pi.hProcess, 0);
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  EXPECT_TRUE(base::PathExists(from_dir));
  EXPECT_FALSE(base::PathExists(from_file));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, to_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(from_dir));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, from_file));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(kTextContent1));
}

// Move one directory from source to destination when destination already
// exists.
TEST_F(MoveTreeWorkItemTest, MoveDirectoryDestExistsCheckForDuplicatesFull) {
  // Create two level deep source dir
  base::FilePath from_dir1(temp_from_dir_.GetPath());
  from_dir1 = from_dir1.AppendASCII("From_Dir1");
  base::CreateDirectory(from_dir1);
  ASSERT_TRUE(base::PathExists(from_dir1));

  base::FilePath from_dir2(from_dir1);
  from_dir2 = from_dir2.AppendASCII("From_Dir2");
  base::CreateDirectory(from_dir2);
  ASSERT_TRUE(base::PathExists(from_dir2));

  base::FilePath from_file(from_dir2);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), kTextContent1);
  ASSERT_TRUE(base::PathExists(from_file));

  // Create a file hierarchy identical to the one in the source directory.
  base::FilePath to_dir(temp_from_dir_.GetPath());
  to_dir = to_dir.AppendASCII("To_Dir");
  ASSERT_TRUE(base::CopyDirectory(from_dir1, to_dir, true));

  // Lock one of the files in the to destination directory to prevent moves.
  base::FilePath orig_to_file(
      to_dir.AppendASCII("From_Dir2").AppendASCII("From_File"));
  base::MemoryMappedFile mapped_file;
  EXPECT_TRUE(mapped_file.Initialize(orig_to_file));

  // First check that we can't do the regular Move().
  std::unique_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_dir1, to_dir, temp_to_dir_.GetPath(), WorkItem::ALWAYS_MOVE));
  EXPECT_FALSE(work_item->Do());
  work_item->Rollback();

  // Now test Do() with the check for duplicates. This should pass.
  work_item.reset(WorkItem::CreateMoveTreeWorkItem(
      from_dir1, to_dir, temp_to_dir_.GetPath(), WorkItem::CHECK_DUPLICATES));
  EXPECT_TRUE(work_item->Do());

  // Make sure that we "moved" the files, i.e. that the source directory isn't
  // there anymore,
  EXPECT_FALSE(base::PathExists(from_dir1));
  // Make sure that the original directory structure and file are still present.
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::PathExists(orig_to_file));
  // Make sure that the backup path is not empty.
  EXPECT_FALSE(base::IsDirectoryEmpty(temp_to_dir_.GetPath()));

  // Check that the work item believes the source to have been moved.
  EXPECT_TRUE(work_item->source_moved_to_backup_);
  EXPECT_FALSE(work_item->moved_to_dest_path_);
  EXPECT_FALSE(work_item->moved_to_backup_);

  // test rollback()
  work_item->Rollback();

  // Once we rollback all the original files should still be there, as should
  // the source files.
  EXPECT_TRUE(base::PathExists(from_dir1));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::PathExists(orig_to_file));
  EXPECT_EQ(0, ReadTextFile(orig_to_file).compare(kTextContent1));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(kTextContent1));
}

// Move one directory from source to destination when destination already
// exists but contains only a subset of the files in source.
TEST_F(MoveTreeWorkItemTest, MoveDirectoryDestExistsCheckForDuplicatesPartial) {
  // Create two level deep source dir
  base::FilePath from_dir1(temp_from_dir_.GetPath());
  from_dir1 = from_dir1.AppendASCII("From_Dir1");
  base::CreateDirectory(from_dir1);
  ASSERT_TRUE(base::PathExists(from_dir1));

  base::FilePath from_dir2(from_dir1);
  from_dir2 = from_dir2.AppendASCII("From_Dir2");
  base::CreateDirectory(from_dir2);
  ASSERT_TRUE(base::PathExists(from_dir2));

  base::FilePath from_file(from_dir2);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), kTextContent1);
  ASSERT_TRUE(base::PathExists(from_file));

  base::FilePath from_file2(from_dir2);
  from_file2 = from_file2.AppendASCII("From_File2");
  CreateTextFile(from_file2.value(), kTextContent2);
  ASSERT_TRUE(base::PathExists(from_file2));

  // Create destination path
  base::FilePath to_dir(temp_from_dir_.GetPath());
  to_dir = to_dir.AppendASCII("To_Dir");
  base::CreateDirectory(to_dir);
  ASSERT_TRUE(base::PathExists(to_dir));

  // Create a sub-directory of the same name as in the source directory.
  base::FilePath to_dir2(to_dir);
  to_dir2 = to_dir2.AppendASCII("From_Dir2");
  base::CreateDirectory(to_dir2);
  ASSERT_TRUE(base::PathExists(to_dir2));

  // Create one of the files in the to sub-directory, but not the other.
  base::FilePath orig_to_file(to_dir2);
  orig_to_file = orig_to_file.AppendASCII("From_File");
  CreateTextFile(orig_to_file.value(), kTextContent1);
  ASSERT_TRUE(base::PathExists(orig_to_file));

  // test Do(), check for duplicates.
  std::unique_ptr<MoveTreeWorkItem> work_item(WorkItem::CreateMoveTreeWorkItem(
      from_dir1, to_dir, temp_to_dir_.GetPath(), WorkItem::CHECK_DUPLICATES));
  EXPECT_TRUE(work_item->Do());

  // Make sure that we "moved" the files, i.e. that the source directory isn't
  // there anymore,
  EXPECT_FALSE(base::PathExists(from_dir1));
  // Make sure that the original directory structure and file are still present.
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::PathExists(orig_to_file));
  // Make sure that the backup path is not empty.
  EXPECT_FALSE(base::IsDirectoryEmpty(temp_to_dir_.GetPath()));
  // Make sure that the "new" file is also present.
  base::FilePath new_to_file2(to_dir2);
  new_to_file2 = new_to_file2.AppendASCII("From_File2");
  EXPECT_TRUE(base::PathExists(new_to_file2));

  // Check that the work item believes that this was a regular move.
  EXPECT_FALSE(work_item->source_moved_to_backup_);
  EXPECT_TRUE(work_item->moved_to_dest_path_);
  EXPECT_TRUE(work_item->moved_to_backup_);

  // test rollback()
  work_item->Rollback();

  // Once we rollback all the original files should still be there, as should
  // the source files.
  EXPECT_TRUE(base::PathExists(from_dir1));
  EXPECT_TRUE(base::PathExists(to_dir));
  EXPECT_TRUE(base::PathExists(orig_to_file));
  EXPECT_EQ(0, ReadTextFile(orig_to_file).compare(kTextContent1));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(kTextContent1));

  // Also, after rollback the new "to" file should be gone.
  EXPECT_FALSE(base::PathExists(new_to_file2));
}
