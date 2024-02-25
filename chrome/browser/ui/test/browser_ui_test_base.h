// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_BROWSER_UI_TEST_BASE_H_
#define CHROME_BROWSER_UI_TEST_BROWSER_UI_TEST_BASE_H_

#include "testing/gtest/include/gtest/gtest.h"

// Base class that provides a `Invoke` method to invoke tests that
// inherit from `UiBrowserTest` or `DialogBrowserTest`. This is not
// intended to run on bots, but to assist UI inspection in local builds.
// See //docs/testing/test_browser_dialog.md.
class BrowserUiTestBase : public ::testing::Test {
 public:
  BrowserUiTestBase() = default;
  BrowserUiTestBase(const BrowserUiTestBase&) = delete;
  BrowserUiTestBase& operator=(const BrowserUiTestBase&) = delete;

  // Prints or runs interactive tests.
  // If no command line is specified, prints gtests that have "InvokeUi_"
  // in their names.
  // If `--ui=TEST_NAME` is provided, invokes that test case in a subprocess.
  // If `--test-launcher-interactive` is specified, the test won't end
  // until the UI dismissed manually, allowing for interactive inspection.
  void Invoke();

 protected:
  ~BrowserUiTestBase() override = default;
};

#endif  // CHROME_BROWSER_UI_TEST_BROWSER_UI_TEST_BASE_H_
