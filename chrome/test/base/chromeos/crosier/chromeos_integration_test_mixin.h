// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_TEST_MIXIN_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_TEST_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace base {
class CommandLine;
}

// Mixin for all tests that run in chromeos_integration_tests. Handles setup for
// running on hardware (DUT) and running in VM.
class ChromeOSIntegrationTestMixin : public InProcessBrowserTestMixin {
 public:
  explicit ChromeOSIntegrationTestMixin(InProcessBrowserTestMixinHost* host);
  ChromeOSIntegrationTestMixin(const ChromeOSIntegrationTestMixin&) = delete;
  ChromeOSIntegrationTestMixin& operator=(const ChromeOSIntegrationTestMixin&) =
      delete;
  ~ChromeOSIntegrationTestMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  bool SetUpUserDataDirectory() override;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_TEST_MIXIN_H_
