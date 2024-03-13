// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/synchronization/waitable_event.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/display/overlay_processor_on_gpu.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
OverlayProcessorAndroid::OverlayProcessorAndroid(
    DisplayCompositorMemoryAndTaskController* display_controller)
    : OverlayProcessorUsingStrategy(),
      gpu_task_scheduler_(display_controller->gpu_task_scheduler()) {
  // Promoting video to overlay with SurfaceView overlays requires recreation of
  // main SurfaceView and Display. This leads to the situation when we
  // consider video overlay not being efficient for the first frame after we
  // updated SurfaceView and video gets demoted back to composition. To avoid
  // this, we disable heuristics that filter out not efficient quads but still
  // sort them by potential power savings.
  prioritization_config_.changing_threshold = false;
  prioritization_config_.damage_rate_threshold = false;

  // In unittests, we don't have the gpu_task_scheduler_ set up, but still want
  // to test ProcessForOverlays functionalities where we are making overlay
  // candidates correctly.
  if (gpu_task_scheduler_) {
    gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
    // TODO(weiliangc): Eventually move the on gpu initialization to another
    // static function.
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    auto callback = base::BindOnce(
        &OverlayProcessorAndroid::InitializeOverlayProcessorOnGpu,
        base::Unretained(this), display_controller->controller_on_gpu(),
        &event);
    gpu_task_scheduler_->ScheduleGpuTask(std::move(callback), {});
    event.Wait();
  }

  // For Android, we do not have the ability to skip an overlay, since the
  // texture is already in a SurfaceView.  Ideally, we would honor a 'force
  // overlay' flag that FromDrawQuad would also check.
  // For now, though, just skip the opacity check.  We really have no idea if
  // the underlying overlay is opaque anyway; the candidate is referring to
  // a dummy resource that has no relation to what the overlay contains.
  // https://crbug.com/842931 .
  strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
      this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));

  overlay_candidates_.clear();
}

OverlayProcessorAndroid::~OverlayProcessorAndroid() {
  if (processor_on_gpu_) {
    gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
    // If we have a |gpu_task_scheduler_|, we must have started initializing
    // a |processor_on_gpu_| on the |gpu_task_scheduler_|.
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    auto callback =
        base::BindOnce(&OverlayProcessorAndroid::DestroyOverlayProcessorOnGpu,
                       base::Unretained(this), &event);
    gpu_task_scheduler_->ScheduleGpuTask(std::move(callback), {});
    event.Wait();
  }
}

void OverlayProcessorAndroid::InitializeOverlayProcessorOnGpu(
    gpu::DisplayCompositorMemoryAndTaskControllerOnGpu*
        display_controller_on_gpu,
    base::WaitableEvent* event) {
  processor_on_gpu_ =
      std::make_unique<OverlayProcessorOnGpu>(display_controller_on_gpu);
  DCHECK(event);
  event->Signal();
}

void OverlayProcessorAndroid::DestroyOverlayProcessorOnGpu(
    base::WaitableEvent* event) {
  processor_on_gpu_ = nullptr;
  DCHECK(event);
  event->Signal();
}

bool OverlayProcessorAndroid::IsOverlaySupported() const {
  return true;
}

bool OverlayProcessorAndroid::NeedsSurfaceDamageRectList() const {
  return false;
}

void OverlayProcessorAndroid::ScheduleOverlays(
    DisplayResourceProvider* resource_provider) {
  if (!processor_on_gpu_)
    return;

  // Even if we don't have anything to overlay, still generate overlay locks for
  // empty frame.
  pending_overlay_locks_.emplace_back();

  // Early out if we don't have any overlay candidates.
  if (overlay_candidates_.empty())
    return;

  auto& locks = pending_overlay_locks_.back();
  std::vector<gpu::SyncToken> locks_sync_tokens;
  for (auto& candidate : overlay_candidates_) {
    locks.emplace_back(resource_provider, candidate.resource_id);
    locks_sync_tokens.push_back(locks.back().sync_token());
  }

  auto task = base::BindOnce(&OverlayProcessorOnGpu::ScheduleOverlays,
                             base::Unretained(processor_on_gpu_.get()),
                             std::move(overlay_candidates_));
  gpu_task_scheduler_->ScheduleGpuTask(std::move(task), locks_sync_tokens);
  overlay_candidates_.clear();
}

void OverlayProcessorAndroid::OverlayPresentationComplete() {
  // If there is not |processor_on_gpu_| to send information to, we won't have
  // overlay resources locked, and don't need to clear the locks.
  if (!processor_on_gpu_)
    return;

  // This is a signal from Display::DidReceiveSwapBuffersAck. We use this to
  // help clear locks on resources from the old frame.
  committed_overlay_locks_.clear();
  std::swap(committed_overlay_locks_, pending_overlay_locks_.front());
  pending_overlay_locks_.pop_front();
}

void OverlayProcessorAndroid::CheckOverlaySupportImpl(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates) {
  // For pre-SurfaceControl Android we should not have output surface as overlay
  // plane.
  DCHECK(!primary_plane);

  // There should only be at most a single overlay candidate: the
  // video quad.
  // There's no check that the presented candidate is really a video frame for
  // a fullscreen video. Instead it's assumed that if a quad is marked as
  // overlayable, it's a fullscreen video quad.
  DCHECK_LE(candidates->size(), 1u);

  if (!candidates->empty()) {
    OverlayCandidate& candidate = candidates->front();

    // This quad either will be promoted, or would be if it were backed by a
    // SurfaceView.  Record that it should get a promotion hint.
    promotion_hint_info_map_[candidate.resource_id] = candidate.display_rect;

    if (!candidate.is_video_in_surface_view) {
      // This quad would be promoted if it were backed by a SurfaceView.  Since
      // it isn't, we can't promote it.
      return;
    }

    candidate.display_rect =
        gfx::RectF(gfx::ToEnclosingRect(candidate.display_rect));
    candidate.overlay_handled = true;
    candidate.plane_z_order = -1;

    // This quad will be promoted.  We clear the promotable hints here, since
    // we can only promote a single quad.  Otherwise, somebody might try to
    // back one of the promotable quads with a SurfaceView, and either it or
    // |candidate| would have to fall back to a texture.
    promotion_hint_info_map_.clear();
    promotion_hint_info_map_[candidate.resource_id] = candidate.display_rect;
  }
}

gfx::Rect OverlayProcessorAndroid::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& overlay) const {
  return ToEnclosedRect(overlay.display_rect);
}

void OverlayProcessorAndroid::TakeOverlayCandidates(
    OverlayCandidateList* candidate_list) {
  overlay_candidates_.swap(*candidate_list);
  candidate_list->clear();
}

void OverlayProcessorAndroid::NotifyOverlayPromotion(
    DisplayResourceProvider* resource_provider,
    const CandidateList& candidates,
    const QuadList& quad_list) {
  // If we don't have a processor_on_gpu_, there is nothing to send the overlay
  // promotions to.
  if (!processor_on_gpu_) {
    promotion_hint_info_map_.clear();
    return;
  }

  // Set of resources that have requested a promotion hint that also have quads
  // that use them.
  ResourceIdSet promotion_hint_requestor_set;

  for (auto* quad : quad_list) {
    if (quad->material != DrawQuad::Material::kTextureContent)
      continue;
    const TextureDrawQuad* texture_quad = TextureDrawQuad::MaterialCast(quad);
    if (!texture_quad->is_stream_video)
      continue;
    ResourceId id = texture_quad->resource_id();
    if (!resource_provider->DoesResourceWantPromotionHint(id))
      continue;
    promotion_hint_requestor_set.insert(id);
  }

  if (promotion_hint_requestor_set.empty()) {
    promotion_hint_info_map_.clear();
    return;
  }

  base::flat_set<gpu::Mailbox> promotion_denied;
  base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions;

  DCHECK(candidates.empty() || candidates.size() == 1u);

  std::vector<
      std::unique_ptr<DisplayResourceProvider::ScopedReadLockSharedImage>>
      locks;
  for (auto& request : promotion_hint_requestor_set) {
    // If we successfully promote one candidate, then that promotion hint
    // should be sent later when we schedule the overlay.
    if (!candidates.empty() && candidates.front().resource_id == request)
      continue;

    locks.emplace_back(
        std::make_unique<DisplayResourceProvider::ScopedReadLockSharedImage>(
            resource_provider, request));
    auto iter = promotion_hint_info_map_.find(request);
    if (iter != promotion_hint_info_map_.end()) {
      // This is a possible promotion.
      possible_promotions.emplace(locks.back()->mailbox(),
                                  gfx::ToEnclosedRect(iter->second));
    } else {
      promotion_denied.insert(locks.back()->mailbox());
    }
  }

  std::vector<gpu::SyncToken> locks_sync_tokens;
  for (auto& read_lock : locks)
    locks_sync_tokens.push_back(read_lock->sync_token());

  if (gpu_task_scheduler_) {
    auto task = base::BindOnce(&OverlayProcessorOnGpu::NotifyOverlayPromotions,
                               base::Unretained(processor_on_gpu_.get()),
                               std::move(promotion_denied),
                               std::move(possible_promotions));
    gpu_task_scheduler_->ScheduleGpuTask(std::move(task), locks_sync_tokens);
  }
  promotion_hint_info_map_.clear();
}

}  // namespace viz
