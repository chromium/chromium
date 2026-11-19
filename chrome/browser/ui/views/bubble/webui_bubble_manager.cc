// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_warmup_level_recorder.h"
#include "content/public/browser/render_process_host.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
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
  // destruction to mitigate the risk of UAFs (see crbug.com/40060325).
  close_bubble_helper_.reset();
  bubble_widget_.reset();
  closing_widgets_.clear();
}

bool WebUIBubbleManager::ShowBubble(Anchor anchor,
                                    views::BubbleBorder::Arrow arrow,
                                    ui::ElementIdentifier identifier) {
  if (bubble_view_) {
    return false;
  }

  cache_timer_->Stop();

  WebUIContentsWarmupLevelRecorder warmup_level_recorder;
  warmup_level_recorder.BeforeContentsCreation();
  auto bubble_view = CreateWebUIBubbleDialog(anchor, arrow);
  bubble_view_ = bubble_view->GetWeakPtr();
  bubble_widget_ = views::BubbleDialogDelegate::CreateBubble(
      std::move(bubble_view).release(),
      base::BindOnce(&WebUIBubbleManager::OnBubbleWidgetClosed,
                     weak_factory_.GetWeakPtr()));
  warmup_level_recorder.AfterContentsCreation(
      bubble_view_->web_view()->GetWebContents());
  warmup_level_recorder.SetUsedCachedContents(
      bubble_using_cached_web_contents_);
  contents_warmup_level_ = warmup_level_recorder.GetWarmupLevel();

  observers_.Notify(&WebUIBubbleManagerObserver::BeforeBubbleWidgetShowed,
                    bubble_widget_.get());

  // Some bubbles can be triggered when there is no active browser (e.g. emoji
  // picker in Chrome OS launcher). In that case, the close bubble helper isn't
  // needed.
  BrowserWindowInterface* const last_active_browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if ((!disable_close_bubble_helper_) && last_active_browser) {
    close_bubble_helper_ = std::make_unique<CloseBubbleOnTabActivationHelper>(
        bubble_view_.get(), last_active_browser->GetTabStripModel());
  }

  if (identifier) {
    bubble_view_->SetProperty(views::kElementIdentifierKey, identifier);
  }

  if (GetContentsWrapper()->is_ready_to_show()) {
    GetContentsWrapper()->ShowUI();
  }

  return true;
}

void WebUIBubbleManager::CloseBubble() {
  if (!bubble_widget_) {
    return;
  }
  bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
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

void WebUIBubbleManager::ResetContentsWrapperForTesting() {
  ResetContentsWrapper();
}

void WebUIBubbleManager::DisableCloseBubbleHelperForTesting() {
  disable_close_bubble_helper_ = true;
}

void WebUIBubbleManager::ResetContentsWrapper() {
  if (!cached_contents_wrapper_) {
    return;
  }

  if (bubble_widget_) {
    CloseBubble();
  }
  CHECK(!cached_contents_wrapper_->GetHost(), base::NotFatalUntil::M162);
  cached_contents_wrapper_.reset();
}

void WebUIBubbleManager::OnBubbleWidgetClosed(
    views::Widget::ClosedReason reason) {
  if (!bubble_widget_) {
    return;
  }

  if (bubble_view_) {
    bubble_view_->ClearContentsWrapper();
  }

  close_bubble_helper_.reset();
  cache_timer_->Reset();
  bubble_using_cached_web_contents_ = false;

  if (!bubble_widget_->IsClosed()) {
    bubble_widget_->CloseWithReason(reason);
  }

  closing_widgets_.push_back(std::move(bubble_widget_));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebUIBubbleManager::DestroyClosingWidgets,
                                weak_factory_.GetWeakPtr()));
}

void WebUIBubbleManager::DestroyClosingWidgets() {
  closing_widgets_.clear();
}
