// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/notimplemented.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_list.h"
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

  bubble_init_start_time_ = base::TimeTicks::Now();

  const content::RenderProcessHost* spare_render_process_host =
      content::RenderProcessHost::GetSpareRenderProcessHost();
  bubble_warmup_level_ = WebUIBubbleWarmUpLevel::kNoRenderer;

  bubble_view_ = CreateWebUIBubbleDialog(anchor, arrow);

  content::RenderProcessHost* render_process_host = bubble_view_->web_view()
                                                        ->GetWebContents()
                                                        ->GetPrimaryMainFrame()
                                                        ->GetProcess();
  const bool bubble_using_spare_render_process =
      render_process_host == spare_render_process_host;

  size_t top_chrome_frames = 0;
  render_process_host->ForEachRenderFrameHost(
      [&top_chrome_frames](content::RenderFrameHost* rfh) {
        top_chrome_frames +=
            base::EndsWith(rfh->GetSiteInstance()->GetSiteURL().host_piece(),
                           chrome::kChromeUITopChromeDomain);
      });
  const bool bubble_reused_render_process = top_chrome_frames > 1;

  if (bubble_using_cached_web_contents_) {
    bubble_warmup_level_ = WebUIBubbleWarmUpLevel::kNavigatedWebContents;
  } else if (bubble_using_spare_render_process) {
    bubble_warmup_level_ = WebUIBubbleWarmUpLevel::kSpareRenderer;
  } else if (bubble_reused_render_process) {
    bubble_warmup_level_ = WebUIBubbleWarmUpLevel::kDedicatedRenderer;
  }

  bubble_widget_observation_.Observe(bubble_view_->GetWidget());
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

std::string ToString(WebUIBubbleWarmUpLevel warmup_level) {
  switch (warmup_level) {
    case WebUIBubbleWarmUpLevel::kNoRenderer:
      return "NoRenderer";
    case WebUIBubbleWarmUpLevel::kSpareRenderer:
      return "SpareRenderer";
    case WebUIBubbleWarmUpLevel::kDedicatedRenderer:
      return "DedicatedRenderer";
    case WebUIBubbleWarmUpLevel::kRedirectedWebContents:
      return "RedirectedWebContents";
    case WebUIBubbleWarmUpLevel::kNavigatedWebContents:
      return "NavigatedWebContents";
    default:
      NOTIMPLEMENTED();
      return "";
  }
}
