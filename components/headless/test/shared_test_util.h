// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_TEST_SHARED_TEST_UTIL_H_
#define COMPONENTS_HEADLESS_TEST_SHARED_TEST_UTIL_H_

#include <string_view>

#include "base/files/file_path.h"

namespace headless {

// Returns true if the specified script name refers to a shared script.
bool IsSharedTestScript(std::string_view script_name);

// Returns expectation file path given the test script path.
base::FilePath GetTestExpectationFilePath(
    const base::FilePath& test_script_path);

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_TEST_SHARED_TEST_UTIL_H_
