// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_LAYER_TREE_FRAME_SINK_HOLDER_H_
#define COMPONENTS_EXO_LAYER_TREE_FRAME_SINK_HOLDER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/exo/frame_sink_resource_manager.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/release_callback.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
class LayerTreeFrameSink;
}

namespace exo {

class SurfaceTreeHost;

// This class talks to CompositorFrameSink and keeps track of references to
// the contents of Buffers.
class LayerTreeFrameSinkHolder : public cc::LayerTreeFrameSinkClient,
                                 public WMHelper::LifetimeManager::Observer {
 public:
  LayerTreeFrameSinkHolder(SurfaceTreeHost* surface_tree_host,
                           std::unique_ptr<cc::LayerTreeFrameSink> frame_sink);
  ~LayerTreeFrameSinkHolder() override;

  // Delete frame sink after having reclaimed and called all resource
  // release callbacks.
  // TODO(reveman): Find a better way to handle deletion of in-flight resources.
  // crbug.com/765763
  static void DeleteWhenLastResourceHasBeenReclaimed(
      std::unique_ptr<LayerTreeFrameSinkHolder> holder);

  void SubmitCompositorFrame(viz::CompositorFrame frame);
  void DidNotProduceFrame(const viz::BeginFrameAck& ack);

  // Returns true if owned LayerTreeFrameSink has been lost.
  bool is_lost() const { return is_lost_; }

  FrameSinkResourceManager* resource_manager() { return &resource_manager_; }

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  base::Optional<viz::HitTestRegionList> BuildHitTestData() override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
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

 private:
  void ScheduleDelete();

  // WMHelper::LifetimeManager::Observer:
  void OnDestroyed() override;

  SurfaceTreeHost* surface_tree_host_;
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;

  FrameSinkResourceManager resource_manager_;

  gfx::Size last_frame_size_in_pixels_;
  float last_frame_device_scale_factor_ = 1.0f;
  base::TimeTicks last_local_surface_id_allocation_time_;
  std::vector<viz::ResourceId> last_frame_resources_;
  viz::FrameTokenGenerator next_frame_token_;

  bool is_lost_ = false;
  bool delete_pending_ = false;

  WMHelper::LifetimeManager* lifetime_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeFrameSinkHolder);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_LAYER_TREE_FRAME_SINK_HOLDER_H_
