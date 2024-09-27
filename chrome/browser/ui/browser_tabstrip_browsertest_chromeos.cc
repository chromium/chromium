// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_pin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace chrome {

using BrowserTabstripBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserTabstripBrowserTest,
                       AddTabAtNavigationBlockedInLockedFullscreen) {
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  // Set locked fullscreen state.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);
  AddTabAt(browser(), GURL("https://google.com"),
           /*index=*/0,
           /*foreground=*/true);

  // No tab added.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

}  // namespace chrome
