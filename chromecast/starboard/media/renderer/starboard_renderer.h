// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_RENDERER_STARBOARD_RENDERER_H_
#define CHROMECAST_STARBOARD_MEDIA_RENDERER_STARBOARD_RENDERER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/renderer/geometry_change_handler.h"
#include "chromecast/starboard/media/renderer/starboard_player_manager.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"

namespace chromecast {

namespace metrics {
class CastMetricsHelper;
}  // namespace metrics

namespace media {

class VideoGeometrySetterService;

// A renderer implementation that calls into an SbPlayer for decoding and
// rendering.
//
// All public functions of this class must be called on a single sequence
// (media_task_runner, passed into the constructor).
class StarboardRenderer : public ::media::Renderer {
 public:
  StarboardRenderer(
      std::unique_ptr<StarboardApiWrapper> starboard,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      const base::UnguessableToken& overlay_plane_id,
      bool enable_buffering,
      VideoGeometrySetterService* geometry_setter_service,
      chromecast::metrics::CastMetricsHelper* cast_metrics_helper);

  // Disallow copy and assign.
  StarboardRenderer(const StarboardRenderer&) = delete;
  StarboardRenderer& operator=(const StarboardRenderer&) = delete;

  ~StarboardRenderer() override;

  // Renderer implementation:
  void Initialize(::media::MediaResource* media_resource,
                  ::media::RendererClient* client,
                  ::media::PipelineStatusCallback init_cb) override;
  void SetCdm(::media::CdmContext* cdm_context,
              CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  ::media::RendererType GetRendererType() override;

  // If cast needs to support track changes in the future,
  // OnSelectedVideoTracksChanged and OnEnabledAudioTracksChanged should be
  // overridden here.

 private:
  // Creates the underlying SbPlayer object and runs init_cb.
  void InitializeInternal(::media::PipelineStatusCallback init_cb);

  std::unique_ptr<StarboardApiWrapper> starboard_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  raw_ptr<::media::RendererClient> client_ = nullptr;
  std::unique_ptr<StarboardPlayerManager> player_manager_;
  // This must be destructed before starboard_.
  GeometryChangeHandler geometry_change_handler_;
  raw_ptr<chromecast::metrics::CastMetricsHelper> cast_metrics_helper_ =
      nullptr;
  bool enable_buffering_ = true;
  // Whether a Cast.Platform.Ended message has already been reported for this
  // play. Used to avoid double reporting the Cast.Platform.Ended metric.
  bool end_reported_ = false;

  // This is set if a volume change is made before SbPlayer is created.
  std::optional<float> pending_volume_;

  // This should be destructed first, to invalidate any references to this.
  base::WeakPtrFactory<StarboardRenderer> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_RENDERER_STARBOARD_RENDERER_H_
