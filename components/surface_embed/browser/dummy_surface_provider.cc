// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/browser/dummy_surface_provider.h"

#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "content/public/browser/context_factory.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace surface_embed {

DummySurfaceProvider::DummySurfaceProvider()
    : frame_sink_manager_(
          content::GetContextFactory()->GetHostFrameSinkManager()) {
  frame_sink_id_ = content::GetContextFactory()->AllocateFrameSinkId();
  frame_sink_manager_->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  frame_sink_manager_->CreateCompositorFrameSink(
      frame_sink_id_,
      compositor_frame_sink_remote_.BindNewPipeAndPassReceiver(),
      compositor_frame_sink_client_receiver_.BindNewPipeAndPassRemote());
}

DummySurfaceProvider::~DummySurfaceProvider() {
  frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_, this, {});
}

void DummySurfaceProvider::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    float device_scale_factor,
    const gfx::Size& frame_size_in_pixels) {
  viz::CompositorFrame frame;
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.device_scale_factor = device_scale_factor;
  frame.metadata.frame_token = ++frame_token_generator_;

  const gfx::Rect output_rect(frame_size_in_pixels);

  auto pass = viz::CompositorRenderPass::Create();
  pass->SetNew(viz::CompositorRenderPassId{1}, output_rect, output_rect,
               gfx::Transform());
  viz::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), /*layer_rect=*/output_rect,
              /*visible_layer_rect=*/output_rect,
              /*filter_info=*/gfx::MaskFilterInfo(),
              /*clip=*/std::nullopt,
              /*contents_opaque=*/true, /*opacity_f=*/1.f,
              SkBlendMode::kSrcOver, /*sorting_context=*/0,
              /*layer_id=*/0, /*fast_rounded_corner=*/false);
  viz::SolidColorDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  quad->SetNew(sqs, output_rect, output_rect, SkColors::kRed,
               /*anti_aliasing_off=*/false);
  frame.render_pass_list.push_back(std::move(pass));

  compositor_frame_sink_remote_->SubmitCompositorFrame(
      local_surface_id, std::move(frame), std::nullopt, 0);
}

void DummySurfaceProvider::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {}

void DummySurfaceProvider::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {}

void DummySurfaceProvider::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {}

void DummySurfaceProvider::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const base::flat_map<uint32_t, viz::FrameTimingDetails>& details,
    std::vector<viz::ReturnedResource> resources) {}

void DummySurfaceProvider::OnBeginFramePausedChanged(bool paused) {}

void DummySurfaceProvider::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {}

void DummySurfaceProvider::OnCompositorFrameTransitionDirectiveProcessed(
    uint32_t sequence_id) {}

void DummySurfaceProvider::OnSurfaceEvicted(
    const viz::LocalSurfaceId& local_surface_id) {}

}  // namespace surface_embed
