// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_list.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::TimeDelta kWebViewRetentionTime =
    base::TimeDelta::FromSeconds(30);

}  // namespace

WebUIBubbleManager::WebUIBubbleManager()
    : cache_timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          kWebViewRetentionTime,
          base::BindRepeating(&WebUIBubbleManager::ResetContentsWrapper,
                              base::Unretained(this)))) {}

WebUIBubbleManager::~WebUIBubbleManager() = default;

bool WebUIBubbleManager::ShowBubble() {
  if (bubble_view_)
    return false;

  cache_timer_->Stop();

  bubble_view_ = CreateWebUIBubbleDialog();

  bubble_widget_observation_.Observe(bubble_view_->GetWidget());
  if (!disable_close_bubble_helper_) {
    close_bubble_helper_ = std::make_unique<CloseBubbleOnTabActivationHelper>(
        bubble_view_.get(), BrowserList::GetInstance()->GetLastActive());
  }
  return true;
}

void WebUIBubbleManager::CloseBubble() {
  if (!bubble_view_)
    return;
  DCHECK(bubble_view_->GetWidget());
  bubble_view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
}

views::Widget* WebUIBubbleManager::GetBubbleWidget() const {
  return bubble_view_ ? bubble_view_->GetWidget() : nullptr;
}

void WebUIBubbleManager::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(bubble_view_);
  DCHECK_EQ(bubble_view_->GetWidget(), widget);
  DCHECK(bubble_widget_observation_.IsObserving());
  bubble_widget_observation_.Reset();
  close_bubble_helper_.reset();
  cache_timer_->Reset();
  bubble_using_cached_web_contents_ = false;
}

void WebUIBubbleManager::ResetContentsWrapperForTesting() {
  ResetContentsWrapper();
}

void WebUIBubbleManager::ResetContentsWrapper() {
  if (!cached_contents_wrapper_)
    return;

  if (bubble_view_)
    CloseBubble();
  DCHECK(!cached_contents_wrapper_->GetHost());
  cached_contents_wrapper_.reset();
}

void WebUIBubbleManager::DisableCloseBubbleHelperForTesting() {
  disable_close_bubble_helper_ = true;
}
