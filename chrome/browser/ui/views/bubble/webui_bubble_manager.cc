// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/notimplemented.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_warmup_level_recorder.h"
#include "chrome/browser/ui/webui/top_chrome/webui_url_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::TimeDelta kWebViewRetentionTime = base::Seconds(30);

}  // namespace

WebUIBubbleManager::WebUIBubbleManager()
    : cache_timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          kWebViewRetentionTime,
          base::BindRepeating(&WebUIBubbleManager::ResetContentsWrapper,
                              base::Unretained(this)))) {}

WebUIBubbleManager::~WebUIBubbleManager() {
  // The bubble manager may be destroyed before the bubble in certain
  // situations. Ensure we forcefully close the managed bubble during
  // destruction to mitigate the risk of UAFs (see crbug.com/1345546).
  if (bubble_view_) {
    DCHECK(bubble_view_->GetWidget());
    bubble_view_->GetWidget()->CloseNow();
  }
}

bool WebUIBubbleManager::ShowBubble(const std::optional<gfx::Rect>& anchor,
                                    views::BubbleBorder::Arrow arrow,
                                    ui::ElementIdentifier identifier) {
  if (bubble_view_)
    return false;

  cache_timer_->Stop();

  WebUIContentsWarmupLevelRecorder warmup_level_recorder;
  warmup_level_recorder.BeforeContentsCreation();
  bubble_view_ = CreateWebUIBubbleDialog(anchor, arrow);
  warmup_level_recorder.AfterContentsCreation(
      bubble_view_->web_view()->GetWebContents());
  warmup_level_recorder.SetUsedCachedContents(
      bubble_using_cached_web_contents_);
  contents_warmup_level_ = warmup_level_recorder.GetWarmupLevel();

  bubble_widget_observation_.Observe(bubble_view_->GetWidget());

  observers_.Notify(&WebUIBubbleManagerObserver::BeforeBubbleWidgetShowed,
                    bubble_view_->GetWidget());

  // Some bubbles can be triggered when there is no active browser (e.g. emoji
  // picker in Chrome OS launcher). In that case, the close bubble helper isn't
  // needed.
  if ((!disable_close_bubble_helper_) &&
      BrowserList::GetInstance()->GetLastActive()) {
    close_bubble_helper_ = std::make_unique<CloseBubbleOnTabActivationHelper>(
        bubble_view_.get(), BrowserList::GetInstance()->GetLastActive());
  }

  if (identifier)
    bubble_view_->SetProperty(views::kElementIdentifierKey, identifier);

  if (GetContentsWrapper()->is_ready_to_show()) {
    GetContentsWrapper()->ShowUI();
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

void WebUIBubbleManager::AddObserver(WebUIBubbleManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WebUIBubbleManager::RemoveObserver(WebUIBubbleManagerObserver* observer) {
  observers_.RemoveObserver(observer);
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
