// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_file_utils.h"

#include <cstdint>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"

namespace web_app {

scoped_refptr<TestFileUtils> TestFileUtils::Create(
    absl::flat_hash_map<base::FilePath, base::FilePath> read_file_rerouting) {
  return base::MakeRefCounted<TestFileUtils>(std::move(read_file_rerouting));
}

TestFileUtils::TestFileUtils(
    absl::flat_hash_map<base::FilePath, base::FilePath> read_file_rerouting)
    : read_file_rerouting_(std::move(read_file_rerouting)) {}

TestFileUtils::~TestFileUtils() = default;

void TestFileUtils::SetRemainingDiskSpaceSize(int remaining_disk_space) {
  base::AutoLock lock(lock_);
  remaining_disk_space_ = remaining_disk_space;
}

void TestFileUtils::SetNextDeleteFileRecursivelyResult(
    std::optional<bool> delete_result) {
  base::AutoLock lock(lock_);
  delete_file_recursively_result_ = delete_result;
}

void TestFileUtils::SetDeleteFileRecursivelyResult(const base::FilePath& path,
                                                   bool result) {
  base::AutoLock lock(lock_);
  delete_file_recursively_results_[path] = result;
}

bool TestFileUtils::WriteFile(const base::FilePath& filename,
                              base::span<const uint8_t> file_data) {
  bool disk_full = false;
  int size_written = 0;
  {
    base::AutoLock lock(lock_);
    if (remaining_disk_space_ != kNoLimit) {
      int data_size = base::checked_cast<int>(file_data.size());
      if (data_size > remaining_disk_space_) {
        // Disk full:
        disk_full = true;
        size_written = remaining_disk_space_;
        remaining_disk_space_ = 0;
      } else {
        remaining_disk_space_ -= file_data.size();
      }
    }
  }

  if (disk_full) {
    if (size_written > 0) {
      FileUtilsWrapper::WriteFile(filename, file_data);
    }
    return size_written;
  }

  return FileUtilsWrapper::WriteFile(filename, file_data);
}

bool TestFileUtils::ReadFileToString(const base::FilePath& path,
                                     std::string* contents) {
  std::optional<base::FilePath> rerouted_path;
  {
    base::AutoLock lock(lock_);
    auto it = read_file_rerouting_.find(path);
    if (it != read_file_rerouting_.end()) {
      rerouted_path = it->second;
    }
  }
  if (rerouted_path.has_value()) {
    return FileUtilsWrapper::ReadFileToString(*rerouted_path, contents);
  }
  return FileUtilsWrapper::ReadFileToString(path, contents);
}

bool TestFileUtils::DeleteFile(const base::FilePath& path, bool recursive) {
  {
    base::AutoLock lock(lock_);
    deleted_files_.push_back(path);
  }
  return FileUtilsWrapper::DeleteFile(path, recursive);
}

bool TestFileUtils::DeleteFileRecursively(const base::FilePath& path) {
  std::optional<bool> custom_result;
  {
    base::AutoLock lock(lock_);
    deleted_files_.push_back(path);
    auto it = delete_file_recursively_results_.find(path);
    if (it != delete_file_recursively_results_.end()) {
      return it->second;
    }
    custom_result = delete_file_recursively_result_;
  }
  return custom_result ? *custom_result
                       : FileUtilsWrapper::DeleteFileRecursively(path);
}

TestFileUtils* TestFileUtils::AsTestFileUtils() {
  return this;
}

std::vector<base::FilePath> TestFileUtils::deleted_files() const {
  base::AutoLock lock(lock_);
  return deleted_files_;
}

}  // namespace web_app
