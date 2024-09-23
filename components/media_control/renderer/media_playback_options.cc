// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_control/renderer/media_playback_options.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "components/media_control/renderer/media_control_buildflags.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

#if BUILDFLAG(ENABLE_MEDIA_CONTROL_LOGGING_OVERRIDE)
#if !defined(DVLOG)
#error This file must be included after base/logging.h.
#endif
#undef DVLOG
#define DVLOG(verboselevel) LOG(INFO)
#endif  // BUILDFLAG(ENABLE_MEDIA_CONTROL_LOGGING_OVERRIDE)

namespace media_control {

MediaPlaybackOptions::MediaPlaybackOptions(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<MediaPlaybackOptions>(render_frame),
      render_frame_action_blocked_(false) {
  // TODO(crbug.com/40120884): Extract to callers and remove
  // renderer_media_playback_options_.
  // Override default content MediaPlaybackOptions
  renderer_media_playback_options_
      .is_background_video_track_optimization_supported = false;
  render_frame->SetRenderFrameMediaPlaybackOptions(
      renderer_media_playback_options_);

  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<components::media_control::mojom::MediaPlaybackOptions>(
          base::BindRepeating(
              &MediaPlaybackOptions::OnMediaPlaybackOptionsAssociatedReceiver,
              base::Unretained(this)));
}

MediaPlaybackOptions::~MediaPlaybackOptions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MediaPlaybackOptions::OnDestruct() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete this;
}

bool MediaPlaybackOptions::RunWhenInForeground(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!render_frame_action_blocked_) {
    std::move(closure).Run();
    return false;
  }

  DVLOG(1) << "A render frame action is being blocked.";
  pending_closures_.push_back(std::move(closure));
  return true;
}

void MediaPlaybackOptions::SetMediaLoadingBlocked(bool blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  render_frame_action_blocked_ = blocked;
  if (blocked) {
    DVLOG(1) << "Render frame actions are blocked.";
    return;
  }
  // Move callbacks in case OnBlockMediaLoading() is called somehow
  // during iteration.
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(pending_closures_);
  for (auto& cb : callbacks) {
    std::move(cb).Run();
  }
  DVLOG(1) << "Render frame actions are unblocked.";
}

void MediaPlaybackOptions::SetBackgroundVideoPlaybackEnabled(bool enabled) {
  renderer_media_playback_options_.is_background_video_playback_enabled =
      enabled;
  render_frame()->SetRenderFrameMediaPlaybackOptions(
      renderer_media_playback_options_);
}

void MediaPlaybackOptions::SetRendererType(content::mojom::RendererType type) {
  renderer_media_playback_options_.renderer_type = type;
  render_frame()->SetRenderFrameMediaPlaybackOptions(
      renderer_media_playback_options_);
}

void MediaPlaybackOptions::OnMediaPlaybackOptionsAssociatedReceiver(
    mojo::PendingAssociatedReceiver<
        components::media_control::mojom::MediaPlaybackOptions> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace media_control
