// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/browser/script_tools_ready_waiter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace page_content_annotations {

// static
bool ScriptToolsReadyWaiter::AreScriptToolsReady(
    content::WebContents& web_contents,
    bool include_ad_frames) {
  bool ready = true;
  web_contents.GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
      [&](content::RenderFrameHost* rfh) {
        if (!include_ad_frames && rfh->IsAdFrame()) {
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        }
        switch (rfh->GetScriptToolsReadyState()) {
          case content::RenderFrameHost::ScriptToolsReadyState::kReady:
          case content::RenderFrameHost::ScriptToolsReadyState::kNoRenderer:
            return content::RenderFrameHost::FrameIterationAction::kContinue;
          case content::RenderFrameHost::ScriptToolsReadyState::kLoading:
          case content::RenderFrameHost::ScriptToolsReadyState::kWaiting:
            ready = false;
            return content::RenderFrameHost::FrameIterationAction::kStop;
        }
      });
  return ready;
}

ScriptToolsReadyWaiter::ScriptToolsReadyWaiter(
    content::WebContents* web_contents,
    Callback callback,
    bool include_ad_frames)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback)),
      include_ad_frames_(include_ad_frames) {
  ScheduleCheck();
}

ScriptToolsReadyWaiter::~ScriptToolsReadyWaiter() = default;

void ScriptToolsReadyWaiter::ScriptToolsReadyStateChanged(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetPage().IsPrimary()) {
    ScheduleCheck();
  }
}

void ScriptToolsReadyWaiter::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetPage().IsPrimary()) {
    ScheduleCheck();
  }
}

void ScriptToolsReadyWaiter::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetPage().IsPrimary()) {
    ScheduleCheck();
  }
}

void ScriptToolsReadyWaiter::PrimaryPageChanged(content::Page& page) {
  Finish(Result::kPrimaryPageChanged);
}

void ScriptToolsReadyWaiter::WebContentsDestroyed() {
  Finish(Result::kPrimaryPageChanged);
}

void ScriptToolsReadyWaiter::ScheduleCheck() {
  if (check_scheduled_) {
    return;
  }
  check_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ScriptToolsReadyWaiter::Check,
                                weak_factory_.GetWeakPtr()));
}

void ScriptToolsReadyWaiter::Check() {
  check_scheduled_ = false;
  if (web_contents() &&
      AreScriptToolsReady(*web_contents(), include_ad_frames_)) {
    Finish(Result::kReady);
  }
}

void ScriptToolsReadyWaiter::Finish(Result result) {
  if (!callback_) {
    return;
  }
  weak_factory_.InvalidateWeakPtrs();
  Observe(nullptr);
  std::move(callback_).Run(result);
}

}  // namespace page_content_annotations
