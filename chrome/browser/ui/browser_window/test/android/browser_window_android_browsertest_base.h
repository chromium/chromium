// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_ANDROID_BROWSER_WINDOW_ANDROID_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_ANDROID_BROWSER_WINDOW_ANDROID_BROWSERTEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/android/android_browser_test.h"

class BrowserWindowAndroidBrowserTestBase : public AndroidBrowserTest {
 public:
  BrowserWindowAndroidBrowserTestBase();
  ~BrowserWindowAndroidBrowserTestBase() override;

  // AndroidBrowserTest:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override;

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_TEST_ANDROID_BROWSER_WINDOW_ANDROID_BROWSERTEST_BASE_H_
