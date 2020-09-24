// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_file_utils.h"

#include <utility>

#include "base/files/file_path.h"

namespace web_app {

std::unique_ptr<TestFileUtils> TestFileUtils::Create(
    std::map<base::FilePath, base::FilePath> read_file_rerouting) {
  return std::make_unique<TestFileUtils>(read_file_rerouting);
}

TestFileUtils::TestFileUtils(
    std::map<base::FilePath, base::FilePath> read_file_rerouting)
    : read_file_rerouting_(read_file_rerouting) {}

TestFileUtils::TestFileUtils(const TestFileUtils&) = default;

TestFileUtils::~TestFileUtils() = default;

std::unique_ptr<FileUtilsWrapper> TestFileUtils::Clone() const {
  return std::make_unique<TestFileUtils>(*this);
}

void TestFileUtils::SetRemainingDiskSpaceSize(int remaining_disk_space) {
  remaining_disk_space_ = remaining_disk_space;
}

void TestFileUtils::SetNextDeleteFileRecursivelyResult(
    base::Optional<bool> delete_result) {
  delete_file_recursively_result_ = delete_result;
}

int TestFileUtils::WriteFile(const base::FilePath& filename,
                             const char* data,
                             int size) {
  if (remaining_disk_space_ != kNoLimit) {
    if (size > remaining_disk_space_) {
      // Disk full:
      const int size_written = remaining_disk_space_;
      if (size_written > 0)
        FileUtilsWrapper::WriteFile(filename, data, size_written);
      remaining_disk_space_ = 0;
      return size_written;
    }

    remaining_disk_space_ -= size;
  }

  return FileUtilsWrapper::WriteFile(filename, data, size);
}

bool TestFileUtils::ReadFileToString(const base::FilePath& path,
                                     std::string* contents) {
  for (const std::pair<const base::FilePath, base::FilePath>& route :
       read_file_rerouting_) {
    if (route.first == path)
      return FileUtilsWrapper::ReadFileToString(route.second, contents);
  }
  return FileUtilsWrapper::ReadFileToString(path, contents);
}

bool TestFileUtils::DeleteFileRecursively(const base::FilePath& path) {
  return delete_file_recursively_result_
             ? *delete_file_recursively_result_
             : FileUtilsWrapper::DeleteFileRecursively(path);
}

}  // namespace web_app
