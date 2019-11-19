// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_FILE_UTILS_WRAPPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_FILE_UTILS_WRAPPER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"

// Include this to avoid conflicts with CreateDirectory Win macro.
// It converts CreateDirectory into CreateDirectoryW.
#if defined(OS_WIN)
#include "base/win/windows_types.h"
#endif  // defined(OS_WIN)

namespace base {
class FilePath;
}

namespace web_app {

// A simple wrapper for base/files/file_util.h utilities.
// See detailed comments for functionality in corresponding
// base/files/file_util.h functions.
// Allows a testing implementation to intercept calls to the file system.
// TODO(loyso): Add more tests and promote mocked methods to |virtual|.
class FileUtilsWrapper {
 public:
  FileUtilsWrapper() = default;
  virtual ~FileUtilsWrapper() = default;

  // Create a copy to use in IO task.
  virtual std::unique_ptr<FileUtilsWrapper> Clone();

  bool PathExists(const base::FilePath& path);

  bool PathIsWritable(const base::FilePath& path);

  bool DirectoryExists(const base::FilePath& path);

  bool CreateDirectory(const base::FilePath& full_path);

  int ReadFile(const base::FilePath& filename, char* data, int max_size);

  virtual int WriteFile(const base::FilePath& filename,
                        const char* data,
                        int size);

  bool Move(const base::FilePath& from_path, const base::FilePath& to_path);

  bool IsDirectoryEmpty(const base::FilePath& dir_path);

  bool ReadFileToString(const base::FilePath& path, std::string* contents);

  bool DeleteFile(const base::FilePath& path, bool recursive);

  virtual bool DeleteFileRecursively(const base::FilePath& path);

  DISALLOW_ASSIGN(FileUtilsWrapper);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_FILE_UTILS_WRAPPER_H_
