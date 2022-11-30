// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_SCOPED_FILE_H_
#define CHROME_CHROME_CLEANER_TEST_SCOPED_FILE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"

// Holds the path for a file that gets deleted on destruction.
class ScopedFile {
 public:
  static std::unique_ptr<ScopedFile> Create(const base::FilePath& dir,
                                            const std::wstring& file_name,
                                            const std::string& contents);

  explicit ScopedFile(const base::FilePath& file_path);

  // Deletes the file with path |file_path_| if it exists. If the file doesn't
  // exist on destruction (which can be the case for tests involving UwS
  // removals), the destructor is a no-op.
  ~ScopedFile();

  const base::FilePath& file_path();

 private:
  base::FilePath file_path_;
};

#endif  // CHROME_CHROME_CLEANER_TEST_SCOPED_FILE_H_
