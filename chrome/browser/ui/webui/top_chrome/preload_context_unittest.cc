// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/preload_context.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webui {

using PreloadContextTest = BrowserWithTestWindowTest;

// Tests creating PreloadContext from Profile and Browser.
TEST_F(PreloadContextTest, Basic) {
  PreloadContext brower_context = PreloadContext::From(browser());
  EXPECT_TRUE(brower_context.IsBrowser());
  EXPECT_FALSE(brower_context.IsProfile());
  EXPECT_EQ(brower_context.GetBrowser(), browser());
  EXPECT_EQ(brower_context.GetProfile(), nullptr);

  PreloadContext profile_context = PreloadContext::From(GetProfile());
  EXPECT_FALSE(profile_context.IsBrowser());
  EXPECT_TRUE(profile_context.IsProfile());
  EXPECT_EQ(profile_context.GetBrowser(), nullptr);
  EXPECT_EQ(profile_context.GetProfile(), GetProfile());
}

}  // namespace webui
