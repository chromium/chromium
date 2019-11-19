// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/file_utils_wrapper.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"

namespace web_app {

std::unique_ptr<FileUtilsWrapper> FileUtilsWrapper::Clone() {
  return std::make_unique<FileUtilsWrapper>();
}

bool FileUtilsWrapper::PathExists(const base::FilePath& path) {
  return base::PathExists(path);
}

bool FileUtilsWrapper::PathIsWritable(const base::FilePath& path) {
  return base::PathIsWritable(path);
}

bool FileUtilsWrapper::DirectoryExists(const base::FilePath& path) {
  return base::DirectoryExists(path);
}

bool FileUtilsWrapper::CreateDirectory(const base::FilePath& full_path) {
  return base::CreateDirectory(full_path);
}

int FileUtilsWrapper::ReadFile(const base::FilePath& filename,
                               char* data,
                               int max_size) {
  return base::ReadFile(filename, data, max_size);
}

int FileUtilsWrapper::WriteFile(const base::FilePath& filename,
                                const char* data,
                                int size) {
  return base::WriteFile(filename, data, size);
}

bool FileUtilsWrapper::Move(const base::FilePath& from_path,
                            const base::FilePath& to_path) {
  return base::Move(from_path, to_path);
}

bool FileUtilsWrapper::IsDirectoryEmpty(const base::FilePath& dir_path) {
  return base::IsDirectoryEmpty(dir_path);
}

bool FileUtilsWrapper::ReadFileToString(const base::FilePath& path,
                                        std::string* contents) {
  return base::ReadFileToString(path, contents);
}

bool FileUtilsWrapper::DeleteFile(const base::FilePath& path, bool recursive) {
  return base::DeleteFile(path, recursive);
}

bool FileUtilsWrapper::DeleteFileRecursively(const base::FilePath& path) {
  return base::DeleteFileRecursively(path);
}

}  // namespace web_app
