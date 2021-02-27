// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_web_view.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/views/chrome_test_widget.h"

using DownloadShelfWebViewTest = BrowserWithTestWindowTest;

TEST_F(DownloadShelfWebViewTest, BorderSet) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.context = GetContext();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  ChromeTestWidget widget;
  widget.Init(std::move(params));
  DownloadShelfWebView* view =
      widget.SetContentsView(std::make_unique<DownloadShelfWebView>(browser()));
  EXPECT_NE(view->GetBorder(), nullptr);
}
