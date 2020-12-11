// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"

#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// The min / max size available to the WebBubbleDialogView.
// These are arbitrary sizes that match those set by ExtensionPopup.
// TODO(tluk): Determine the correct size constraints for the
// WebBubbleDialogView.
constexpr gfx::Size kMinSize(25, 25);
constexpr gfx::Size kMaxSize(800, 600);

}  // namespace

// static.
base::WeakPtr<WebUIBubbleDialogView>
WebUIBubbleDialogView::CreateWebUIBubbleDialog(
    std::unique_ptr<WebUIBubbleDialogView> bubble_view) {
  auto weak_ptr = bubble_view->weak_factory_.GetWeakPtr();
  BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  return weak_ptr;
}

WebUIBubbleDialogView::WebUIBubbleDialogView(
    views::View* anchor_view,
    std::unique_ptr<WebUIBubbleView> web_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      web_view_(AddChildView(std::move(web_view))) {
  web_view_->set_host(this);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(gfx::Insets());
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

WebUIBubbleDialogView::~WebUIBubbleDialogView() = default;

std::unique_ptr<WebUIBubbleView> WebUIBubbleDialogView::RemoveWebView() {
  DCHECK(web_view_);
  auto* web_view = web_view_;
  web_view_ = nullptr;
  web_view->set_host(nullptr);
  return RemoveChildViewT(web_view);
}

gfx::Size WebUIBubbleDialogView::CalculatePreferredSize() const {
  // Constrain the size to popup min/max.
  gfx::Size preferred_size = BubbleDialogDelegateView::CalculatePreferredSize();
  preferred_size.SetToMax(kMinSize);
  preferred_size.SetToMin(kMaxSize);
  return preferred_size;
}

void WebUIBubbleDialogView::AddedToWidget() {
  BubbleDialogDelegateView::AddedToWidget();
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(GetCornerRadius()));
}

void WebUIBubbleDialogView::ShowUI() {
  SetVisible(true);
  DCHECK(GetWidget());
  GetWidget()->Show();
  web_view_->GetWebContents()->Focus();
}

void WebUIBubbleDialogView::CloseUI() {
  DCHECK(GetWidget());
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void WebUIBubbleDialogView::OnWebViewSizeChanged() {
  SizeToContents();
}
