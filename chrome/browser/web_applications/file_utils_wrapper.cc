// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/web_applications/file_utils_wrapper.h"

#include <cstdint>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"

namespace web_app {

bool FileUtilsWrapper::PathExists(const base::FilePath& path) {
  return base::PathExists(path);
}

bool FileUtilsWrapper::DirectoryExists(const base::FilePath& path) {
  return base::DirectoryExists(path);
}

bool FileUtilsWrapper::CreateDirectory(const base::FilePath& full_path) {
  return base::CreateDirectory(full_path);
}

bool FileUtilsWrapper::GetFileInfo(const base::FilePath& file_path,
                                   base::File::Info* info) {
  return base::GetFileInfo(file_path, info);
}

bool FileUtilsWrapper::WriteFile(const base::FilePath& filename,
                                 base::span<const uint8_t> file_data) {
  return base::WriteFile(filename, file_data);
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
  if (!recursive)
    return base::DeleteFile(path);
  return base::DeletePathRecursively(path);
}

bool FileUtilsWrapper::DeleteFileRecursively(const base::FilePath& path) {
  return base::DeletePathRecursively(path);
}

TestFileUtils* FileUtilsWrapper::AsTestFileUtils() {
  return nullptr;
}

}  // namespace web_app
