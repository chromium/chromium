// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_
#define CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_

#include <vector>

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
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
class CastMediaBlocker : public content::WebContentsObserver,
                         public media_session::mojom::MediaSessionObserver {
 public:
  // Observes WebContents and MediaSession.
  explicit CastMediaBlocker(content::WebContents* web_contents);

  // Observes only the MediaSession.
  explicit CastMediaBlocker(content::MediaSession* media_session);

  ~CastMediaBlocker() override;

  // Sets if the web contents is allowed to load and play media or not.
  // If media is unblocked, previously suspended elements should begin playing
  // again. Media is unblocked when both MediaLoading and MediaStarting blocks
  // are off.
  void BlockMediaLoading(bool blocked);
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
      const base::Optional<media_session::MediaMetadata>& metadata) override {}
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override {}
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      override {}
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

 private:
  friend shell::CastMediaBlockerTest;
  // content::WebContentsObserver implementation:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderViewReady() override;

  // Suspends or resumes the media session for the web contents.
  void Suspend();
  void Resume();

  // Blocks or unblocks the render process from loading new media
  // according to |blocked_|.
  void UpdateMediaLoadingBlockedState();
  void UpdateRenderFrameMediaLoadingBlockedState(
      content::RenderFrameHost* render_frame_host);

  void UpdatePlayingState();

  void UpdateBackgroundVideoPlaybackState();
  void UpdateRenderFrameBackgroundVideoPlaybackState(
      content::RenderFrameHost* frame);

  bool PlayingBlocked() const;

  // MediaSession when initialized from WebContesnts is always a
  // MediaSessionImpl type. This method allows to replace the MediaSession with
  // mockable MediaSessions for testing.
  void SetMediaSessionForTesting(content::MediaSession* media_session);

  // Whether or not media loading should be blocked. This value cache's the last
  // call to BlockMediaLoading. Is false by default.
  bool media_loading_blocked_;

  // Whether or not media starting should be blocked. This value cache's the
  // last call to BlockMediaStarting. Is false by default.
  bool media_starting_blocked_;

  // Whether or not the user paused media on the page.
  bool paused_by_user_;

  // Whether or not media in the app can be controlled and if media is currently
  // suspended. These variables cache arguments from MediaSessionStateChanged().
  bool suspended_;
  bool controllable_;

  // Setting for whether or not the WebContents should suspend video when the
  // content is put into the background. For most content, this setting should
  // be disabled.
  bool background_video_playback_enabled_;

  content::MediaSession* media_session_;

  mojo::Receiver<media_session::mojom::MediaSessionObserver> observer_receiver_{
      this};

  DISALLOW_COPY_AND_ASSIGN(CastMediaBlocker);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_
