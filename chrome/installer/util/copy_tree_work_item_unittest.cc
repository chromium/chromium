// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/copy_tree_work_item.h"

#include <windows.h>

#include <fstream>
#include <memory>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CopyTreeWorkItemTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { logging::CloseLogFile(); }

  // the path to temporary directory used to contain the test operations
  base::ScopedTempDir test_dir_;
  base::ScopedTempDir temp_dir_;
};

// Simple function to dump some text into a new file.
void CreateTextFile(const std::wstring& filename,
                    const std::wstring& contents) {
  std::ofstream file;
  file.open(filename.c_str());
  ASSERT_TRUE(file.is_open());
  file << contents;
  file.close();
}

// Simple function to read text from a file.
std::wstring ReadTextFile(const std::wstring& filename) {
  WCHAR contents[64];
  std::wifstream file;
  file.open(filename.c_str());
  EXPECT_TRUE(file.is_open());
  file.getline(contents, 64);
  file.close();
  return std::wstring(contents);
}

const wchar_t text_content_1[] = L"Gooooooooooooooooooooogle";
const wchar_t text_content_2[] = L"Overwrite Me";

}  // namespace

// Copy one file from source to destination.
TEST_F(CopyTreeWorkItemTest, CopyFile) {
  // Create source file
  base::FilePath file_name_from(test_dir_.GetPath());
  file_name_from = file_name_from.AppendASCII("File_From.txt");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from));

  // Create destination path
  base::FilePath dir_name_to(test_dir_.GetPath());
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  base::CreateDirectory(dir_name_to);
  ASSERT_TRUE(base::PathExists(dir_name_to));

  base::FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To.txt");

  // test Do()
  std::unique_ptr<CopyTreeWorkItem> work_item(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(), WorkItem::ALWAYS,
      base::FilePath()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_TRUE(base::ContentsEqual(file_name_from, file_name_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_FALSE(base::PathExists(file_name_to));
  EXPECT_TRUE(base::PathExists(file_name_from));
}

// Copy one file, overwriting the existing one in destination.
// Test with always_overwrite being true or false. The file is overwritten
// regardless since the content at destination file is different from source.
TEST_F(CopyTreeWorkItemTest, CopyFileOverwrite) {
  // Create source file
  base::FilePath file_name_from(test_dir_.GetPath());
  file_name_from = file_name_from.AppendASCII("File_From.txt");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from));

  // Create destination file
  base::FilePath dir_name_to(test_dir_.GetPath());
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  base::CreateDirectory(dir_name_to);
  ASSERT_TRUE(base::PathExists(dir_name_to));

  base::FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To.txt");
  CreateTextFile(file_name_to.value(), text_content_2);
  ASSERT_TRUE(base::PathExists(file_name_to));

  // test Do() with always_overwrite being true.
  std::unique_ptr<CopyTreeWorkItem> work_item(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(), WorkItem::ALWAYS,
      base::FilePath()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_2));

  // test Do() with always_overwrite being false.
  // the file is still overwritten since the content is different.
  work_item.reset(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(), WorkItem::IF_DIFFERENT,
      base::FilePath()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_2));
}

// Copy one file, with the existing one in destination having the same
// content.
// If always_overwrite being true, the file is overwritten.
// If always_overwrite being false, the file is unchanged.
TEST_F(CopyTreeWorkItemTest, CopyFileSameContent) {
  // Create source file
  base::FilePath file_name_from(test_dir_.GetPath());
  file_name_from = file_name_from.AppendASCII("File_From.txt");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from));

  // Create destination file
  base::FilePath dir_name_to(test_dir_.GetPath());
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  base::CreateDirectory(dir_name_to);
  ASSERT_TRUE(base::PathExists(dir_name_to));

  base::FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To.txt");
  CreateTextFile(file_name_to.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_to));

  // test Do() with always_overwrite being true.
  std::unique_ptr<CopyTreeWorkItem> work_item(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(), WorkItem::ALWAYS,
      base::FilePath()));

  EXPECT_TRUE(work_item->Do());

  // Get the path of backup file
  base::FilePath backup_file(work_item->backup_path_.GetPath());
  EXPECT_FALSE(backup_file.empty());
  backup_file = backup_file.AppendASCII("File_To.txt");

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  // we verify the file is overwritten by checking the existence of backup
  // file.
  EXPECT_TRUE(base::PathExists(backup_file));
  EXPECT_EQ(0, ReadTextFile(backup_file.value()).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  // the backup file should be gone after rollback
  EXPECT_FALSE(base::PathExists(backup_file));

  // test Do() with always_overwrite being false. nothing should change.
  work_item.reset(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(), WorkItem::IF_DIFFERENT,
      base::FilePath()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  // we verify the file is not overwritten by checking that the backup
  // file does not exist.
  EXPECT_FALSE(base::PathExists(backup_file));

  // test rollback(). nothing should happen here.
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  EXPECT_FALSE(base::PathExists(backup_file));
}

// Copy one file and without rollback. Verify all temporary files are deleted.
TEST_F(CopyTreeWorkItemTest, CopyFileAndCleanup) {
  // Create source file
  base::FilePath file_name_from(test_dir_.GetPath());
  file_name_from = file_name_from.AppendASCII("File_From.txt");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from));

  // Create destination file
  base::FilePath dir_name_to(test_dir_.GetPath());
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  base::CreateDirectory(dir_name_to);
  ASSERT_TRUE(base::PathExists(dir_name_to));

  base::FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To.txt");
  CreateTextFile(file_name_to.value(), text_content_2);
  ASSERT_TRUE(base::PathExists(file_name_to));

  base::FilePath backup_file;

  {
    // test Do().
    std::unique_ptr<CopyTreeWorkItem> work_item(
        WorkItem::CreateCopyTreeWorkItem(
            file_name_from, file_name_to, temp_dir_.GetPath(),
            WorkItem::IF_DIFFERENT, base::FilePath()));

    EXPECT_TRUE(work_item->Do());

    // Get the path of backup file
    backup_file = work_item->backup_path_.GetPath();
    EXPECT_FALSE(backup_file.empty());
    backup_file = backup_file.AppendASCII("File_To.txt");

    EXPECT_TRUE(base::PathExists(file_name_from));
    EXPECT_TRUE(base::PathExists(file_name_to));
    EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
    EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
    // verify the file is moved to backup place.
    EXPECT_TRUE(base::PathExists(backup_file));
    EXPECT_EQ(0, ReadTextFile(backup_file.value()).compare(text_content_2));
  }

  // verify the backup file is cleaned up as well.
  EXPECT_FALSE(base::PathExists(backup_file));
}

// Copy one file, with the existing one in destination being used with
// overwrite option as IF_DIFFERENT. This destination-file-in-use should
// be moved to backup location after Do() and moved back after Rollback().
TEST_F(CopyTreeWorkItemTest, CopyFileInUse) {
  // Create source file
  base::FilePath file_name_from(test_dir_.GetPath());
  file_name_from = file_name_from.AppendASCII("File_From");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(nullptr, exe_full_path_str, MAX_PATH);
  base::FilePath exe_full_path(exe_full_path_str);

  base::FilePath dir_name_to(test_dir_.GetPath());
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  base::CreateDirectory(dir_name_to);
  ASSERT_TRUE(base::PathExists(dir_name_to));

  base::FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To");
  base::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(base::PathExists(file_name_to));

  VLOG(1) << "copy ourself from " << exe_full_path.value() << " to "
          << file_name_to.value();

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(::CreateProcess(
      nullptr, const_cast<wchar_t*>(file_name_to.value().c_str()), nullptr,
      nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
      &si, &pi));

  // test Do().
  std::unique_ptr<CopyTreeWorkItem> work_item(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(), WorkItem::IF_DIFFERENT,
      base::FilePath()));

  EXPECT_TRUE(work_item->Do());

  // Get the path of backup file
  base::FilePath backup_file(work_item->backup_path_.GetPath());
  EXPECT_FALSE(backup_file.empty());
  backup_file = backup_file.AppendASCII("File_To");

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  // verify the file in used is moved to backup place.
  EXPECT_TRUE(base::PathExists(backup_file));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, backup_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, file_name_to));
  // the backup file should be gone after rollback
  EXPECT_FALSE(base::PathExists(backup_file));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Test overwrite option NEW_NAME_IF_IN_USE:
// 1. If destination file is in use, the source should be copied with the
//    new name after Do() and this new name file should be deleted
//    after rollback.
// 2. If destination file is not in use, the source should be copied in the
//    destination folder after Do() and should be rolled back after Rollback().
TEST_F(CopyTreeWorkItemTest, NewNameAndCopyTest) {
  // Create source file
  base::FilePath file_name_from(test_dir_.GetPath());
  file_name_from = file_name_from.AppendASCII("File_From");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(nullptr, exe_full_path_str, MAX_PATH);
  base::FilePath exe_full_path(exe_full_path_str);

  base::FilePath dir_name_to(test_dir_.GetPath());
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  base::CreateDirectory(dir_name_to);
  ASSERT_TRUE(base::PathExists(dir_name_to));

  base::FilePath file_name_to(dir_name_to), alternate_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To");
  alternate_to = alternate_to.AppendASCII("Alternate_To");
  base::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(base::PathExists(file_name_to));
  ASSERT_FALSE(CopyTreeWorkItem::IsFileInUse(file_name_to));

  VLOG(1) << "copy ourself from " << exe_full_path.value() << " to "
          << file_name_to.value();

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(::CreateProcess(
      nullptr, const_cast<wchar_t*>(file_name_to.value().c_str()), nullptr,
      nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
      &si, &pi));

  // test Do().
  std::unique_ptr<CopyTreeWorkItem> work_item(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(),
      WorkItem::NEW_NAME_IF_IN_USE, alternate_to));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, file_name_to));
  // verify that the backup path does not exist
  EXPECT_FALSE(work_item->backup_path_created_);
  EXPECT_TRUE(base::ContentsEqual(file_name_from, alternate_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_FALSE(work_item->backup_path_created_);
  // the alternate file should be gone after rollback
  EXPECT_FALSE(base::PathExists(alternate_to));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  // Now the process has terminated, lets try overwriting the file again
  work_item.reset(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(),
      WorkItem::NEW_NAME_IF_IN_USE, alternate_to));
  if (CopyTreeWorkItem::IsFileInUse(file_name_to))
    base::PlatformThread::Sleep(base::Seconds(2));
  // If file is still in use, the rest of the test will fail.
  ASSERT_FALSE(CopyTreeWorkItem::IsFileInUse(file_name_to));
  EXPECT_TRUE(work_item->Do());

  // Get the path of backup file
  base::FilePath backup_file(work_item->backup_path_.GetPath());
  EXPECT_FALSE(backup_file.empty());
  backup_file = backup_file.AppendASCII("File_To");

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(base::ContentsEqual(file_name_from, file_name_to));
  // verify that the backup path does exist
  EXPECT_TRUE(base::PathExists(backup_file));
  EXPECT_FALSE(base::PathExists(alternate_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, file_name_to));
  // the backup file should be gone after rollback
  EXPECT_FALSE(base::PathExists(backup_file));
  EXPECT_FALSE(base::PathExists(alternate_to));
}

// Test overwrite option IF_NOT_PRESENT:
// 1. If destination file/directory exist, the source should not be copied
// 2. If destination file/directory do not exist, the source should be copied
//    in the destination folder after Do() and should be rolled back after
//    Rollback().
// Flaky, http://crbug.com/59785.
TEST_F(CopyTreeWorkItemTest, DISABLED_IfNotPresentTest) {
  // Create source file
  base::FilePath file_name_from(test_dir_.GetPath());
  file_name_from = file_name_from.AppendASCII("File_From");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(nullptr, exe_full_path_str, MAX_PATH);
  base::FilePath exe_full_path(exe_full_path_str);

  base::FilePath dir_name_to(test_dir_.GetPath());
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  base::CreateDirectory(dir_name_to);
  ASSERT_TRUE(base::PathExists(dir_name_to));
  base::FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To");
  base::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(base::PathExists(file_name_to));

  // Get the path of backup file
  base::FilePath backup_file(temp_dir_.GetPath());
  backup_file = backup_file.AppendASCII("File_To");

  // test Do().
  std::unique_ptr<CopyTreeWorkItem> work_item(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(),
      WorkItem::IF_NOT_PRESENT, base::FilePath()));
  EXPECT_TRUE(work_item->Do());

  // verify that the source, destination have not changed and backup path
  // does not exist
  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_FALSE(base::PathExists(backup_file));

  // test rollback()
  work_item->Rollback();

  // verify that the source, destination have not changed and backup path
  // does not exist after rollback also
  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_FALSE(base::PathExists(backup_file));

  // Now delete the destination and try copying the file again.
  base::DeleteFile(file_name_to);
  work_item.reset(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to, temp_dir_.GetPath(),
      WorkItem::IF_NOT_PRESENT, base::FilePath()));
  EXPECT_TRUE(work_item->Do());

  // verify that the source, destination are the same and backup path
  // does not exist
  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_TRUE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  EXPECT_FALSE(base::PathExists(backup_file));

  // test rollback()
  work_item->Rollback();

  // verify that the destination does not exist anymore
  EXPECT_TRUE(base::PathExists(file_name_from));
  EXPECT_FALSE(base::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_FALSE(base::PathExists(backup_file));
}

// Copy one file without rollback. The existing one in destination is in use.
// Verify it is moved to backup location and stays there.
// Flaky, http://crbug.com/59783.
TEST_F(CopyTreeWorkItemTest, DISABLED_CopyFileInUseAndCleanup) {
  // Create source file
  base::FilePath file_name_from(test_dir_.GetPath());
  file_name_from = file_name_from.AppendASCII("File_From");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(nullptr, exe_full_path_str, MAX_PATH);
  base::FilePath exe_full_path(exe_full_path_str);

  base::FilePath dir_name_to(test_dir_.GetPath());
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  base::CreateDirectory(dir_name_to);
  ASSERT_TRUE(base::PathExists(dir_name_to));

  base::FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To");
  base::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(base::PathExists(file_name_to));

  VLOG(1) << "copy ourself from " << exe_full_path.value() << " to "
          << file_name_to.value();

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(::CreateProcess(
      nullptr, const_cast<wchar_t*>(file_name_to.value().c_str()), nullptr,
      nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
      &si, &pi));

  base::FilePath backup_file;

  // test Do().
  {
    std::unique_ptr<CopyTreeWorkItem> work_item(
        WorkItem::CreateCopyTreeWorkItem(
            file_name_from, file_name_to, temp_dir_.GetPath(),
            WorkItem::IF_DIFFERENT, base::FilePath()));

    EXPECT_TRUE(work_item->Do());

    // Get the path of backup file
    backup_file = work_item->backup_path_.GetPath();
    EXPECT_FALSE(backup_file.empty());
    backup_file = backup_file.AppendASCII("File_To");

    EXPECT_TRUE(base::PathExists(file_name_from));
    EXPECT_TRUE(base::PathExists(file_name_to));
    EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
    EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
    // verify the file in used is moved to backup place.
    EXPECT_TRUE(base::PathExists(backup_file));
    EXPECT_TRUE(base::ContentsEqual(exe_full_path, backup_file));
  }

  // verify the file in used should be still at the backup place.
  EXPECT_TRUE(base::PathExists(backup_file));
  EXPECT_TRUE(base::ContentsEqual(exe_full_path, backup_file));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Copy a tree from source to destination.
// Flaky, http://crbug.com/59784.
TEST_F(CopyTreeWorkItemTest, DISABLED_CopyTree) {
  // Create source tree
  base::FilePath dir_name_from(test_dir_.GetPath());
  dir_name_from = dir_name_from.AppendASCII("from");
  base::CreateDirectory(dir_name_from);
  ASSERT_TRUE(base::PathExists(dir_name_from));

  base::FilePath dir_name_from_1(dir_name_from);
  dir_name_from_1 = dir_name_from_1.AppendASCII("1");
  base::CreateDirectory(dir_name_from_1);
  ASSERT_TRUE(base::PathExists(dir_name_from_1));

  base::FilePath dir_name_from_2(dir_name_from);
  dir_name_from_2 = dir_name_from_2.AppendASCII("2");
  base::CreateDirectory(dir_name_from_2);
  ASSERT_TRUE(base::PathExists(dir_name_from_2));

  base::FilePath file_name_from_1(dir_name_from_1);
  file_name_from_1 = file_name_from_1.AppendASCII("File_1.txt");
  CreateTextFile(file_name_from_1.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from_1));

  base::FilePath file_name_from_2(dir_name_from_2);
  file_name_from_2 = file_name_from_2.AppendASCII("File_2.txt");
  CreateTextFile(file_name_from_2.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_from_2));

  base::FilePath dir_name_to(test_dir_.GetPath());
  dir_name_to = dir_name_to.AppendASCII("to");

  // test Do()
  {
    std::unique_ptr<CopyTreeWorkItem> work_item(
        WorkItem::CreateCopyTreeWorkItem(dir_name_from, dir_name_to,
                                         temp_dir_.GetPath(), WorkItem::ALWAYS,
                                         base::FilePath()));

    EXPECT_TRUE(work_item->Do());
  }

  base::FilePath file_name_to_1(dir_name_to);
  file_name_to_1 = file_name_to_1.AppendASCII("1");
  file_name_to_1 = file_name_to_1.AppendASCII("File_1.txt");
  EXPECT_TRUE(base::PathExists(file_name_to_1));
  VLOG(1) << "compare " << file_name_from_1.value() << " and "
          << file_name_to_1.value();
  EXPECT_TRUE(base::ContentsEqual(file_name_from_1, file_name_to_1));

  base::FilePath file_name_to_2(dir_name_to);
  file_name_to_2 = file_name_to_2.AppendASCII("2");
  file_name_to_2 = file_name_to_2.AppendASCII("File_2.txt");
  EXPECT_TRUE(base::PathExists(file_name_to_2));
  VLOG(1) << "compare " << file_name_from_2.value() << " and "
          << file_name_to_2.value();
  EXPECT_TRUE(base::ContentsEqual(file_name_from_2, file_name_to_2));
}
