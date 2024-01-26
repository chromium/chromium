// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_UTILS_H_

#include <map>
#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"

namespace web_app {

// A testing implementation to intercept calls to the file system.
class TestFileUtils : public FileUtilsWrapper {
 public:
  // Initializer list type deduction does not work through std::make_unique so
  // provide this helper function.
  static scoped_refptr<TestFileUtils> Create(
      std::map<base::FilePath, base::FilePath> read_file_rerouting);

  explicit TestFileUtils(
      std::map<base::FilePath, base::FilePath> read_file_rerouting = {});
  TestFileUtils(const TestFileUtils&) = delete;
  TestFileUtils& operator=(const TestFileUtils&) = delete;

  // FileUtilsWrapper:
  bool WriteFile(const base::FilePath& filename,
                 base::span<const uint8_t> file_data) override;
  bool ReadFileToString(const base::FilePath& path,
                        std::string* contents) override;
  bool DeleteFileRecursively(const base::FilePath& path) override;

  static constexpr int kNoLimit = -1;

  // Simulate "disk full" error: limit disk space for |WriteFile| operations.
  void SetRemainingDiskSpaceSize(int remaining_disk_space);

  void SetNextDeleteFileRecursivelyResult(std::optional<bool> delete_result);

  TestFileUtils* AsTestFileUtils() override;

 private:
  ~TestFileUtils() override;

  std::map<base::FilePath, base::FilePath> read_file_rerouting_;
  std::optional<bool> delete_file_recursively_result_;
  int remaining_disk_space_ = kNoLimit;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_UTILS_H_
