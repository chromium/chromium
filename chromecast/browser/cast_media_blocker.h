// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_
#define CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_

#include <vector>

#include "components/media_control/browser/media_blocker.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {
class MediaSession;
}  // namespace content

namespace chromecast {

namespace shell {
class CastMediaBlockerTest;
}  // namespace shell

// This class implements a blocking mode for web applications and is used in
// Chromecast internal code. Media is unblocked by default.
class CastMediaBlocker : public media_control::MediaBlocker,
                         public media_session::mojom::MediaSessionObserver {
 public:
  // Observes WebContents and the associated MediaSession.
  explicit CastMediaBlocker(content::WebContents* web_contents);

  ~CastMediaBlocker() override;

  CastMediaBlocker(const CastMediaBlocker&) = delete;
  CastMediaBlocker& operator=(const CastMediaBlocker&) = delete;

  // Called when there's a change in whether or not web contents is allowed to
  // load and play media.
  // If media is unblocked, previously suspended elements should begin playing
  // again. Media is unblocked when both MediaLoading and MediaStarting blocks
  // are off.
  void OnBlockMediaLoadingChanged() override;
  // Sets if the web contents is allowed to play media or not. If media is
  // unblocked, previously suspended elements should begin playing again.  Media
  // is unblocked when both MediaLoading and MediaStarting blocks are off.
  // This is a more relaxed block than BlockMediaLoading since the block doesn't
  // block media from loading but it does block media from starting.
  void BlockMediaStarting(bool blocked);
  void EnableBackgroundVideoPlayback(bool enabled);

  // media_session::mojom::MediaSessionObserver implementation:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override {}
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override {}
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      override {}
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override {}

 private:
  friend shell::CastMediaBlockerTest;
  // components::media_control::MediaBlocker implementation:
  void OnRenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override;

  // Suspends or resumes the media session for the web contents.
  void Suspend();
  void Resume();

  void UpdatePlayingState();

  void UpdateBackgroundVideoPlaybackState();
  void UpdateRenderFrameBackgroundVideoPlaybackState(
      content::RenderFrameHost* frame);

  bool PlayingBlocked() const;

  // MediaSession when initialized from WebContesnts is always a
  // MediaSessionImpl type. This method allows to replace the MediaSession with
  // mockable MediaSessions for testing.
  void SetMediaSessionForTesting(content::MediaSession* media_session);

  // Whether or not media starting should be blocked. This value caches the last
  // call to BlockMediaStarting.
  bool media_starting_blocked_ = false;

  // Whether or not the user paused media on the page.
  bool paused_by_user_ = true;

  // Whether or not media in the app can be controlled and if media is currently
  // suspended. These variables cache arguments from MediaSessionInfoChanged().
  bool suspended_ = true;
  bool controllable_ = false;

  // Setting for whether or not the WebContents should suspend video when the
  // content is put into the background. For most content, this setting should
  // be disabled.
  bool background_video_playback_enabled_ = false;

  content::MediaSession* media_session_;

  mojo::Receiver<media_session::mojom::MediaSessionObserver> observer_receiver_{
      this};
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_
