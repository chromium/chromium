// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_MEDIA_PLAYBACK_OPTIONS_H_
#define CHROMECAST_RENDERER_CAST_MEDIA_PLAYBACK_OPTIONS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/sequence_checker.h"
#include "chromecast/common/mojom/media_playback_options.mojom.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace chromecast {

// Manages options for suspending playback and deferring media load.
// Based on Chrome prerender. Manages its own lifetime.
class CastMediaPlaybackOptions
    : public chromecast::shell::mojom::MediaPlaybackOptions,
      public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<CastMediaPlaybackOptions> {
 public:
  explicit CastMediaPlaybackOptions(content::RenderFrame* render_frame);

  // Runs |closure| if the page/frame is switched to foreground. Returns true if
  // the running of |closure| is deferred (not yet in foreground); false if
  // |closure| can be run immediately.
  bool RunWhenInForeground(base::OnceClosure closure);

  bool IsBackgroundSuspendEnabled() const;

 private:
  ~CastMediaPlaybackOptions() override;

  // content::RenderFrameObserver implementation:
  void OnDestruct() override;

  // MediaPlaybackOptions implementation
  void SetMediaLoadingBlocked(bool blocked) override;
  void SetBackgroundVideoPlaybackEnabled(bool enabled) override;
  void SetUseCmaRenderer(bool enable) override;

  void OnMediaPlaybackOptionsAssociatedReceiver(
      mojo::PendingAssociatedReceiver<
          chromecast::shell::mojom::MediaPlaybackOptions> receiver);

  bool render_frame_action_blocked_;
  content::RenderFrameMediaPlaybackOptions renderer_media_playback_options_;

  std::vector<base::OnceClosure> pending_closures_;

  mojo::AssociatedReceiverSet<chromecast::shell::mojom::MediaPlaybackOptions>
      receivers_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CastMediaPlaybackOptions);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_MEDIA_PLAYBACK_OPTIONS_H_
