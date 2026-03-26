// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
BrowserWindowInterface* GetBrowserWindow() {
  std::vector<BrowserWindowInterface*> windows =
      GetAllBrowserWindowInterfaces();
  EXPECT_EQ(1u, windows.size())
      << "We don't expect more than one window in this test.";
  return windows[0];
}
}  // namespace

class SidePanelCoordinatorAndroidBrowserTest : public AndroidBrowserTest {
 public:
  SidePanelCoordinatorAndroidBrowserTest() {
    // Note:
    //
    // Java code reads the cached `kEnableAndroidSidePanel` flag because we
    // use this flag to decide whether to inflate the main layout that contains
    // the side panel container, which happens _before_ the native library is
    // loaded.
    //
    // As of Mar 25, 2026, there was no way to override a cached flag in native
    // browser tests, so http://crrev.com/c/7689838 made the default value of
    // the cached flag `true` in tests.
    //
    // However, we still need to explicitly enable the flag here:
    //
    // On a newly installed ChromeBrowserTests APK, the `SharedPreferences`
    // backing the cached flag is empty so http://crrev.com/c/7689838 makes the
    // test pass the 1st run.
    //
    // After the 1st run, the `SharedPreferences` will contain the key for the
    // cached flag, but the default value of the cached flag won't be persisted.
    // If we don't explicitly enable the flag here, the cached flag value will
    // be `false` on subsequent runs and the tests will fail.
    feature_list_.InitAndEnableFeature(
        chrome::android::kEnableAndroidSidePanel);
  }

  ~SidePanelCoordinatorAndroidBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SidePanelCoordinatorAndroidBrowserTest,
                       TestSidePanelUIProvider) {
  SidePanelUI* side_panel_ui = SidePanelUIProvider::From(GetBrowserWindow());
  EXPECT_NE(nullptr, side_panel_ui);
}
