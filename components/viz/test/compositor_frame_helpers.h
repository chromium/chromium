// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_
#define COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "ui/latency/latency_info.h"

namespace viz {

// A builder class for constructing CompositorFrames in tests. The initial
// CompositorFrame will have a valid BeginFrameAck and device_scale_factor of 1.
// At least one RenderPass must be added for the CompositorFrame to be valid.
class CompositorFrameBuilder {
 public:
  CompositorFrameBuilder();
  ~CompositorFrameBuilder();

  // Builds the CompositorFrame and leaves |this| in an invalid state. This can
  // only be called once.
  CompositorFrame Build();

  // Adds a render pass with 20x20 output_rect and empty damage_rect.
  CompositorFrameBuilder& AddDefaultRenderPass();
  // Adds a render pass with specified |output_rect| and |damage_rect|.
  CompositorFrameBuilder& AddRenderPass(const gfx::Rect& output_rect,
                                        const gfx::Rect& damage_rect);
  CompositorFrameBuilder& AddRenderPass(
      std::unique_ptr<CompositorRenderPass> render_pass);
  // Sets list of render passes. The list of render passes must be empty when
  // this is called.
  CompositorFrameBuilder& SetRenderPassList(
      CompositorRenderPassList render_pass_list);

  CompositorFrameBuilder& AddTransferableResource(
      TransferableResource resource);
  // Sets list of transferable resources. The list of transferable resources
  // must be empty when this is called.
  CompositorFrameBuilder& SetTransferableResources(
      std::vector<TransferableResource> resource_list);

  // Sets the BeginFrameAck. This replaces the default BeginFrameAck.
  CompositorFrameBuilder& SetBeginFrameAck(const BeginFrameAck& ack);
  CompositorFrameBuilder& SetDeviceScaleFactor(float device_scale_factor);
  CompositorFrameBuilder& AddLatencyInfo(ui::LatencyInfo latency_info);
  CompositorFrameBuilder& AddLatencyInfos(
      std::vector<ui::LatencyInfo> latency_info);
  CompositorFrameBuilder& SetReferencedSurfaces(
      std::vector<SurfaceRange> referenced_surfaces);
  CompositorFrameBuilder& SetActivationDependencies(
      std::vector<SurfaceId> activation_dependencies);
  CompositorFrameBuilder& SetDeadline(const FrameDeadline& deadline);
  CompositorFrameBuilder& SetSendFrameTokenToEmbedder(bool send);

 private:
  CompositorFrame MakeInitCompositorFrame() const;

  base::Optional<CompositorFrame> frame_;
  CompositorRenderPassId::Generator render_pass_id_generator_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameBuilder);
};

// Creates a CompositorFrame that has a render pass with 20x20 output_rect and
// empty damage_rect. This CompositorFrame is valid and can be sent over IPC.
CompositorFrame MakeDefaultCompositorFrame();

// Makes an aggregated frame out of the default compositor frame.
AggregatedFrame MakeDefaultAggregatedFrame(size_t num_render_passes = 1);

// Creates a CompositorFrame that will be valid once its render_pass_list is
// initialized.
CompositorFrame MakeEmptyCompositorFrame();

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_
