// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_TEST_PATH_UTILS_H_
#define CHROME_TEST_BASE_CHROME_TEST_PATH_UTILS_H_

#include "base/files/file_path.h"

class GURL;

namespace chrome_test_utils {

// Returns the test data path used by the embedded test server.
base::FilePath GetChromeTestDataDir();

// Overrides the path chrome::DIR_TEST_DATA. Used early in test startup so the
// value is available in constructors and SetUp methods.
void OverrideChromeTestDataDir();

// Generate the file path for testing a particular test.
// The file for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is base::FilePath format.
base::FilePath GetTestFilePath(const base::FilePath& dir,
                               const base::FilePath& file);

// Generate the URL for testing a particular test.
// HTML for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is GURL format.
GURL GetTestUrl(const base::FilePath& dir, const base::FilePath& file);

}  // namespace chrome_test_utils

#endif  // CHROME_TEST_BASE_CHROME_TEST_PATH_UTILS_H_
