// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::TimeDelta kWebViewRetentionTime =
    base::TimeDelta::FromSeconds(30);

}  // namespace

WebUIBubbleManagerBase::WebUIBubbleManagerBase(
    views::View* anchor_view,
    content::BrowserContext* browser_context,
    const GURL& webui_url,
    bool enable_extension_apis)
    : anchor_view_(anchor_view),
      browser_context_(browser_context),
      webui_url_(webui_url),
      enable_extension_apis_(enable_extension_apis),
      cache_timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          kWebViewRetentionTime,
          base::BindRepeating(&WebUIBubbleManagerBase::ResetWebView,
                              base::Unretained(this)))) {}

WebUIBubbleManagerBase::~WebUIBubbleManagerBase() = default;

bool WebUIBubbleManagerBase::ShowBubble() {
  if (bubble_view_)
    return false;

  cache_timer_->Stop();

  if (cached_web_view_) {
    cached_web_view_->GetWebContents()->ReloadFocusedFrame();
  } else {
    cached_web_view_ = CreateWebView();
  }

  bubble_view_ = WebUIBubbleDialogView::CreateWebUIBubbleDialog(
      std::make_unique<WebUIBubbleDialogView>(anchor_view_,
                                              std::move(cached_web_view_)));
  observed_bubble_widget_.Add(bubble_view_->GetWidget());
  return true;
}

void WebUIBubbleManagerBase::CloseBubble() {
  if (!bubble_view_)
    return;
  DCHECK(bubble_view_->GetWidget());
  bubble_view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
}

views::Widget* WebUIBubbleManagerBase::GetBubbleWidget() const {
  return bubble_view_ ? bubble_view_->GetWidget() : nullptr;
}

void WebUIBubbleManagerBase::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(bubble_view_);
  DCHECK_EQ(bubble_view_->GetWidget(), widget);
  cached_web_view_ = bubble_view_->RemoveWebView();
  observed_bubble_widget_.Remove(bubble_view_->GetWidget());
  cache_timer_->Reset();
}

void WebUIBubbleManagerBase::ResetWebViewForTesting() {
  ResetWebView();
}

void WebUIBubbleManagerBase::ResetWebView() {
  cached_web_view_.reset();
}
