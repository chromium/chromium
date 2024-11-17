// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_control/browser/media_blocker.h"

#include <utility>

#include "components/media_control/mojom/media_playback_options.mojom.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace media_control {

MediaBlocker::MediaBlocker(content::WebContents* web_contents) {
  content::WebContentsObserver::Observe(web_contents);
}

MediaBlocker::~MediaBlocker() = default;

void MediaBlocker::BlockMediaLoading(bool blocked) {
  if (media_loading_blocked_ == blocked)
    return;

  media_loading_blocked_ = blocked;
  UpdateMediaLoadingBlockedState();

  OnBlockMediaLoadingChanged();
}

void MediaBlocker::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  UpdateRenderFrameMediaLoadingBlockedState(render_frame_host);

  OnRenderFrameCreated(render_frame_host);
}

void MediaBlocker::RenderViewReady() {
  UpdateMediaLoadingBlockedState();
}

void MediaBlocker::UpdateMediaLoadingBlockedState() {
  if (!web_contents())
    return;

  web_contents()->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsRenderFrameLive()) {
          UpdateRenderFrameMediaLoadingBlockedState(render_frame_host);
        }
      });
}

void MediaBlocker::UpdateRenderFrameMediaLoadingBlockedState(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  mojo::AssociatedRemote<components::media_control::mojom::MediaPlaybackOptions>
      media_playback_options;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &media_playback_options);
  media_playback_options->SetMediaLoadingBlocked(media_loading_blocked_);
}

}  // namespace media_control
