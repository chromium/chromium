// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_LAYER_TREE_FRAME_SINK_HOLDER_H_
#define COMPONENTS_EXO_LAYER_TREE_FRAME_SINK_HOLDER_H_

#include <memory>
#include <optional>

#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/exo/frame_sink_resource_manager.h"
#include "components/exo/frame_timing_history.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc::mojo_embedder {
class AsyncLayerTreeFrameSink;
}

namespace exo {

class SurfaceTreeHost;

// When this feature is disabled (by default at the moment), frames are
// submitted to the remote side as soon as they arrive, disregarding BeginFrame
// requests.
//
// TODO(yzshen): Remove this flag and always submit according to BeginFrame
// requests. crbug.com/1408614
BASE_DECLARE_FEATURE(kExoReactiveFrameSubmission);

// This class talks to CompositorFrameSink and keeps track of references to
// the contents of Buffers.
class LayerTreeFrameSinkHolder : public cc::LayerTreeFrameSinkClient,
                                 public WMHelper::LifetimeManager::Observer,
                                 public viz::BeginFrameObserverBase {
 public:
  LayerTreeFrameSinkHolder(
      SurfaceTreeHost* surface_tree_host,
      std::unique_ptr<cc::mojo_embedder::AsyncLayerTreeFrameSink> frame_sink);

  LayerTreeFrameSinkHolder(const LayerTreeFrameSinkHolder&) = delete;
  LayerTreeFrameSinkHolder& operator=(const LayerTreeFrameSinkHolder&) = delete;

  ~LayerTreeFrameSinkHolder() override;

  // Delete frame sink after having reclaimed and called all resource
  // release callbacks.
  // TODO(reveman): Find a better way to handle deletion of in-flight resources.
  // crbug.com/765763
  static void DeleteWhenLastResourceHasBeenReclaimed(
      std::unique_ptr<LayerTreeFrameSinkHolder> holder);

  // If a frame is submitted "now" (meaning before returning to event loop)
  // via SubmitCompositorFrame(), whether it needs full damage.
  bool NeedsFullDamageForNextFrame() const { return cached_frame_.has_value(); }
  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool submit_now = false);
  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id);

  // Properties of the `frame` from the last `SubmitCompositorFrame()` call,
  // either from `cached_frame_`, or `frame_sink_`.
  float LastDeviceScaleFactor() const;
  const gfx::Size& LastSizeInPixels() const;

  // Returns true if owned LayerTreeFrameSink has been lost.
  bool is_lost() const { return is_lost_; }

  FrameSinkResourceManager* resource_manager() { return &resource_manager_; }

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  std::optional<viz::HitTestRegionList> BuildHitTestData() override;
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void SetTreeActivationCallback(base::RepeatingClosure callback) override {}
  void DidReceiveCompositorFrameAck() override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override;
  void DidLoseLayerTreeFrameSink() override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw,
              bool skip_draw) override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override {}

  void ClearPendingBeginFramesForTesting();

 private:
  struct PendingBeginFrame {
    viz::BeginFrameAck begin_frame_ack;
    base::TimeTicks send_deadline_estimate;
  };

  void ScheduleDelete();

  // WMHelper::LifetimeManager::Observer:
  void OnDestroyed() override;

  // viz::BeginFrameObserverBase:
  bool OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

  void SubmitCompositorFrameToRemote(viz::CompositorFrame* frame);

  // Discards `cached_frame_`, reclaims resources and returns failure
  // presentation feedback.
  void DiscardCachedFrame(const viz::CompositorFrame* new_frame);
  void SendDiscardedFrameNotifications(uint32_t frame_token);

  void StopProcessingPendingFrames();

  void OnSendDeadlineExpired(bool update_timer);

  // Starts timer based on estimated deadline of the first pending BeginFrame
  // request; or stops timer if there is no pending BeginFrame request.
  void UpdateSubmitFrameTimer();

  void ProcessFirstPendingBeginFrame(viz::CompositorFrame* frame);

  bool ShouldSubmitFrameNow() const;

  void ObserveBeginFrameSource(bool start);

  // Returns true if the feature AutoNeedsBeginFrame is enabled, and currently
  // we are not receiving BeginFrame requests. In this case, it is allowed to
  // submit an unsolicited frame.
  bool UnsolicitedFrameAllowed() const;

  raw_ptr<SurfaceTreeHost> surface_tree_host_;
  std::unique_ptr<cc::mojo_embedder::AsyncLayerTreeFrameSink> frame_sink_;

  FrameSinkResourceManager resource_manager_;

  std::vector<viz::ResourceId> last_frame_resources_;

  std::optional<viz::CompositorFrame> cached_frame_;

  // Resources that are submitted and still in use by the remote side.
  std::set<viz::ResourceId> in_use_resources_;

  bool is_lost_ = false;
  bool delete_pending_ = false;

  raw_ptr<WMHelper::LifetimeManager> lifetime_manager_ = nullptr;

  raw_ptr<viz::BeginFrameSource> begin_frame_source_ = nullptr;
  bool observing_begin_frame_source_ = false;

  base::queue<PendingBeginFrame> pending_begin_frames_;

  // The number of frames submitted to the remote side for which acks haven't
  // been received.
  int pending_submit_frames_ = 0;

  // A queue of discarded frame tokens for which acks and presentation feedbacks
  // haven't been sent to `surface_tree_host_`.
  base::queue<uint32_t> pending_discarded_frame_notifications_;

  base::DeadlineTimer submit_frame_timer_;

  const bool reactive_frame_submission_ = false;

  // Set if `reactive_frame_submission_` is enabled.
  std::optional<FrameTimingHistory> frame_timing_history_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_LAYER_TREE_FRAME_SINK_HOLDER_H_
