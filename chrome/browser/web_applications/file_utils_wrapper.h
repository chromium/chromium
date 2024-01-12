// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_FILE_UTILS_WRAPPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_FILE_UTILS_WRAPPER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"

// Include this to avoid conflicts with CreateDirectory Win macro.
// It converts CreateDirectory into CreateDirectoryW.
#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif  // BUILDFLAG(IS_WIN)

namespace base {
class FilePath;
}

namespace web_app {

class TestFileUtils;

// A simple wrapper for base/files/file_util.h utilities.
// See detailed comments for functionality in corresponding
// base/files/file_util.h functions.
// Allows a testing implementation to intercept calls to the file system.
class FileUtilsWrapper : public base::RefCountedThreadSafe<FileUtilsWrapper> {
 public:
  FileUtilsWrapper() = default;

  FileUtilsWrapper& operator=(const FileUtilsWrapper&) = delete;

  bool PathExists(const base::FilePath& path);

  bool DirectoryExists(const base::FilePath& path);

  bool CreateDirectory(const base::FilePath& full_path);

  bool GetFileInfo(const base::FilePath& file_path, base::File::Info* info);

  virtual bool WriteFile(const base::FilePath& filename,
                         base::span<const uint8_t> file_data);

  bool Move(const base::FilePath& from_path, const base::FilePath& to_path);

  bool IsDirectoryEmpty(const base::FilePath& dir_path);

  virtual bool ReadFileToString(const base::FilePath& path,
                                std::string* contents);

  bool DeleteFile(const base::FilePath& path, bool recursive);

  virtual bool DeleteFileRecursively(const base::FilePath& path);

  virtual TestFileUtils* AsTestFileUtils();

 protected:
  friend class base::RefCountedThreadSafe<FileUtilsWrapper>;
  virtual ~FileUtilsWrapper() = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_FILE_UTILS_WRAPPER_H_
