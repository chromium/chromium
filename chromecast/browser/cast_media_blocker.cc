// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_media_blocker.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/browser/cast_renderer_block_data.h"
#include "components/media_control/mojom/media_playback_options.mojom.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace chromecast {

CastMediaBlocker::CastMediaBlocker(content::WebContents* web_contents)
    : media_control::MediaBlocker(web_contents),
      media_session_(content::MediaSession::Get(web_contents)) {
  media_session_->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
}

CastMediaBlocker::~CastMediaBlocker() = default;

void CastMediaBlocker::OnBlockMediaLoadingChanged() {
  UpdatePlayingState();
}

void CastMediaBlocker::BlockMediaStarting(bool blocked) {
  if (media_starting_blocked_ == blocked)
    return;

  media_starting_blocked_ = blocked;

  shell::CastRendererBlockData::SetRendererBlockForWebContents(
      web_contents(), media_starting_blocked_);

  UpdatePlayingState();
}

void CastMediaBlocker::UpdatePlayingState() {
  LOG(INFO) << __FUNCTION__
            << " media_loading_blocked=" << media_loading_blocked()
            << " media_starting_blocked=" << media_starting_blocked_
            << " suspended=" << suspended_ << " controllable=" << controllable_
            << " paused_by_user=" << paused_by_user_;

  // If blocking media, suspend if possible.
  if (PlayingBlocked()) {
    if (!suspended_ && controllable_) {
      Suspend();
    }
    return;
  }

  // If unblocking media, resume if media was not paused by user.
  if (!paused_by_user_ && suspended_ && controllable_) {
    paused_by_user_ = true;
    Resume();
  }
}

void CastMediaBlocker::EnableBackgroundVideoPlayback(bool enabled) {
  if (!web_contents())
    return;

  background_video_playback_enabled_ = enabled;
  UpdateBackgroundVideoPlaybackState();
}

bool CastMediaBlocker::PlayingBlocked() const {
  return (media_loading_blocked() || media_starting_blocked_);
}

void CastMediaBlocker::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  bool is_suspended = session_info->playback_state ==
                      media_session::mojom::MediaPlaybackState::kPaused;

  LOG(INFO) << __FUNCTION__
            << " media_loading_blocked=" << media_loading_blocked()
            << " media_starting_blocked=" << media_starting_blocked_
            << " is_suspended=" << is_suspended
            << " is_controllable=" << session_info->is_controllable
            << " paused_by_user=" << paused_by_user_;

  // Process controllability first.
  if (controllable_ != session_info->is_controllable) {
    controllable_ = session_info->is_controllable;

    // If not blocked, and we regain control and the media wasn't paused when
    // blocked, resume media if suspended.
    if (!PlayingBlocked() && !paused_by_user_ && is_suspended &&
        controllable_) {
      paused_by_user_ = true;
      Resume();
    }

    // Suspend if blocked and the session becomes controllable.
    if (PlayingBlocked() && !is_suspended && controllable_) {
      // Only suspend if suspended_ doesn't change. Otherwise, this will be
      // handled in the suspended changed block.
      if (suspended_ == is_suspended)
        Suspend();
    }
  }

  // TODO(crbug.com/40120884): Rename suspended to paused to be consistent with
  // MediaSession types.
  // Process suspended state next.
  if (suspended_ != is_suspended) {
    suspended_ = is_suspended;
    // If blocking, suspend media whenever possible.
    if (PlayingBlocked() && !suspended_) {
      // If media was resumed when blocked, the user tried to play music.
      paused_by_user_ = false;
      if (controllable_)
        Suspend();
    }

    // If not blocking, cache the user's play intent.
    if (!PlayingBlocked())
      paused_by_user_ = suspended_;
  }
}

void CastMediaBlocker::Suspend() {
  if (!media_session_)
    return;

  LOG(INFO) << "Suspending media session.";
  media_session_->Suspend(content::MediaSession::SuspendType::kSystem);
}

void CastMediaBlocker::Resume() {
  if (!media_session_)
    return;

  LOG(INFO) << "Resuming media session.";
  media_session_->Resume(content::MediaSession::SuspendType::kSystem);
}

void CastMediaBlocker::OnRenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  UpdateRenderFrameBackgroundVideoPlaybackState(render_frame_host);
}

void CastMediaBlocker::UpdateBackgroundVideoPlaybackState() {
  if (!web_contents())
    return;
  web_contents()->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* frame) {
        UpdateRenderFrameBackgroundVideoPlaybackState(frame);
      });
}

void CastMediaBlocker::UpdateRenderFrameBackgroundVideoPlaybackState(
    content::RenderFrameHost* frame) {
  mojo::AssociatedRemote<components::media_control::mojom::MediaPlaybackOptions>
      media_playback_options;
  frame->GetRemoteAssociatedInterfaces()->GetInterface(&media_playback_options);
  media_playback_options->SetBackgroundVideoPlaybackEnabled(
      background_video_playback_enabled_);
}

void CastMediaBlocker::SetMediaSessionForTesting(
    content::MediaSession* media_session) {
  media_session_ = media_session;
}

}  // namespace chromecast
