// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_frame_view.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

class BubbleFrameViewBrowserTest : public DialogBrowserTest {
 public:
  BubbleFrameViewBrowserTest() = default;

  BubbleFrameViewBrowserTest(const BubbleFrameViewBrowserTest&) = delete;
  BubbleFrameViewBrowserTest& operator=(const BubbleFrameViewBrowserTest&) =
      delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    const std::u16string subtitle =
        u"Subtitle that is long enough to wrap-around a bubble once";
    auto dialog_model = ui::DialogModel::Builder()
                            .SetTitle(u"Title")
                            .SetSubtitle(subtitle)
                            .Build();

    views::View* anchor_view =
        BrowserView::GetBrowserViewForBrowser(browser())->top_container();

    auto bubble = std::make_unique<views::BubbleDialogModelHost>(
        std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);

    views::Widget* const widget =
        views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
    widget->Show();
  }
};

IN_PROC_BROWSER_TEST_F(BubbleFrameViewBrowserTest, InvokeUi_Subtitle) {
  ShowAndVerifyUi();
}
