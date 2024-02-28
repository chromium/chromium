// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/notimplemented.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
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

// This struct captures the state immediately before a WebUI bubble is created.
// TODO(326490753): move warm-up level computation to cbui/webui/top_chrome.
struct WebUIBubbleWarmupLevelPreCondition {
  // The spare process before the contents is created. This can be nullptr
  // when the memory pressure is high or the spare process creation is delayed.
  raw_ptr<const content::RenderProcessHost> spare_process = nullptr;

  // Use WeakPtr because the preloaded contents could be destroyed, e.g. after
  // a request of contents under a different browser context.
  base::WeakPtr<content::WebContents> preloaded_contents;

  // The RenderProcessHost of `preloaded_contents`, if exists.
  raw_ptr<const content::RenderProcessHost> preloaded_process = nullptr;

  // The preloaded WebUI's host URL, if exists.
  std::string preloaded_host;
};

WebUIBubbleWarmupLevelPreCondition GetWebUIBubbleWarmupLevelPreCondition(
    bool using_cached_contents) {
  WebUIBubbleWarmupLevelPreCondition pre_condition;
  pre_condition.spare_process =
      content::RenderProcessHost::GetSpareRenderProcessHost();
  if (content::WebContents* preloaded_contents =
          WebUIContentsPreloadManager::GetInstance()
              ->preloaded_web_contents()) {
    pre_condition.preloaded_contents = preloaded_contents->GetWeakPtr();
    pre_condition.preloaded_process =
        preloaded_contents->GetPrimaryMainFrame()->GetProcess();
    pre_condition.preloaded_host = preloaded_contents->GetURL().host();
  }
  return pre_condition;
}

// Returns true if `web_contents` was the first Top Chrome WebContents
// created on its render process. This does not consider preloaded contents.
bool IsFirstWebContentsOnProcess(
    const WebUIBubbleWarmupLevelPreCondition& pre_condition,
    content::WebContents* web_contents) {
  if (pre_condition.preloaded_process ==
      web_contents->GetPrimaryMainFrame()->GetProcess()) {
    return false;
  }

  // Count the number of top-level Top Chrome documents on the process.
  size_t top_chrome_frames = 0;
  web_contents->GetPrimaryMainFrame()->GetProcess()->ForEachRenderFrameHost(
      [&top_chrome_frames](content::RenderFrameHost* rfh) {
        top_chrome_frames +=
            rfh->GetOutermostMainFrame() == rfh &&
            IsTopChromeWebUIURL(rfh->GetSiteInstance()->GetSiteURL());
      });
  const size_t has_preloaded_contents =
      WebUIContentsPreloadManager::GetInstance()->preloaded_web_contents() ? 1
                                                                           : 0;
  return top_chrome_frames == 1 + has_preloaded_contents;
}

WebUIBubbleWarmUpLevel ComputeWebUIBubbleWarmupLevel(
    const WebUIBubbleWarmupLevelPreCondition& pre_condition,
    bool using_cached_contents,
    content::WebContents* web_contents) {
  CHECK(web_contents);
  if (using_cached_contents) {
    return WebUIBubbleWarmUpLevel::kReshowingWebContents;
  }

  if (pre_condition.spare_process ==
      web_contents->GetPrimaryMainFrame()->GetProcess()) {
    return WebUIBubbleWarmUpLevel::kSpareRenderer;
  }

  if (pre_condition.preloaded_contents.get() == web_contents) {
    return pre_condition.preloaded_host == web_contents->GetURL().host()
               ? WebUIBubbleWarmUpLevel::kPreloadedWebContents
               : WebUIBubbleWarmUpLevel::kRedirectedWebContents;
  }

  return IsFirstWebContentsOnProcess(pre_condition, web_contents)
             ? WebUIBubbleWarmUpLevel::kNoRenderer
             : WebUIBubbleWarmUpLevel::kDedicatedRenderer;
}

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

  const WebUIBubbleWarmupLevelPreCondition warmup_level_pre_condition =
      GetWebUIBubbleWarmupLevelPreCondition(bubble_using_cached_web_contents_);
  bubble_view_ = CreateWebUIBubbleDialog(anchor, arrow);
  bubble_warmup_level_ = ComputeWebUIBubbleWarmupLevel(
      warmup_level_pre_condition, bubble_using_cached_web_contents_,
      bubble_view_->web_view()->GetWebContents());

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
    case WebUIBubbleWarmUpLevel::kPreloadedWebContents:
      return "PreloadedWebContents";
    case WebUIBubbleWarmUpLevel::kReshowingWebContents:
      return "ReshowingWebContents";
    default:
      NOTIMPLEMENTED();
      return "";
  }
}
