// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_file_util.h"

#include <windows.h>

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

bool CreateEmptyFile(const base::FilePath& path) {
  DCHECK(!base::PathExists(path));
  DCHECK(base::PathExists(path.DirName()));

  FILE* file = base::OpenFile(path, "wb+");
  if (file == nullptr) {
    PLOG(WARNING) << "Failed to create file: " << path.value();
    return false;
  }
  base::CloseFile(file);
  return true;
}

bool CreateFileInFolder(const base::FilePath& folder,
                        const std::wstring& name) {
  DCHECK(!name.empty());
  DCHECK(base::PathExists(folder));
  base::FilePath file_path = folder.Append(name);
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS);
  file.Close();
  return base::PathExists(file_path);
}

void CreateFileWithContent(const base::FilePath& path,
                           const char* content,
                           size_t content_length) {
  CreateFileWithRepeatedContent(path, content, content_length, 1);
}

void CreateFileWithRepeatedContent(const base::FilePath& path,
                                   const char* content,
                                   size_t content_length,
                                   size_t count) {
  DCHECK(content);
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid())
      << base::File::ErrorToString(file.error_details());
  for (size_t i = 0; i < count; ++i)
    ASSERT_EQ(content_length, static_cast<size_t>(file.WriteAtCurrentPos(
                                  content, content_length)));
}

base::win::ScopedHandle CreateFileWithContent(
    const std::string& content,
    const std::wstring& file_name,
    const base::ScopedTempDir& temp_dir) {
  base::FilePath path(temp_dir.GetPath().Append(file_name));
  EXPECT_TRUE(base::WriteFile(path, content));
  std::wstring wide_file_path = path.value();
  base::win::ScopedHandle file_handle(
      ::CreateFile(wide_file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL));
  EXPECT_TRUE(file_handle.IsValid());
  return file_handle;
}

void CreateFileAndGetShortName(const base::FilePath& long_name_path,
                               base::FilePath* short_name_path) {
  DCHECK(short_name_path);
  // The file must exist for the short to long path name conversion to work.
  base::File file(long_name_path, base::File::FLAG_OPEN_ALWAYS);
  file.Close();
  DWORD short_name_len =
      ::GetShortPathName(long_name_path.value().c_str(), nullptr, 0);
  ASSERT_GT(short_name_len, 0UL);

  std::wstring short_name_string;
  short_name_len = ::GetShortPathName(
      long_name_path.value().c_str(),
      base::WriteInto(&short_name_string, short_name_len), short_name_len);
  ASSERT_GT(short_name_len, 0UL);
  *short_name_path = base::FilePath(short_name_string);
}

}  // namespace chrome_cleaner
