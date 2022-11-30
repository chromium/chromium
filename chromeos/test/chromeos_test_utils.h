// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TEST_CHROMEOS_TEST_UTILS_H_
#define CHROMEOS_TEST_CHROMEOS_TEST_UTILS_H_

#include <string>

namespace base {
class FilePath;
}

namespace chromeos {
namespace test_utils {

// Returns the path to the given test data file for this library.
bool GetTestDataPath(const std::string& component,
                     const std::string& filename,
                     base::FilePath* data_dir);

}  // namespace test_utils
}  // namespace chromeos

#endif  // CHROMEOS_TEST_CHROMEOS_TEST_UTILS_H_
