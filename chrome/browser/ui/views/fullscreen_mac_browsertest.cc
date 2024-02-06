// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen_util_mac.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"

class FullscreenMacTest : public InProcessBrowserTest {
 public:
  FullscreenMacTest() {
    feature_list_.InitAndEnableFeature(features::kImmersiveFullscreen);
  }

  base::test::ScopedFeatureList feature_list_;
};

// Checks that the toolbar widget is considered to be in fullscreen when the
// browser enters fullscreen.
IN_PROC_BROWSER_TEST_F(FullscreenMacTest, ToolbarWidgetFullscreen) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->top_container()
                  ->GetWidget()
                  ->IsFullscreen());
}
