// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_BROWSER_SCRIPT_TOOLS_READY_WAITER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_BROWSER_SCRIPT_TOOLS_READY_WAITER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace page_content_annotations {

// Waits until the script tools of every frame in a WebContents' primary page
// are ready.
class ScriptToolsReadyWaiter : public content::WebContentsObserver {
 public:
  enum class Result {
    kReady,
    // The primary page changed before its tools were ready.
    kPrimaryPageChanged,
  };
  using Callback = base::OnceCallback<void(Result)>;

  // Returns whether the script tools of every frame in `web_contents`' primary
  // page are ready.
  static bool AreScriptToolsReady(content::WebContents& web_contents,
                                  bool include_ad_frames);

  // Runs `callback` once the script tools are ready or the primary page
  // changes. Callback is always dispatched async and may delete `this`.
  ScriptToolsReadyWaiter(content::WebContents* web_contents,
                         Callback callback,
                         bool include_ad_frames);
  ScriptToolsReadyWaiter(const ScriptToolsReadyWaiter&) = delete;
  ScriptToolsReadyWaiter& operator=(const ScriptToolsReadyWaiter&) = delete;
  ~ScriptToolsReadyWaiter() override;

 private:
  // content::WebContentsObserver:
  void ScriptToolsReadyStateChanged(
      content::RenderFrameHost* render_frame_host) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

  // Checks whether the tools are ready, in a separate task, since frames are
  // added and removed in the middle of updating the frame tree.
  void ScheduleCheck();
  void Check();
  void Finish(Result result);

  Callback callback_;
  const bool include_ad_frames_;
  bool check_scheduled_ = false;
  base::WeakPtrFactory<ScriptToolsReadyWaiter> weak_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_BROWSER_SCRIPT_TOOLS_READY_WAITER_H_
