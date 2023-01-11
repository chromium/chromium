// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_CONTROL_RENDERER_MEDIA_PLAYBACK_OPTIONS_H_
#define COMPONENTS_MEDIA_CONTROL_RENDERER_MEDIA_PLAYBACK_OPTIONS_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "components/media_control/mojom/media_playback_options.mojom.h"
#include "content/public/common/media_playback_renderer_type.mojom.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace media_control {

// Manages options for suspending playback and deferring media load.
// Based on Chrome prerender. Manages its own lifetime.
class MediaPlaybackOptions
    : public components::media_control::mojom::MediaPlaybackOptions,
      public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<MediaPlaybackOptions> {
 public:
  explicit MediaPlaybackOptions(content::RenderFrame* render_frame);

  MediaPlaybackOptions(const MediaPlaybackOptions&) = delete;
  MediaPlaybackOptions& operator=(const MediaPlaybackOptions&) = delete;

  // Runs |closure| if the page/frame is switched to foreground. Returns true if
  // the running of |closure| is deferred (not yet in foreground); false if
  // |closure| can be run immediately.
  bool RunWhenInForeground(base::OnceClosure closure);

  bool IsBackgroundSuspendEnabled() const;

 private:
  ~MediaPlaybackOptions() override;

  // content::RenderFrameObserver implementation:
  void OnDestruct() override;

  // MediaPlaybackOptions implementation
  void SetMediaLoadingBlocked(bool blocked) override;
  void SetBackgroundVideoPlaybackEnabled(bool enabled) override;
  void SetRendererType(content::mojom::RendererType type) override;

  void OnMediaPlaybackOptionsAssociatedReceiver(
      mojo::PendingAssociatedReceiver<
          components::media_control::mojom::MediaPlaybackOptions> receiver);

  bool render_frame_action_blocked_;
  content::RenderFrameMediaPlaybackOptions renderer_media_playback_options_;

  std::vector<base::OnceClosure> pending_closures_;

  mojo::AssociatedReceiverSet<
      components::media_control::mojom::MediaPlaybackOptions>
      receivers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_control

#endif  // COMPONENTS_MEDIA_CONTROL_RENDERER_MEDIA_PLAYBACK_OPTIONS_H_
