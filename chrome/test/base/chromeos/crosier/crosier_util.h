// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_CROSIER_UTIL_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_CROSIER_UTIL_H_

#include "chrome/test/base/chromeos/crosier/chromeos_test_definition.pb.h"

namespace crosier_util {

// Adds test information. Call this during test setup or at the beginning of the
// test body. Setting test info helps other engineers to understand the test and
// its ownership. See |chromeos_test_definition.proto| for more information.
void AddTestInfo(const chrome_test_base_chromeos_crosier::TestInfo& info);

}  // namespace crosier_util

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_CROSIER_UTIL_H_
