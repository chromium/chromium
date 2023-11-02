// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_SCOPED_LOGGING_H_
#define CHROME_CHROME_CLEANER_LOGGING_SCOPED_LOGGING_H_

#include <string>

#include "base/files/file_path.h"

namespace chrome_cleaner {

// A utility to print the "Null" string for null wchar_t*.
std::wstring ConvertIfNull(const wchar_t* str);

// Un/Initialize the logging machinery. A |suffix| can be appended to the log
// file name if necessary.
class ScopedLogging {
 public:
  explicit ScopedLogging(base::FilePath::StringPieceType suffix);

  ScopedLogging(const ScopedLogging&) = delete;
  ScopedLogging& operator=(const ScopedLogging&) = delete;

  ~ScopedLogging();

  // Returns the path to the log file for the current process.
  static base::FilePath GetLogFilePath(base::FilePath::StringPieceType suffix);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_SCOPED_LOGGING_H_
