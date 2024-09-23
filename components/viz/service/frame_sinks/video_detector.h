// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_DETECTOR_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_DETECTOR_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"

namespace viz {

class SurfaceManager;
class VideoDetectorTest;

// Watches for updates to clients and tries to detect when a video is playing.
// If a client sends CompositorFrames with damages of size at least
// |kMinDamageWidth| x |kMinDamageHeight| at the rate of at least
// |kMinFramesPerSecond| for the duration of at least |kMinVideoDuration| then
// we say it is playing video.  We err on the side of false positives and can be
// fooled by things like continuous scrolling of a page.
class VIZ_SERVICE_EXPORT VideoDetector : public SurfaceObserver {
 public:
  VideoDetector(
      const std::vector<FrameSinkId>& registered_frame_sink_ids,
      SurfaceManager* surface_manager,
      const base::TickClock* tick_clock = base::DefaultTickClock::GetInstance(),
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);

  VideoDetector(const VideoDetector&) = delete;
  VideoDetector& operator=(const VideoDetector&) = delete;

  ~VideoDetector() override;

  // Adds an observer. The observer can be removed by closing the mojo
  // connection.
  void AddObserver(
      mojo::PendingRemote<mojom::VideoDetectorObserver> pending_observer);

  // When a FrameSinkId is registered/invalidated, we need to insert/delete the
  // corresponding entry in client_infos_.
  void OnFrameSinkIdRegistered(const FrameSinkId& frame_sink_id);
  void OnFrameSinkIdInvalidated(const FrameSinkId& frame_sink_id);

 private:
  friend class VideoDetectorTest;

  class ClientInfo;

  // Minimum dimensions in pixels that damages must have to be considered a
  // potential video frame.
  static constexpr int kMinDamageWidth = 333;
  static constexpr int kMinDamageHeight = 250;

  // Number of video-sized updates that we must see within a second in a client
  // before we assume that a video is playing.
  static constexpr int kMinFramesPerSecond = 15;

  // A video will no longer be consider playing at some interval between
  // 'kMinVideoTimeout' to 'kMaxVideoTimeout'.
  static constexpr base::TimeDelta kMinVideoTimeout = base::Milliseconds(500);
  static constexpr base::TimeDelta kMaxVideoTimeout = base::Milliseconds(1000);

  // Duration video must be playing in a client before it is reported to
  // observers.
  static constexpr base::TimeDelta kMinVideoDuration = base::Milliseconds(3000);

  // If no video activity is detected for |kVideoTimeout|, this
  // method will be called by |video_inactive_timer_|;
  void OnVideoActivityEnded();

  // SurfaceObserver implementation.
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override {}
  void OnSurfaceActivated(const SurfaceId& surface_id) override {}
  void OnSurfaceMarkedForDestruction(const SurfaceId& surface_id) override {}
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const BeginFrameAck& ack,
                        HandleInteraction handle_interaction) override;
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override {}
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const BeginFrameArgs& args) override {}
  void OnSurfaceWillBeDrawn(Surface* surface) override;

  // True if video has been observed in the last |kVideoTimeout|.
  bool video_is_playing_ = false;

  // Provides the current time.
  raw_ptr<const base::TickClock> tick_clock_;

  // Calls OnVideoActivityEnded() after |kVideoTimeout|. Uses |tick_clock_| to
  // measure time.
  base::OneShotTimer video_inactive_timer_;

  // Contains information used for determining video activity in each client.
  base::flat_map<FrameSinkId, std::unique_ptr<ClientInfo>> client_infos_;

  // Observers that are interested to know about video activity. We only detect
  // video activity if there is at least one client.
  mojo::RemoteSet<mojom::VideoDetectorObserver> observers_;

  const raw_ptr<SurfaceManager> surface_manager_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_DETECTOR_H_
