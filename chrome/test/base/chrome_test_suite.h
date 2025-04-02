// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_TEST_SUITE_H_
#define CHROME_TEST_BASE_CHROME_TEST_SUITE_H_

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/test/content_test_suite_base.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"
#endif

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

#if BUILDFLAG(IS_ANDROID)
  // InstanceID can make network requests which will time out and make tests
  // slow. Insert a fake one in all tests, as the prefetch service (perhaps
  // among others in the future) causes us to use the InstanceID in a posted
  // task which delays test completion.
  instance_id::ScopedUseFakeInstanceIDAndroid fake_instance_id_android_;
#endif
};

#endif  // CHROME_TEST_BASE_CHROME_TEST_SUITE_H_
