// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_CONTROL_BROWSER_MEDIA_BLOCKER_H_
#define COMPONENTS_MEDIA_CONTROL_BROWSER_MEDIA_BLOCKER_H_

#include "content/public/browser/web_contents_observer.h"

namespace media_control {

// This class implements a blocking mode for web applications. Media is
// unblocked by default.
// This is currently used in Chromecast internal code.
// TODO(crbug.com/40120884): Add comment about Fuchsia with implementation.
class MediaBlocker : public content::WebContentsObserver {
 public:
  // Observes WebContents.
  explicit MediaBlocker(content::WebContents* web_contents);

  ~MediaBlocker() override;

  MediaBlocker(const MediaBlocker&) = delete;
  MediaBlocker& operator=(const MediaBlocker&) = delete;

  // Sets if the web contents is allowed to load and play media or not.
  // If media is unblocked, previously suspended elements should begin playing
  // again.
  void BlockMediaLoading(bool blocked);

  bool media_loading_blocked() const { return media_loading_blocked_; }

 private:
  // content::WebContentsObserver implementation:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) final;
  void RenderViewReady() final;

  // Blocks or unblocks the render process from loading new media
  // according to |media_loading_blocked_|.
  void UpdateMediaLoadingBlockedState();
  void UpdateRenderFrameMediaLoadingBlockedState(
      content::RenderFrameHost* render_frame_host);

  // Subclasses can override this function if additional handling for
  // BlockMediaLoading is needed when the blocked state changes.
  virtual void OnBlockMediaLoadingChanged() {}

  // Subclasses can override this function if additional handling for
  // RenderFrameCreated is needed.
  virtual void OnRenderFrameCreated(
      content::RenderFrameHost* render_frame_host) {}

  // Whether or not media loading should be blocked. This value cache's the last
  // call to BlockMediaLoading. Is false by default.
  bool media_loading_blocked_ = false;
};

}  // namespace media_control

#endif  // COMPONENTS_MEDIA_CONTROL_BROWSER_MEDIA_BLOCKER_H_
