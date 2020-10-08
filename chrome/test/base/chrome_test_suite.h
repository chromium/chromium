// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_TEST_SUITE_H_
#define CHROME_TEST_BASE_CHROME_TEST_SUITE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "content/public/test/content_test_suite_base.h"

// Test suite for unit and browser tests. Creates services needed by both.
// See also ChromeUnitTestSuite for additional services created for unit tests.
class ChromeTestSuite : public content::ContentTestSuiteBase {
 public:
  ChromeTestSuite(int argc, char** argv);
  ChromeTestSuite(const ChromeTestSuite&) = delete;
  ChromeTestSuite& operator=(const ChromeTestSuite&) = delete;
  ~ChromeTestSuite() override;

 protected:
  // base::TestSuite overrides:
  void Initialize() override;
  void Shutdown() override;

  void SetBrowserDirectory(const base::FilePath& browser_dir) {
    browser_dir_ = browser_dir;
  }

  // Alternative path to browser binaries.
  base::FilePath browser_dir_;
};

#endif  // CHROME_TEST_BASE_CHROME_TEST_SUITE_H_
