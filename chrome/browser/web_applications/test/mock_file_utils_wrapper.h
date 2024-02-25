// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_FILE_UTILS_WRAPPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_FILE_UTILS_WRAPPER_H_

#include <cstdint>

#include "base/containers/span.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace web_app {

class MockFileUtilsWrapper : public FileUtilsWrapper {
 public:
  MockFileUtilsWrapper();

  MOCK_METHOD(bool,
              WriteFile,
              (const base::FilePath& filename,
               base::span<const uint8_t> file_data),
              (override));
  MOCK_METHOD(bool,
              ReadFileToString,
              (const base::FilePath& path, std::string* contents),
              (override));
  MOCK_METHOD(bool,
              DeleteFileRecursively,
              (const base::FilePath& path),
              (override));

 protected:
  ~MockFileUtilsWrapper() override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_FILE_UTILS_WRAPPER_H_
