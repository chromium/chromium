// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/eviction_handler.h"

#include <GLES2/gl2.h>

#include <utility>

#include "base/functional/callback_helpers.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/gfx/video_types.h"

namespace viz {

EvictionHandler::EvictionHandler(Display* display,
                                 CompositorFrameSinkSupport* support,
                                 ReservedResourceIdTracker* id_tracker)
    : display_(display), support_(support), id_tracker_(id_tracker) {
  support_->SetExternalReservedResourceDelegate(this);
}

EvictionHandler::~EvictionHandler() {
  support_->SetExternalReservedResourceDelegate(nullptr);
}

bool EvictionHandler::WillEvictSurface(const SurfaceId& surface_id) {
  const SurfaceId& current_surface_id = display_->CurrentSurfaceId();
  if (!current_surface_id.is_valid()) {
    return true;  // Okay to evict immediately.
  }
  DCHECK_EQ(surface_id.frame_sink_id(), current_surface_id.frame_sink_id());
  // We should only evict when the display is not visible. If the display is
  // visible, then we likely received two evictions for the root surface at the
  // same time, which should not happen.
  CHECK(!display_->visible())
      << "Expected display to be not visible - in_progress_: " << in_progress_;
  DCHECK(display_->has_scheduler());

  // This matches CompositorFrameSinkSupport's eviction logic, which will
  // evict `surface_id` or matching but older ones. Avoid overwriting the
  // contents of `current_surface_id` if it's newer here by doing the same
  // check.
  if (surface_id.local_surface_id().parent_sequence_number() >=
      current_surface_id.local_surface_id().parent_sequence_number()) {
    auto snapshot_scale = features::SnapshotEvictedRootSurfaceScale();

    in_progress_ = true;
    if (snapshot_scale.has_value()) {
      TakeSnapshotForEviction(surface_id, *snapshot_scale);
    } else {
      SubmitPlaceholderContentForEviction(surface_id, snapshot_seq_id_,
                                          /*copy_result=*/nullptr);
    }

    // Don't evict immediately.
    // Delay eviction until the next draw to make sure that the draw is
    // successful (requires the surface not to be evicted). We need the draw (of
    // an empty CF) to be successful to push out and free resources.
    return false;
  }
  return true;  // Okay to evict immediately.
}

void EvictionHandler::MaybeFinishEvictionProcess() {
  // We don't modify `copy_output_results_` here, because they may still be in
  // use. They will be freed when no longer used, so just prevent eviction for
  // the root surface.
  to_evict_on_next_draw_and_swap_ = LocalSurfaceId();
  in_progress_ = false;
}

void EvictionHandler::DisplayDidDrawAndSwap() {
  if (!in_progress_) {
    return;
  }

  if (to_evict_on_next_draw_and_swap_.is_valid()) {
    display_->SetVisible(false);
    display_->InvalidateCurrentSurfaceId();

    support_->EvictSurface(to_evict_on_next_draw_and_swap_);

    // Trigger garbage collection immediately, otherwise the surface may not be
    // evicted for a long time (e.g. not before a frame is produced).
    support_->GarbageCollectSurfaces();

    MaybeFinishEvictionProcess();
    // We will get two unref calls and destroy `copy_output_results_` in
    // `UnrefResources`.
  }
}

void EvictionHandler::TakeSnapshotForEviction(const SurfaceId& to_evict,
                                              double scale) {
  snapshot_seq_id_++;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kNativeTextures,
      base::BindOnce(&EvictionHandler::SubmitPlaceholderContentForEviction,
                     weak_factory_.GetWeakPtr(), to_evict, snapshot_seq_id_));

  auto current_surface_id = display_->CurrentSurfaceId();
  auto* surface =
      support_->frame_sink_manager()->surface_manager()->GetSurfaceForId(
          current_surface_id);

  auto src_rect = surface->size_in_pixels();
  auto dst_rect = gfx::ScaleToRoundedSize(src_rect, scale);
  request->SetScaleRatio(gfx::Vector2d(src_rect.width(), src_rect.height()),
                         gfx::Vector2d(dst_rect.width(), dst_rect.height()));

  // Run result callback on the current thread in case `callback` needs to run
  // on the current thread.
  request->set_result_task_runner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // `to_evict` may be newer and not have arrived yet. Snapshot
  // `current_surface_id` which definitely exists at this point.
  support_->frame_sink_manager()->RequestCopyOfOutput(
      current_surface_id, std::move(request),
      /*capture_exact_surface_id=*/true);

  // Force a draw so the copy request completes.
  display_->SetVisible(true);
  display_->ForceImmediateDrawAndSwapIfPossible();
}

void EvictionHandler::SubmitPlaceholderContentForEviction(
    SurfaceId to_evict,
    int64_t snapshot_seq_id,
    std::unique_ptr<CopyOutputResult> copy_result) {
  if (!in_progress_ || snapshot_seq_id != snapshot_seq_id_) {
    return;
  }
  // Push replacement compositor frame to root surface. This is so the resources
  // can be unreffed from both viz and the OS compositor (if required).
  CompositorFrame frame;

  auto& metadata = frame.metadata;
  metadata.frame_token = kLocalFrameToken;

  auto current_surface_id = display_->CurrentSurfaceId();
  auto* surface =
      support_->frame_sink_manager()->surface_manager()->GetSurfaceForId(
          current_surface_id);
  CHECK(surface);
  metadata.device_scale_factor = surface->device_scale_factor();
  frame.metadata.begin_frame_ack = BeginFrameAck::CreateManualAckWithDamage();

  frame.render_pass_list.push_back(CompositorRenderPass::Create());
  auto* render_pass = frame.render_pass_list.back().get();

  const CompositorRenderPassId kRenderPassId{1};
  auto surface_rect = gfx::Rect(surface->size_in_pixels());
  DCHECK(!surface_rect.IsEmpty());
  render_pass->SetNew(kRenderPassId, /*output_rect=*/
                      surface_rect,
                      /*damage_rect=*/surface_rect, gfx::Transform());

  SharedQuadState* quad_state = render_pass->CreateAndAppendSharedQuadState();

  quad_state->SetAll(gfx::Transform(), /*layer_rect=*/surface_rect,
                     /*visible_layer_rect=*/surface_rect,
                     /*filter_info=*/gfx::MaskFilterInfo(),
                     /*clip=*/std::nullopt,
                     /*contents_opaque=*/true, /*opacity_f=*/1.f,
                     /*blend=*/SkBlendMode::kSrcOver, /*sorting_context=*/0,
                     /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  // TODO(edcourtney): Handle this for software rendering, where there is no
  // texture result but a SkBitmap instead.
  if (copy_result && !copy_result->IsEmpty() &&
      copy_result->GetTextureResult()) {
    auto resource = TransferableResource::MakeGpu(
        copy_result->GetTextureResult()->mailbox, GL_TEXTURE_2D,
        gpu::SyncToken(), copy_result->size(), SinglePlaneFormat::kRGBA_8888,
        /*is_overlay_candidate=*/false,
        TransferableResource::ResourceSource::kStaleContent);

    // The first ref will come from `ReceiveFromChild`.
    resource.id = id_tracker_->AllocId(
        /*initial_ref_count=*/0);

    // When we submit the compositor frame containing this, the resource will
    // be reffed until it is no longer needed.
    frame.resource_list.push_back(resource);

    TextureDrawQuad* texture_quad =
        render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    texture_quad->SetNew(
        quad_state, surface_rect, surface_rect,
        /*needs_blending=*/false, resource.id,
        /*premultiplied=*/false, /*top_left=*/gfx::PointF(0.0, 0.0),
        /*bottom_right=*/gfx::PointF(1.0, 1.0),
        /*background=*/SkColors::kBlack,
        /*flipped=*/false,
        /*nearest=*/false,
        /*secure_output=*/false, gfx::ProtectedVideoType::kClear);

    // It's possible that if the eviction process is cancelled and then started
    // again quickly, the previous copy request may still be in use.
    DCHECK(!base::Contains(copy_output_results_, resource.id));
    copy_output_results_[resource.id] = std::move(copy_result);
  } else {
    SolidColorDrawQuad* solid_quad =
        render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    solid_quad->SetNew(quad_state, surface_rect, surface_rect, SkColors::kWhite,
                       /*anti_aliasing_off=*/false);
  }

  support_->SubmitCompositorFrameLocally(current_surface_id, std::move(frame),
                                         display_->settings());

  // Complete the eviction on next draw and swap.
  to_evict_on_next_draw_and_swap_ = to_evict.local_surface_id();
  display_->SetVisible(true);
  display_->ForceImmediateDrawAndSwapIfPossible();
}

void EvictionHandler::ReceiveFromChild(
    const std::vector<TransferableResource>& resources) {
  for (const auto& resource : resources) {
    if (copy_output_results_.contains(resource.id)) {
      id_tracker_->RefId(resource.id, /*count=*/1);
    }
  }
}

void EvictionHandler::RefResources(
    const std::vector<TransferableResource>& resources) {
  for (const auto& resource : resources) {
    if (copy_output_results_.contains(resource.id)) {
      id_tracker_->RefId(resource.id, /*count=*/1);
    }
  }
}

void EvictionHandler::UnrefResources(
    const std::vector<ReturnedResource>& resources) {
  for (const auto& resource : resources) {
    if (copy_output_results_.contains(resource.id)) {
      // There are no further references, destroy the `CopyOutputRequest`s which
      // will call their release callbacks.
      if (id_tracker_->UnrefId(resource.id, /*count=*/1)) {
        copy_output_results_.erase(resource.id);
      }
    }
  }
}

}  // namespace viz
