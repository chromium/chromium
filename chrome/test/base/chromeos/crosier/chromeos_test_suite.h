// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_TEST_SUITE_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_TEST_SUITE_H_

#include "content/public/test/content_test_suite_base.h"

// Test suite for chromeos integration test on device.
// Creates services needed by Ash.
class ChromeOSTestSuite : public content::ContentTestSuiteBase {
 public:
  ChromeOSTestSuite(int argc, char** argv);
  ChromeOSTestSuite(const ChromeOSTestSuite&) = delete;
  ChromeOSTestSuite& operator=(const ChromeOSTestSuite&) = delete;
  ~ChromeOSTestSuite() override;

 protected:
  // content::ContentTestSuiteBase
  void Initialize() override;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_TEST_SUITE_H_
