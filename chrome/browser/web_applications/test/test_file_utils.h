// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_UTILS_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"

namespace web_app {

// A testing implementation to intercept calls to the file system.
class TestFileUtils : public FileUtilsWrapper {
 public:
  TestFileUtils();
  TestFileUtils(const TestFileUtils&);
  ~TestFileUtils() override;

  // FileUtilsWrapper:
  std::unique_ptr<FileUtilsWrapper> Clone() override;
  int WriteFile(const base::FilePath& filename,
                const char* data,
                int size) override;
  bool DeleteFileRecursively(const base::FilePath& path) override;

  static constexpr int kNoLimit = -1;

  // Simulate "disk full" error: limit disk space for |WriteFile| operations.
  void SetRemainingDiskSpaceSize(int remaining_disk_space);

  void SetNextDeleteFileRecursivelyResult(base::Optional<bool> delete_result);

 private:
  base::Optional<bool> delete_file_recursively_result_;
  int remaining_disk_space_ = kNoLimit;

  DISALLOW_ASSIGN(TestFileUtils);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_UTILS_H_
