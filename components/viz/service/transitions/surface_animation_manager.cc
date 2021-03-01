// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/transitions/surface_animation_manager.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_saved_frame_storage.h"

namespace viz {
namespace {

CompositorRenderPassId NextRenderPassId(const CompositorRenderPassId& id) {
  return CompositorRenderPassId(id.GetUnsafeValue() + 1);
}

// TODO(vmpstr): This is here to make sure that we can compute the progress by
// dividing by duration. However, when we use the animation curves that don't
// rely on progress, this can be removed.
constexpr base::TimeDelta kMinimumAnimationDuration =
    base::TimeDelta::FromMilliseconds(1);

}  // namespace

SurfaceAnimationManager::SurfaceAnimationManager() = default;
SurfaceAnimationManager::~SurfaceAnimationManager() = default;

void SurfaceAnimationManager::SetDirectiveFinishedCallback(
    SurfaceSavedFrame::TransitionDirectiveCompleteCallback
        sequence_id_finished_callback) {
  sequence_id_finished_callback_ = std::move(sequence_id_finished_callback);
}

bool SurfaceAnimationManager::ProcessTransitionDirectives(
    base::TimeTicks last_frame_time,
    const std::vector<CompositorFrameTransitionDirective>& directives,
    SurfaceSavedFrameStorage* storage) {
  DCHECK_GE(last_frame_time, current_time_);
  current_time_ = last_frame_time;
  bool started_animation = false;
  for (auto& directive : directives) {
    // Don't process directives with sequence ids smaller than or equal to the
    // last seen one. It is possible that we call this with the same frame
    // multiple times.
    if (directive.sequence_id() <= last_processed_sequence_id_)
      continue;
    last_processed_sequence_id_ = directive.sequence_id();

    bool handled = false;
    // Dispatch to a specialized function based on type.
    switch (directive.type()) {
      case CompositorFrameTransitionDirective::Type::kSave:
        handled = ProcessSaveDirective(directive, storage);
        break;
      case CompositorFrameTransitionDirective::Type::kAnimate:
        handled = ProcessAnimateDirective(directive, storage);
        started_animation |= handled;
        break;
    }

    // If we didn't handle the directive, it means that we're in a state that
    // does not permit the directive to be processed, and it was ignored. We
    // should notify that we've fully processed the directive in this case to
    // allow code that is waiting for this to continue.
    if (!handled)
      sequence_id_finished_callback_.Run(directive.sequence_id());
  }
  return started_animation;
}

bool SurfaceAnimationManager::ProcessSaveDirective(
    const CompositorFrameTransitionDirective& directive,
    SurfaceSavedFrameStorage* storage) {
  // We need to be in the idle state in order to save.
  if (state_ != State::kIdle)
    return false;
  storage->ProcessSaveDirective(directive, sequence_id_finished_callback_);
  return true;
}

bool SurfaceAnimationManager::ProcessAnimateDirective(
    const CompositorFrameTransitionDirective& directive,
    SurfaceSavedFrameStorage* storage) {
  // We can only begin an animate if we are currently idle.
  if (state_ != State::kIdle)
    return false;

  // Make sure we don't actually have anything saved as a texture.
  DCHECK(!saved_root_texture_.has_value());

  auto saved_frame = storage->TakeSavedFrame();
  // We can't animate if we don't have a saved frame.
  if (!saved_frame || !saved_frame->IsValid())
    return false;

  // Convert the texture result into a transferable resource.
  save_directive_.emplace(saved_frame->directive());
  animate_directive_sequence_id_.emplace(directive.sequence_id());
  saved_root_texture_.emplace(
      transferable_resource_tracker_.ImportResource(std::move(saved_frame)));

  state_ = State::kAnimating;
  started_time_ = current_time_;
  return true;
}

bool SurfaceAnimationManager::NeedsBeginFrame() const {
  // If we're animating we need to keep pumping frames to advance the animation.
  // If we're done, we require one more frame to switch back to idle state.
  return state_ == State::kAnimating || state_ == State::kLastFrame;
}

void SurfaceAnimationManager::NotifyFrameAdvanced(base::TimeTicks new_time) {
  DCHECK_GE(new_time, current_time_);
  current_time_ = new_time;
  switch (state_) {
    case State::kIdle:
      NOTREACHED() << "We should not advance frames when idle";
      break;
    case State::kAnimating:
      FinishAnimationIfNeeded();
      break;
    case State::kLastFrame:
      FinalizeAndDisposeOfState();
      break;
  }
}

void SurfaceAnimationManager::FinishAnimationIfNeeded() {
  DCHECK_EQ(state_, State::kAnimating);
  DCHECK(saved_root_texture_.has_value());
  DCHECK(save_directive_.has_value());
  DCHECK(animate_directive_sequence_id_.has_value());
  if (current_time_ >= started_time_ + save_directive_->duration()) {
    state_ = State::kLastFrame;
    sequence_id_finished_callback_.Run(*animate_directive_sequence_id_);
  }
}

void SurfaceAnimationManager::FinalizeAndDisposeOfState() {
  DCHECK_EQ(state_, State::kLastFrame);
  DCHECK(saved_root_texture_.has_value());
  // Set state to idle.
  state_ = State::kIdle;

  // Ensure to return the texture / unref it.
  transferable_resource_tracker_.UnrefResource(saved_root_texture_->id);
  saved_root_texture_.reset();

  save_directive_.reset();
  animate_directive_sequence_id_.reset();

  started_time_ = base::TimeTicks();
}

double SurfaceAnimationManager::CalculateAnimationProgress() const {
  DCHECK(state_ == State::kAnimating || state_ == State::kLastFrame);
  if (state_ == State::kLastFrame)
    return 1.;

  DCHECK(save_directive_);
  base::TimeDelta duration = save_directive_->duration();
  if (duration < kMinimumAnimationDuration)
    duration = kMinimumAnimationDuration;

  double result = (current_time_ - started_time_) / duration;
  DCHECK_GE(result, 0.);
  DCHECK_LE(result, 1.);
  return result;
}

void SurfaceAnimationManager::InterpolateFrame(Surface* surface) {
  DCHECK(saved_root_texture_);
  if (state_ == State::kLastFrame) {
    surface->ResetInterpolatedFrame();
    return;
  }

  const auto& active_frame = surface->GetActiveFrame();

  CompositorFrame interpolated_frame;
  interpolated_frame.metadata = active_frame.metadata.Clone();
  interpolated_frame.resource_list = active_frame.resource_list;
  interpolated_frame.resource_list.push_back(*saved_root_texture_);
  CompositorRenderPassId max_id = CompositorRenderPassId(0);
  for (auto& render_pass : active_frame.render_pass_list) {
    if (render_pass->id > max_id)
      max_id = render_pass->id;
    interpolated_frame.render_pass_list.emplace_back(render_pass->DeepCopy());
  }

  gfx::Rect output_rect =
      interpolated_frame.render_pass_list.back()->output_rect;

  auto animation_pass = CompositorRenderPass::Create(2, 2);
  animation_pass->SetNew(NextRenderPassId(max_id), output_rect, output_rect,
                         gfx::Transform());

  float src_opacity = 1 - CalculateAnimationProgress();
  auto* src_quad_state = animation_pass->CreateAndAppendSharedQuadState();
  src_quad_state->SetAll(
      /*quad_to_target_transform=*/gfx::Transform(),
      /*quad_layer_rect=*/output_rect,
      /*visible_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false,
      /*are_contents_opaque=*/false,
      /*opacity=*/src_opacity,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  auto* src_quad = animation_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  float vertex_opacity[] = {1.f, 1.f, 1.f, 1.f};
  src_quad->SetNew(
      /*shared_quad_state=*/src_quad_state,
      /*rect=*/output_rect,
      /*visible_rect=*/output_rect,
      /*needs_blending=*/true,
      /*resource_id=*/saved_root_texture_->id,
      /*premultiplied_alpha=*/true,
      /*uv_top_left=*/gfx::PointF(0, 0),
      /*uv_bottom_right=*/gfx::PointF(1, 1),
      /*background_color=*/SK_ColorWHITE,
      /*vertex_opacity=*/vertex_opacity,
      /*y_flipped=*/true,
      /*nearest_neighbor=*/true,
      /*secure_output_only=*/false,
      /*protected_video_type=*/gfx::ProtectedVideoType::kClear);

  auto* dst_quad_state = animation_pass->CreateAndAppendSharedQuadState();
  dst_quad_state->SetAll(
      /*quad_to_target_transform=*/gfx::Transform(),
      /*quad_layer_rect=*/output_rect,
      /*visible_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false,
      /*are_contents_opaque=*/false,
      /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  auto* dst_quad =
      animation_pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  dst_quad->SetNew(
      /*shared_quad_state=*/dst_quad_state,
      /*rect=*/output_rect,
      /*visible_rect=*/output_rect,
      /*render_pass_id=*/interpolated_frame.render_pass_list.back()->id,
      /*mask_resource_id=*/kInvalidResourceId,
      /*mask_uv_rect=*/gfx::RectF(),
      /*mask_texture_size=*/gfx::Size(),
      /*filters_scale=*/gfx::Vector2dF(),
      /*filters_origin=*/gfx::PointF(),
      /*tex_coord_rect=*/gfx::RectF(output_rect),
      /*force_anti_aliasing_off=*/false,
      /*backdrop_filter_quality*/ 1.0f);

  interpolated_frame.render_pass_list.push_back(std::move(animation_pass));
  surface->SetInterpolatedFrame(std::move(interpolated_frame));
}

void SurfaceAnimationManager::RefResources(
    const std::vector<TransferableResource>& resources) {
  if (transferable_resource_tracker_.is_empty())
    return;
  for (const auto& resource : resources) {
    if (resource.id >= kVizReservedRangeStartId)
      transferable_resource_tracker_.RefResource(resource.id);
  }
}

void SurfaceAnimationManager::UnrefResources(
    const std::vector<ReturnedResource>& resources) {
  if (transferable_resource_tracker_.is_empty())
    return;
  for (const auto& resource : resources) {
    if (resource.id >= kVizReservedRangeStartId)
      transferable_resource_tracker_.UnrefResource(resource.id);
  }
}

}  // namespace viz
