// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_detector.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

constexpr base::TimeDelta VideoDetector::kMaxVideoTimeout;
constexpr base::TimeDelta VideoDetector::kMinVideoTimeout;
constexpr base::TimeDelta VideoDetector::kMinVideoDuration;

// Stores information about updates to a client and determines whether it's
// likely that a video is playing in it.
class VideoDetector::ClientInfo {
 public:
  ClientInfo() = default;

  ClientInfo(const ClientInfo&) = delete;
  ClientInfo& operator=(const ClientInfo&) = delete;

  // Called when a Surface belonging to this client is drawn. Returns true if we
  // determine that video is playing in this client.
  bool ReportDrawnAndCheckForVideo(Surface* surface, base::TimeTicks now) {
    uint64_t frame_index = surface->GetActiveFrameIndex();

    // If |frame_index| hasn't increased, then no new frame was submitted since
    // the last draw.
    if (frame_index <= last_drawn_frame_index_)
      return false;

    last_drawn_frame_index_ = frame_index;

    const CompositorFrame& frame = surface->GetActiveFrame();

    if (!frame.metadata.may_contain_video) {
      return false;
    }

    gfx::Rect damage = frame.render_pass_list.back()->damage_rect;
    if (frame.render_pass_list.back()->has_per_quad_damage) {
      for (auto* quad : frame.render_pass_list.back()->quad_list) {
        if (quad->material != DrawQuad::Material::kTextureContent)
          continue;

        auto* texture_quad = TextureDrawQuad::MaterialCast(quad);
        if (!texture_quad->damage_rect)
          continue;

        damage.Union(*texture_quad->damage_rect);
      }
    }

    damage =
        gfx::ScaleToEnclosingRect(damage, 1.f / frame.device_scale_factor());

    if (damage.width() < kMinDamageWidth || damage.height() < kMinDamageHeight)
      return false;

    // If the buffer is full, drop the first timestamp.
    if (buffer_size_ == kMinFramesPerSecond) {
      buffer_start_ = (buffer_start_ + 1) % kMinFramesPerSecond;
      buffer_size_--;
    }

    update_times_[(buffer_start_ + buffer_size_) % kMinFramesPerSecond] = now;
    buffer_size_++;

    const bool in_video =
        (buffer_size_ == kMinFramesPerSecond) &&
        (now - update_times_[buffer_start_] <= base::Seconds(1));

    if (in_video && video_start_time_.is_null())
      video_start_time_ = update_times_[buffer_start_];
    else if (!in_video && !video_start_time_.is_null())
      video_start_time_ = base::TimeTicks();

    const base::TimeDelta elapsed = now - video_start_time_;
    return in_video && elapsed >= kMinVideoDuration;
  }

 private:
  // Circular buffer containing update times of the last (up to
  // |kMinFramesPerSecond|) video-sized updates to this client.
  std::array<base::TimeTicks, kMinFramesPerSecond> update_times_;

  // Time at which the current sequence of updates that looks like video
  // started. Empty if video isn't currently playing.
  base::TimeTicks video_start_time_;

  // Index into |update_times_| of the oldest update.
  uint32_t buffer_start_ = 0;

  // Number of updates stored in |update_times_|.
  uint32_t buffer_size_ = 0;

  // Frame index of the last drawn Surface. We use this number to determine
  // whether a new frame was submitted since the last time the Surface was
  // drawn.
  uint64_t last_drawn_frame_index_ = 0;
};

VideoDetector::VideoDetector(
    const std::vector<FrameSinkId>& registered_frame_sink_ids,
    SurfaceManager* surface_manager,
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : tick_clock_(tick_clock),
      video_inactive_timer_(tick_clock),
      surface_manager_(surface_manager) {
  surface_manager_->AddObserver(this);
  for (auto& frame_sink_id : registered_frame_sink_ids)
    client_infos_[frame_sink_id] = std::make_unique<ClientInfo>();
  if (task_runner)
    video_inactive_timer_.SetTaskRunner(task_runner);
}

VideoDetector::~VideoDetector() {
  surface_manager_->RemoveObserver(this);
}

void VideoDetector::OnVideoActivityEnded() {
  DCHECK(video_is_playing_);
  video_is_playing_ = false;
  for (auto& observer : observers_) {
    observer->OnVideoActivityEnded();
  }
}

void VideoDetector::AddObserver(
    mojo::PendingRemote<mojom::VideoDetectorObserver> pending_observer) {
  mojo::Remote<mojom::VideoDetectorObserver> observer(
      std::move(pending_observer));
  if (video_is_playing_)
    observer->OnVideoActivityStarted();
  observers_.Add(std::move(observer));
}

void VideoDetector::OnFrameSinkIdRegistered(const FrameSinkId& frame_sink_id) {
  DCHECK(!client_infos_.count(frame_sink_id));
  client_infos_[frame_sink_id] = std::make_unique<ClientInfo>();
}

void VideoDetector::OnFrameSinkIdInvalidated(const FrameSinkId& frame_sink_id) {
  client_infos_.erase(frame_sink_id);
}

bool VideoDetector::OnSurfaceDamaged(const SurfaceId& surface_id,
                                     const BeginFrameAck& ack,
                                     HandleInteraction handle_interaction) {
  return false;
}

// |surface| is scheduled to be drawn. See if it has a new frame since the
// last time it was drawn and record the damage.
void VideoDetector::OnSurfaceWillBeDrawn(Surface* surface) {
  // If there is no observer, don't waste cycles detecting video activity.
  if (observers_.empty())
    return;

  const FrameSinkId& frame_sink_id = surface->surface_id().frame_sink_id();

  auto it = client_infos_.find(frame_sink_id);

  // If the corresponding entry in |client_infos_| does not exist, it means the
  // FrameSinkId has been invalidated and the client's CompositorFrameSink is
  // destroyed so it cannot send new frames and video activity is impossible.
  if (it == client_infos_.end())
    return;

  base::TimeTicks now = tick_clock_->NowTicks();

  if (it->second->ReportDrawnAndCheckForVideo(surface, now)) {
    // Avoid (re)starting the timer every frame since it has considerable
    // overhead.
    if (!video_inactive_timer_.IsRunning() ||
        (video_inactive_timer_.desired_run_time() - now) < kMinVideoTimeout) {
      video_inactive_timer_.Start(FROM_HERE, kMaxVideoTimeout, this,
                                  &VideoDetector::OnVideoActivityEnded);
    }
    if (!video_is_playing_) {
      video_is_playing_ = true;
      for (auto& observer : observers_) {
        observer->OnVideoActivityStarted();
      }
    }
  }
}

}  // namespace viz
