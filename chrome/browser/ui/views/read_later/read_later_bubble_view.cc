// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/read_later/read_later_bubble_view.h"

#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

namespace {
// ReadLaterWebView ---------------------------------------------------------
class ReadLaterWebView : public views::WebView {
 public:
  explicit ReadLaterWebView(content::BrowserContext* context)
      : views::WebView(context) {}
  ReadLaterWebView(const ReadLaterWebView&) = delete;
  ReadLaterWebView& operator=(const ReadLaterWebView&) = delete;
};

}  // namespace

// ReadLaterBubbleView ---------------------------------------------------------

// static
base::WeakPtr<ReadLaterBubbleView> ReadLaterBubbleView::Show(
    const Browser* browser,
    views::View* anchor_view) {
  ReadLaterBubbleView* bubble = new ReadLaterBubbleView(browser, anchor_view);
  views::Widget* const widget = BubbleDialogDelegateView::CreateBubble(bubble);
  widget->Show();
  return bubble->weak_factory_.GetWeakPtr();
}

ReadLaterBubbleView::ReadLaterBubbleView(const Browser* browser,
                                         views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      web_view_(std::make_unique<ReadLaterWebView>(browser->profile())) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(gfx::Insets());

  AddChildView(web_view_.get());

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // TODO(corising): Remove this and add function to calculate preferred size.
  web_view_->SetPreferredSize(gfx::Size(300, 500));
  web_view_->LoadInitialURL(GURL(chrome::kChromeUIReadLaterURL));
}

ReadLaterBubbleView::~ReadLaterBubbleView() = default;

void ReadLaterBubbleView::AddedToWidget() {
  BubbleDialogDelegateView::AddedToWidget();
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(GetCornerRadius()));
}

void ReadLaterBubbleView::ReadingListModelLoaded(
    const ReadingListModel* model) {}
