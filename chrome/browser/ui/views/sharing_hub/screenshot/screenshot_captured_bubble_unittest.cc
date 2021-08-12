// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/screenshot/screenshot_captured_bubble.h"

#include <vector>

#include "base/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace sharing_hub {

class ScreenshotCapturedBubbleTest : public ChromeViewsTestBase {};

TEST_F(ScreenshotCapturedBubbleTest, EditNavigatesToImageEditorWebUI) {
  const gfx::Image image;
  ScreenshotCapturedBubble bubble(
      nullptr, nullptr, image, nullptr,
      base::BindLambdaForTesting([&](NavigateParams* params) {
        EXPECT_EQ(chrome::kChromeUIImageEditorURL, params->url.spec());
        EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  params->disposition);
        EXPECT_EQ(NavigateParams::SHOW_WINDOW, params->window_action);
      }));
}

}  // namespace sharing_hub
