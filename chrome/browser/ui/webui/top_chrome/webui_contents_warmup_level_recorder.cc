// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_warmup_level_recorder.h"

#include "base/containers/contains.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_warmup_level.h"
#include "chrome/browser/ui/webui/top_chrome/webui_url_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/web_contents.h"

namespace {

// Returns true if `web_contents` was the first Top Chrome WebContents
// created on its render process. This does not consider preloaded contents.
bool IsFirstWebContentsOnProcess(
    const WebUIContentsWarmupLevelPreCondition& pre_condition,
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

}  // namespace

WebUIContentsWarmupLevelPreCondition::WebUIContentsWarmupLevelPreCondition() =
    default;
WebUIContentsWarmupLevelPreCondition&
WebUIContentsWarmupLevelPreCondition::operator=(
    WebUIContentsWarmupLevelPreCondition&&) = default;
WebUIContentsWarmupLevelPreCondition::~WebUIContentsWarmupLevelPreCondition() =
    default;

WebUIContentsWarmupLevelRecorder::WebUIContentsWarmupLevelRecorder() = default;
WebUIContentsWarmupLevelRecorder::~WebUIContentsWarmupLevelRecorder() = default;

void WebUIContentsWarmupLevelRecorder::BeforeContentsCreation() {
  pre_condition_.emplace();
  pre_condition_->spare_process_ids =
      content::SpareRenderProcessHostManager::Get().GetSpareIds();
  if (content::WebContents* preloaded_contents =
          WebUIContentsPreloadManager::GetInstance()
              ->preloaded_web_contents()) {
    pre_condition_->preloaded_contents = preloaded_contents->GetWeakPtr();
    pre_condition_->preloaded_process =
        preloaded_contents->GetPrimaryMainFrame()->GetProcess();
    pre_condition_->preloaded_host = preloaded_contents->GetURL().host();
  }
}

void WebUIContentsWarmupLevelRecorder::AfterContentsCreation(
    content::WebContents* web_contents) {
  CHECK(pre_condition_) << "You must call BeforeContentsCreation()";
  CHECK(web_contents);

  if (base::Contains(
          pre_condition_->spare_process_ids,
          web_contents->GetPrimaryMainFrame()->GetProcess()->GetID())) {
    level_ = WebUIContentsWarmupLevel::kSpareRenderer;
    return;
  }

  if (pre_condition_->preloaded_contents.get() == web_contents) {
    level_ = pre_condition_->preloaded_host == web_contents->GetURL().host()
                 ? WebUIContentsWarmupLevel::kPreloadedWebContents
                 : WebUIContentsWarmupLevel::kRedirectedWebContents;
    return;
  }

  level_ = IsFirstWebContentsOnProcess(*pre_condition_, web_contents)
               ? WebUIContentsWarmupLevel::kNoRenderer
               : WebUIContentsWarmupLevel::kDedicatedRenderer;
}

void WebUIContentsWarmupLevelRecorder::SetUsedCachedContents(
    bool used_cached_contents) {
  if (used_cached_contents) {
    level_ = WebUIContentsWarmupLevel::kReshowingWebContents;
  }
}

WebUIContentsWarmupLevel WebUIContentsWarmupLevelRecorder::GetWarmupLevel()
    const {
  CHECK(pre_condition_) << "You must call BeforeContentsCreation()";
  CHECK(level_) << "You must call AfterContentsCreation()";
  return *level_;
}
