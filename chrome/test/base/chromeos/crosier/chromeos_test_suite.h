// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_TEST_SUITE_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_TEST_SUITE_H_

#include "build/chromeos_buildflags.h"
#include "content/public/test/content_test_suite_base.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/files/scoped_temp_dir.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Test suite for chromeos integration test on device.
// Creates services needed by Ash or Lacros.
class ChromeOSTestSuite : public content::ContentTestSuiteBase {
 public:
  ChromeOSTestSuite(int argc, char** argv);
  ChromeOSTestSuite(const ChromeOSTestSuite&) = delete;
  ChromeOSTestSuite& operator=(const ChromeOSTestSuite&) = delete;
  ~ChromeOSTestSuite() override;

 protected:
  // content::ContentTestSuiteBase
  void Initialize() override;

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Used for download and documents path overrides.
  base::ScopedTempDir scoped_temp_dir_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_TEST_SUITE_H_
