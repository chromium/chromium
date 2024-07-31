// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/demo/client/demo_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"

namespace demo {

DemoClient::DemoClient(const viz::FrameSinkId& frame_sink_id,
                       const viz::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& bounds)
    : thread_(frame_sink_id.ToString()),
      frame_sink_id_(frame_sink_id),
      local_surface_id_(local_surface_id),
      bounds_(bounds) {
  CHECK(thread_.Start());
}

DemoClient::~DemoClient() = default;

void DemoClient::Initialize(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> receiver,
    mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
        sink_remote) {
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DemoClient::InitializeOnThread,
                                base::Unretained(this), std::move(receiver),
                                std::move(sink_remote), mojo::NullRemote()));
}

void DemoClient::Initialize(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> receiver,
    mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink_remote) {
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DemoClient::InitializeOnThread, base::Unretained(this),
                     std::move(receiver), mojo::NullAssociatedRemote(),
                     std::move(sink_remote)));
}

viz::LocalSurfaceId DemoClient::Embed(const viz::FrameSinkId& frame_sink_id,
                                      const gfx::Rect& bounds) {
  // |embeds_| is used on the client-thread in CreateFrame(). So this needs to
  // be mutated under a lock.
  base::AutoLock lock(lock_);
  allocator_.GenerateId();
  embeds_[frame_sink_id] = {allocator_.GetCurrentLocalSurfaceId(), bounds};
  return embeds_[frame_sink_id].lsid;
}

void DemoClient::Resize(const gfx::Size& size,
                        const viz::LocalSurfaceId& local_surface_id) {
  // |bounds_| and |local_surface_id_| are used on the client-thread in
  // CreateFrame(). So these need to be mutated under a lock.
  base::AutoLock lock(lock_);
  bounds_.set_size(size);
  local_surface_id_ = local_surface_id;
}

viz::CompositorFrame DemoClient::CreateFrame(const viz::BeginFrameArgs& args) {
  constexpr SkColor4f colors[] = {SkColors::kRed, SkColors::kGreen,
                                  SkColors::kYellow};
  viz::CompositorFrame frame;

  frame.metadata.begin_frame_ack = viz::BeginFrameAck(args, true);
  frame.metadata.device_scale_factor = 1.f;
  frame.metadata.frame_token = ++next_frame_token_;

  const viz::CompositorRenderPassId kRenderPassId{1};
  const gfx::Rect& output_rect = bounds_;
  const gfx::Rect& damage_rect = output_rect;
  auto render_pass = viz::CompositorRenderPass::Create();
  render_pass->SetNew(kRenderPassId, output_rect, damage_rect,
                      gfx::Transform());

  // The content of the client is one big solid-color rectangle, which includes
  // the other clients above it (in z-order). The embedded clients are first
  // added to the CompositorFrame using their SurfaceId (i.e. the FrameSinkId
  // and LocalSurfaceId), and then the big rectangle is added afterwards.
  for (auto& iter : embeds_) {
    const gfx::Rect& child_bounds = iter.second.bounds;
    const gfx::Vector2dF center(child_bounds.width() / 2,
                                child_bounds.height() / 2);

    // Apply a rotation so there's visual-update every frame in the demo.
    gfx::Transform transform;
    transform.Translate(center + child_bounds.OffsetFromOrigin());
    transform.Rotate(iter.second.degrees);
    iter.second.degrees += 0.3;
    transform.Translate(-center);

    viz::SharedQuadState* quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    quad_state->SetAll(transform,
                       /*layer_rect=*/child_bounds,
                       /*visible_layer_rect=*/child_bounds,
                       /*filter_info=*/gfx::MaskFilterInfo(),
                       /*clip=*/std::nullopt,
                       /*contents_opaque=*/false, /*opacity_f=*/1.f,
                       /*blend=*/SkBlendMode::kSrcOver,
                       /*sorting_context=*/0,
                       /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    viz::SurfaceDrawQuad* embed =
        render_pass->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
    viz::SurfaceId surface_id(iter.first, iter.second.lsid);
    // |rect| and |visible_rect| needs to be in the quad's coord-space, so to
    // draw the whole quad, it needs to use origin (0, 0).
    embed->SetNew(quad_state,
                  /*rect=*/gfx::Rect(child_bounds.size()),
                  /*visible_rect=*/gfx::Rect(child_bounds.size()),
                  viz::SurfaceRange(surface_id), SkColors::kGray,
                  /*stretch_content_to_fill_bounds=*/false);
  }

  // Add a solid-color draw-quad for the big rectangle covering the entire
  // content-area of the client.
  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(gfx::Transform(),
                     /*quad_layer_rect=*/output_rect,
                     /*visible_layer_rect=*/output_rect,
                     /*mask_filter_info=*/gfx::MaskFilterInfo(),
                     /*clip_rect=*/std::nullopt, /*are_contents_opaque=*/false,
                     /*opacity=*/1.f,
                     /*blend_mode=*/SkBlendMode::kSrcOver,
                     /*sorting_context=*/0,
                     /*layer_id=*/0u,
                     /*fast_rounded_corner=*/false);

  viz::SolidColorDrawQuad* color_quad =
      render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->SetNew(quad_state, output_rect, output_rect,
                     colors[(++frame_count_ / 60) % std::size(colors)], false);

  frame.render_pass_list.push_back(std::move(render_pass));

  return frame;
}

viz::mojom::CompositorFrameSink* DemoClient::GetPtr() {
  if (associated_sink_.is_bound())
    return associated_sink_.get();
  return sink_.get();
}

void DemoClient::InitializeOnThread(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> receiver,
    mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
        associated_sink_remote,
    mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink_remote) {
  receiver_.Bind(std::move(receiver));
  if (associated_sink_remote)
    associated_sink_.Bind(std::move(associated_sink_remote));
  else
    sink_.Bind(std::move(sink_remote));
  // Request for begin-frames.
  GetPtr()->SetNeedsBeginFrame(true);
}

void DemoClient::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {
  // See documentation in mojom for how this can be used.
}

void DemoClient::OnBeginFrame(const viz::BeginFrameArgs& args,
                              const viz::FrameTimingDetailsMap& timing_details,
                              bool frame_ack,
                              std::vector<viz::ReturnedResource> resources) {
  // Generate a new compositor-frame for each begin-frame. This demo client
  // generates and submits the compositor-frame immediately. But it is possible
  // for the client to delay sending the compositor-frame. |args| includes the
  // deadline for the client before it needs to submit the compositor-frame.
  base::AutoLock lock(lock_);
  GetPtr()->SubmitCompositorFrame(local_surface_id_, CreateFrame(args),
                                  /*hit_test_region_list=*/std::nullopt,
                                  /*submit_time=*/0);
}
void DemoClient::OnBeginFramePausedChanged(bool paused) {}
void DemoClient::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {}

}  // namespace demo
