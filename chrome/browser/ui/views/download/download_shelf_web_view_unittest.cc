// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_web_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/test/views/chrome_test_widget.h"

using DownloadShelfWebViewTest = TestWithBrowserView;

TEST_F(DownloadShelfWebViewTest, VisibilityTest) {
  auto* download_shelf = browser_view()->AddChildView(
      std::make_unique<DownloadShelfWebView>(browser(), browser_view()));
  browser_view()->SetDownloadShelfForTest(download_shelf);

  // Initially hidden.
  EXPECT_FALSE(download_shelf->GetVisible());
  download_shelf->DoOpen();
  EXPECT_TRUE(download_shelf->GetVisible());
  download_shelf->DoClose();
  // Should still be visible during closing because of animation.
  EXPECT_TRUE(download_shelf->GetVisible());
  download_shelf->DoUnhide();
  EXPECT_TRUE(download_shelf->GetVisible());
  download_shelf->DoHide();
  EXPECT_FALSE(download_shelf->GetVisible());
}
