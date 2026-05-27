// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_TEST_ANDROID_SIDE_PANEL_ANDROID_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_TEST_ANDROID_SIDE_PANEL_ANDROID_BROWSER_TEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/test/android/browser_window_android_browsertest_base.h"

class BrowserWindowInterface;

namespace tabs {
class TabInterface;
}

// Base class for Android side panel browser tests that handles the feature
// flags initialization and verification.
class SidePanelAndroidBrowserTestBase
    : public BrowserWindowAndroidBrowserTestBase {
 public:
  SidePanelAndroidBrowserTestBase();
  ~SidePanelAndroidBrowserTestBase() override;

 protected:
  // Returns the last active `BrowserWindowInterface` for this test.
  static BrowserWindowInterface* GetLastActiveBrowser();

  // Returns the active tab in this test's last active browser window.
  static tabs::TabInterface* GetActiveTabInLastActiveBrowser();

  // Implements `AndroidBrowserTest`:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_TEST_ANDROID_SIDE_PANEL_ANDROID_BROWSER_TEST_BASE_H_
