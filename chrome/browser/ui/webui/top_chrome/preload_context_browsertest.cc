// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/preload_context.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace webui {

using PreloadContextTest = InProcessBrowserTest;

// Tests creating PreloadContext from Profile and Browser.
IN_PROC_BROWSER_TEST_F(PreloadContextTest, Basic) {
  PreloadContext brower_context = PreloadContext::From(browser());
  EXPECT_TRUE(brower_context.IsBrowser());
  EXPECT_FALSE(brower_context.IsProfile());
  EXPECT_EQ(brower_context.GetBrowser(), browser());
  EXPECT_EQ(brower_context.GetProfile(), nullptr);

  PreloadContext profile_context = PreloadContext::From(browser()->profile());
  EXPECT_FALSE(profile_context.IsBrowser());
  EXPECT_TRUE(profile_context.IsProfile());
  EXPECT_EQ(profile_context.GetBrowser(), nullptr);
  EXPECT_EQ(profile_context.GetProfile(), browser()->profile());
}

}  // namespace webui
