// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/transitions/surface_animation_manager.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/transition_utils.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_saved_frame_storage.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/animation/keyframe/timing_function.h"

namespace viz {
namespace {

constexpr base::TimeDelta kDefaultAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);

constexpr base::TimeDelta kSharedOpacityAnimationDuration =
    base::TimeDelta::FromMilliseconds(60);

constexpr base::TimeDelta kSharedOpacityAnimationDelay =
    base::TimeDelta::FromMilliseconds(60);

// Scale the overall duration to produce the opacity duration. Opacity
// transitions which reveal an element (i.e., transition opacity from 0 -> 1)
// should finish ahead of a translation. This way, you'll see the next page fade
// into view and settle while fully opaque. Similarly, transitions which hide an
// element (i.e., transition opacity from 1 -> 0) should, for a brief period,
// animate with full opacity so the user can get a sense of the motion before
// the element disappears.
constexpr float kOpacityTransitionDurationScaleFactor = 0.8f;

// When performing slides, the amount moved is proportional to the minimum
// viewport dimension -- this controls that proportion.
constexpr float kTranslationProportion = 0.05f;

// When performing implosions or explosions layers grow or shrink. This value
// determines the scaling done to achieve the larger of the two sizes.
constexpr float kScaleProportion = 1.1f;

void CreateAndAppendSrcTextureQuad(CompositorRenderPass* render_pass,
                                   const gfx::Rect& output_rect,
                                   const gfx::Transform& src_transform,
                                   float src_opacity,
                                   bool y_flipped,
                                   ResourceId id) {
  auto* src_quad_state = render_pass->CreateAndAppendSharedQuadState();
  src_quad_state->SetAll(
      /*quad_to_target_transform=*/src_transform,
      /*quad_layer_rect=*/output_rect,
      /*visible_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/absl::nullopt, /*are_contents_opaque=*/false,
      /*opacity=*/src_opacity,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  auto* src_quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  float vertex_opacity[] = {1.f, 1.f, 1.f, 1.f};
  src_quad->SetNew(
      /*shared_quad_state=*/src_quad_state,
      /*rect=*/output_rect,
      /*visible_rect=*/output_rect,
      /*needs_blending=*/true,
      /*resource_id=*/id,
      /*premultiplied_alpha=*/true,
      /*uv_top_left=*/gfx::PointF(0, 0),
      /*uv_bottom_right=*/gfx::PointF(1, 1),
      /*background_color=*/SK_ColorTRANSPARENT,
      /*vertex_opacity=*/vertex_opacity, y_flipped,
      /*nearest_neighbor=*/true,
      /*secure_output_only=*/false,
      /*protected_video_type=*/gfx::ProtectedVideoType::kClear);
}

void CreateAndAppendSharedRenderPassDrawQuad(
    CompositorRenderPass* render_pass,
    const gfx::Rect& rect,
    gfx::Transform transform,
    float opacity,
    CompositorRenderPassId render_pass_id,
    const CompositorRenderPassDrawQuad& sample_quad) {
  gfx::Transform rect_scale;
  // Convert the needed destination rect to be a scale on the sample quad rect.
  // This ensures that the rects match but are scaled, instead of being clipped.
  rect_scale.Scale(
      rect.width() / static_cast<float>(sample_quad.rect.width()),
      rect.height() / static_cast<float>(sample_quad.rect.height()));
  transform.PreconcatTransform(rect_scale);

  auto* quad_state = render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(
      /*quad_to_target_transform=*/transform,
      /*quad_layer_rect=*/rect,
      /*visible_layer_rect=*/rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/absl::nullopt,
      /*are_contents_opaque=*/false,
      /*opacity=*/opacity,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  auto* quad =
      render_pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  quad->SetNew(
      /*shared_quad_state=*/quad_state,
      /*rect=*/sample_quad.rect,
      /*visible_rect=*/sample_quad.visible_rect,
      /*render_pass_id=*/render_pass_id,
      /*mask_resource_id=*/sample_quad.mask_resource_id(),
      /*mask_uv_rect=*/sample_quad.mask_uv_rect,
      /*mask_texture_size=*/sample_quad.mask_texture_size,
      /*filters_scale=*/sample_quad.filters_scale,
      /*filters_origin=*/sample_quad.filters_origin,
      /*tex_coord_rect=*/sample_quad.tex_coord_rect,
      /*force_anti_aliasing_off=*/sample_quad.force_anti_aliasing_off,
      /*backdrop_filter_quality*/ sample_quad.backdrop_filter_quality);
}

}  // namespace

SurfaceAnimationManager::SurfaceAnimationManager(
    SharedBitmapManager* shared_bitmap_manager)
    : transferable_resource_tracker_(shared_bitmap_manager) {}

SurfaceAnimationManager::~SurfaceAnimationManager() = default;

void SurfaceAnimationManager::SetDirectiveFinishedCallback(
    TransitionDirectiveCompleteCallback sequence_id_finished_callback) {
  sequence_id_finished_callback_ = std::move(sequence_id_finished_callback);
}

bool SurfaceAnimationManager::ProcessTransitionDirectives(
    const std::vector<CompositorFrameTransitionDirective>& directives,
    SurfaceSavedFrameStorage* storage) {
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
  DCHECK(!saved_textures_.has_value());

  auto saved_frame = storage->TakeSavedFrame();
  // We can't animate if we don't have a saved frame.
  if (!saved_frame || !saved_frame->IsValid())
    return false;

  // Take the save directive.
  save_directive_.emplace(saved_frame->directive());
  animate_directive_.emplace(directive);

  // Import the saved frame, which converts it to a ResourceFrame -- a structure
  // which has transferable resources.
  saved_textures_.emplace(
      transferable_resource_tracker_.ImportResources(std::move(saved_frame)));

  CreateRootAnimationCurves(saved_textures_->root.draw_data.rect.size());
  CreateSharedElementCurves();
  TickAnimations(latest_time_);
  state_ = State::kAnimating;
  return true;
}

bool SurfaceAnimationManager::NeedsBeginFrame() const {
  // If we're animating we need to keep pumping frames to advance the animation.
  // If we're done, we require one more frame to switch back to idle state.
  return root_animation_.driver().IsAnimating() || state_ == State::kLastFrame;
}

void SurfaceAnimationManager::TickAnimations(base::TimeTicks new_time) {
  root_animation_.driver().Tick(new_time);
  for (auto& shared : shared_animations_)
    shared.driver().Tick(new_time);
}

void SurfaceAnimationManager::NotifyFrameAdvanced() {
  TickAnimations(latest_time_);

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
  DCHECK(saved_textures_.has_value());
  DCHECK(save_directive_.has_value());
  DCHECK(animate_directive_.has_value());
  if (!root_animation_.driver().IsAnimating()) {
    state_ = State::kLastFrame;
    sequence_id_finished_callback_.Run(animate_directive_->sequence_id());
  }
}

void SurfaceAnimationManager::FinalizeAndDisposeOfState() {
  DCHECK_EQ(state_, State::kLastFrame);
  DCHECK(saved_textures_.has_value());
  // Set state to idle.
  state_ = State::kIdle;

  // Ensure to return the texture / unref it.
  transferable_resource_tracker_.ReturnFrame(*saved_textures_);
  saved_textures_.reset();

  save_directive_.reset();
  animate_directive_.reset();
}

void SurfaceAnimationManager::InterpolateFrame(Surface* surface) {
  DCHECK(saved_textures_);
  if (state_ == State::kLastFrame) {
    surface->ResetInterpolatedFrame();
    return;
  }

  const auto& active_frame = surface->GetActiveFrame();

  CompositorFrame interpolated_frame;
  interpolated_frame.metadata = active_frame.metadata.Clone();
  interpolated_frame.resource_list = active_frame.resource_list;
  interpolated_frame.resource_list.push_back(saved_textures_->root.resource);

  gfx::Rect output_rect = active_frame.render_pass_list.back()->output_rect;
  auto animation_pass = CreateAnimationCompositorRenderPass(output_rect);

  CopyAndInterpolateSharedElements(active_frame.render_pass_list,
                                   animation_pass.get(), &interpolated_frame);

  bool src_on_top = false;
  switch (save_directive_->effect()) {
    case CompositorFrameTransitionDirective::Effect::kRevealRight:
    case CompositorFrameTransitionDirective::Effect::kRevealLeft:
    case CompositorFrameTransitionDirective::Effect::kRevealUp:
    case CompositorFrameTransitionDirective::Effect::kRevealDown:
    case CompositorFrameTransitionDirective::Effect::kExplode:
    case CompositorFrameTransitionDirective::Effect::kFade:
      src_on_top = true;
      break;
    default:
      break;
  }

  gfx::Transform src_transform = root_animation_.src_transform().Apply();
  gfx::Transform dst_transform = root_animation_.dst_transform().Apply();

  // GPU textures are flipped but software bitmaps are not.
  bool y_flipped = !saved_textures_->root.resource.is_software;

  if (src_on_top) {
    CreateAndAppendSrcTextureQuad(animation_pass.get(), output_rect,
                                  src_transform, root_animation_.src_opacity(),
                                  y_flipped, saved_textures_->root.resource.id);
  }

  auto* dst_quad_state = animation_pass->CreateAndAppendSharedQuadState();
  dst_quad_state->SetAll(
      /*quad_to_target_transform=*/dst_transform,
      /*quad_layer_rect=*/output_rect,
      /*visible_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/absl::nullopt,
      /*are_contents_opaque=*/false,
      /*opacity=*/root_animation_.dst_opacity(),
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  auto* dst_quad =
      animation_pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  dst_quad->SetNew(
      /*shared_quad_state=*/dst_quad_state,
      /*rect=*/output_rect,
      /*visible_rect=*/output_rect,
      /*render_pass_id=*/active_frame.render_pass_list.back()->id,
      /*mask_resource_id=*/kInvalidResourceId,
      /*mask_uv_rect=*/gfx::RectF(),
      /*mask_texture_size=*/gfx::Size(),
      /*filters_scale=*/gfx::Vector2dF(),
      /*filters_origin=*/gfx::PointF(),
      /*tex_coord_rect=*/gfx::RectF(output_rect),
      /*force_anti_aliasing_off=*/false,
      /*backdrop_filter_quality*/ 1.0f);

  if (!src_on_top) {
    CreateAndAppendSrcTextureQuad(animation_pass.get(), output_rect,
                                  src_transform, root_animation_.src_opacity(),
                                  y_flipped, saved_textures_->root.resource.id);
  }

  interpolated_frame.render_pass_list.push_back(std::move(animation_pass));
  surface->SetInterpolatedFrame(std::move(interpolated_frame));
}

std::unique_ptr<CompositorRenderPass>
SurfaceAnimationManager::CreateAnimationCompositorRenderPass(
    const gfx::Rect& output_rect) const {
  // One quad for the root render pass, and expect that each shared texture is
  // non-nullopt. Then we double it: 1 for the source frame, one for the
  // destination frame.
  size_t quad_size_limit = 2 * (1 + saved_textures_->shared.size());
  // Reserve the same number of shared quad states as there are quads, since we
  // expect each quad to have a separate shared quad state.
  auto animation_pass =
      CompositorRenderPass::Create(quad_size_limit, quad_size_limit);

  // We create an animation pass before copying the existing frame, since we'll
  // do a smart copy -- interpolating shared elements as we encounter them
  // during the copy. As a result, we use id 1 here, since we don't know what is
  // the maximum id yet, we'll update it after we do the copy.
  animation_pass->SetNew(CompositorRenderPassId(1), output_rect, output_rect,
                         gfx::Transform());
  return animation_pass;
}

void SurfaceAnimationManager::CopyAndInterpolateSharedElements(
    const std::vector<std::unique_ptr<CompositorRenderPass>>& source_passes,
    CompositorRenderPass* animation_pass,
    CompositorFrame* interpolated_frame) {
  // First create a placeholder for the shared render passes. We need this to
  // quickly check whether a compositor render pass is shared (i.e. do we need
  // to do something special for this).
  base::flat_map<CompositorRenderPassId, RenderPassDrawData> shared_draw_data;
  for (const CompositorRenderPassId& shared_render_pass_id :
       animate_directive_->shared_render_pass_ids()) {
    shared_draw_data.emplace(shared_render_pass_id, RenderPassDrawData());
  }

  // Now run through all the source passes, making a 'smart' copy and filtering
  // based on whether the pass is a shared element or not. After this loop, we
  // should have populated `shared_render_passes` with the shared passes, and
  // `animation_draw_quads` with draw quads for those render passes. All other
  // passes would have been copied and added into the interpolated frame.
  CompositorRenderPassId max_id = CompositorRenderPassId(0);
  TransitionUtils::FilterCallback filter_callback = base::BindRepeating(
      &FilterSharedElementQuads, base::Unretained(&shared_draw_data));
  for (auto& render_pass : source_passes) {
    // First, clear the copy requests.
    // TODO(vmpstr): Can we preserve these in some situations?
    render_pass->copy_requests.clear();

    // Get the max_id for any render pass, since we'll need to create new passes
    // with ids that don't conflict with existing passes.
    if (render_pass->id > max_id)
      max_id = render_pass->id;

    // Now do the pass copy, filtering shared element quads into
    // `shared_draw_data` instead of the render pass.
    auto pass_copy = TransitionUtils::CopyPassWithRenderPassFiltering(
        *render_pass, filter_callback);

    // If this is a shared pass, store it in `shared_draw_data`. Otherwise,
    // put it directly into the interpolated frame since we don't need to do
    // anything special with it.
    auto shared_it = shared_draw_data.find(pass_copy->id);
    if (shared_it != shared_draw_data.end()) {
      RenderPassDrawData& data = shared_it->second;
      data.render_pass = std::move(pass_copy);
      data.opacity = TransitionUtils::ComputeAccumulatedOpacity(
          source_passes, data.render_pass->id);
    } else {
      interpolated_frame->render_pass_list.emplace_back(std::move(pass_copy));
    }
  }

  // Update the animation pass id to avoid conflicts.
  max_id = animation_pass->id = TransitionUtils::NextRenderPassId(max_id);

  const std::vector<CompositorRenderPassId>& shared_render_pass_ids =
      animate_directive_->shared_render_pass_ids();
  for (size_t i = 0; i < shared_render_pass_ids.size(); ++i) {
    const CompositorRenderPassId& shared_pass_id = shared_render_pass_ids[i];
    AnimationState& animation = shared_animations_[i];
    auto& draw_data = shared_draw_data[shared_pass_id];

    bool has_destination_pass = draw_data.render_pass && draw_data.draw_quad;

    // We have to retarget the animations, whether or not we have a destination
    // pass.

    // If we have a rect model, it means we have a source texture because we
    // would have only created the rect model if we had a source texture (see
    // `CreateSharedElementCurves()`). If the destination also exists, we have
    // to target it since that's our target. If it isn't there, but was there at
    // some point, then we have to retarget the animmation to its current value
    // so that the animation stops moving.
    //
    // If we don't have a
    // rect_model, meaning that we don't have a source texture, we update the
    // state to whatever the destination rect is directly on the animation.
    gfx::KeyframeModel* rect_model =
        animation.driver().GetKeyframeModel(AnimationState::kRect);
    const gfx::Rect target_rect =
        has_destination_pass ? draw_data.draw_quad->rect : animation.rect();
    if (rect_model) {
      rect_model->Retarget(latest_time_, AnimationState::kRect, target_rect);
    } else {
      animation.OnRectAnimated(target_rect, AnimationState::kRect, nullptr);
    }

    // Now do the same for transform animation.
    gfx::KeyframeModel* transform_model =
        animation.driver().GetKeyframeModel(AnimationState::kSrcTransform);
    gfx::TransformOperations target_transform_ops;
    target_transform_ops.AppendMatrix(
        has_destination_pass ? draw_data.render_pass->transform_to_root_target
                             : animation.src_transform().Apply());
    if (transform_model) {
      transform_model->Retarget(latest_time_, AnimationState::kSrcTransform,
                                target_transform_ops);
    } else {
      animation.OnTransformAnimated(target_transform_ops,
                                    AnimationState::kSrcTransform, nullptr);
    }

    // Now that we have updated the animations, we can append the interpolations
    // to the animation pass.
    const absl::optional<TransferableResourceTracker::PositionedResource>&
        src_texture = saved_textures_->shared[i];

    const gfx::Rect& rect = animation.rect();
    const gfx::Transform& transform = animation.src_transform().Apply();
    float opacity = animation.src_opacity();

    if (src_texture.has_value()) {
      bool y_flipped = !src_texture->resource.is_software;
      CreateAndAppendSrcTextureQuad(animation_pass, rect, transform,
                                    opacity * src_texture->draw_data.opacity,
                                    y_flipped, src_texture->resource.id);
      interpolated_frame->resource_list.push_back(src_texture->resource);
    }

    if (!has_destination_pass)
      continue;

    // Now that we know we have a render pass destination, create a copy of the
    // shared render pass, and update it with all the right values.
    auto pass_copy = draw_data.render_pass->DeepCopy();
    max_id = pass_copy->id = TransitionUtils::NextRenderPassId(max_id);
    pass_copy->transform_to_root_target = transform;

    // Create an quad for the pass into our animation pass.
    // TODO(vmpstr): This needs to be a more sophisticated blending. See
    // crbug.com/1201251 for details.
    CreateAndAppendSharedRenderPassDrawQuad(
        animation_pass, rect, transform, draw_data.opacity * (1.f - opacity),
        pass_copy->id, *draw_data.draw_quad);

    // Finally, add the pass into the interpolated frame. Make sure this comes
    // after CreateAndAppend* call, because we use a pass id, so we need to
    // access the pass before moving it here.
    interpolated_frame->render_pass_list.emplace_back(std::move(pass_copy));
  }
}
// static
bool SurfaceAnimationManager::FilterSharedElementQuads(
    base::flat_map<CompositorRenderPassId, RenderPassDrawData>*
        shared_draw_data,
    const CompositorRenderPassDrawQuad& pass_quad,
    CompositorRenderPass& copy_pass) {
  auto shared_it = shared_draw_data->find(pass_quad.render_pass_id);
  // If the quad is shared, then add it to the `shared_draw_data`.
  // Otherwise, it will be added to the copy pass directly.
  if (shared_it != shared_draw_data->end()) {
    shared_it->second.draw_quad = pass_quad;
    return true;
  }
  return false;
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
      transferable_resource_tracker_.UnrefResource(resource.id, resource.count);
  }
}

void SurfaceAnimationManager::UpdateFrameTime(base::TimeTicks now) {
  latest_time_ = now;
}

void SurfaceAnimationManager::CreateRootAnimationCurves(
    const gfx::Size& output_size) {
  // A small translation. We want to roughly scale this with screen size, but
  // we choose the minimum screen dimension to keep horizontal and vertical
  // transitions consistent and to avoid the impact of very oblong screen.
  const float delta = std::min(output_size.width(), output_size.height()) *
                      kTranslationProportion;

  gfx::TransformOperations start_transform;
  gfx::TransformOperations end_transform;
  int transform_property_id = AnimationState::kDstTransform;

  float start_opacity = 0.0f;
  float end_opacity = 1.0f;
  int opacity_property_id = AnimationState::kDstOpacity;

  DCHECK(save_directive_.has_value());

  switch (save_directive_->effect()) {
    case CompositorFrameTransitionDirective::Effect::kCoverLeft: {
      start_transform.AppendTranslate(delta, 0.0f, 0.0f);
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kCoverRight: {
      start_transform.AppendTranslate(-delta, 0.0f, 0.0f);
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kCoverUp: {
      start_transform.AppendTranslate(0.0f, delta, 0.0f);
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kCoverDown: {
      start_transform.AppendTranslate(0.0f, -delta, 0.0f);
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealLeft: {
      end_transform.AppendTranslate(-delta, 0.0f, 0.0f);
      transform_property_id = AnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = AnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealRight: {
      end_transform.AppendTranslate(delta, 0.0f, 0.0f);
      transform_property_id = AnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = AnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealUp: {
      end_transform.AppendTranslate(0.0f, -delta, 0.0f);
      transform_property_id = AnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = AnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealDown: {
      end_transform.AppendTranslate(0.0f, delta, 0.0f);
      transform_property_id = AnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = AnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kExplode: {
      start_transform.AppendTranslate(output_size.width() * 0.5f,
                                      output_size.height() * 0.5f, 0.0f);
      start_transform.AppendScale(1.0f, 1.0f, 1.0f);
      start_transform.AppendTranslate(-output_size.width() * 0.5f,
                                      -output_size.height() * 0.5f, 0.0f);

      end_transform.AppendTranslate(output_size.width() * 0.5f,
                                    output_size.height() * 0.5f, 0.0f);
      end_transform.AppendScale(kScaleProportion, kScaleProportion, 1.0f);
      end_transform.AppendTranslate(-output_size.width() * 0.5f,
                                    -output_size.height() * 0.5f, 0.0f);
      transform_property_id = AnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = AnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kFade: {
      // Fade is effectively an explode with no scaling.
      transform_property_id = AnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = AnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kNone: {
      transform_property_id = AnimationState::kSrcTransform;
      start_opacity = end_opacity = 0.0f;
      opacity_property_id = AnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kImplode: {
      start_transform.AppendTranslate(output_size.width() * 0.5f,
                                      output_size.height() * 0.5f, 0.0f);
      start_transform.AppendScale(kScaleProportion, kScaleProportion, 1.0f);
      start_transform.AppendTranslate(-output_size.width() * 0.5f,
                                      -output_size.height() * 0.5f, 0.0f);

      end_transform.AppendTranslate(output_size.width() * 0.5f,
                                    output_size.height() * 0.5f, 0.0f);
      end_transform.AppendScale(1.0f, 1.0f, 1.0f);
      end_transform.AppendTranslate(-output_size.width() * 0.5f,
                                    -output_size.height() * 0.5f, 0.0f);
      break;
    }
  }

  // Ensure we have no conflicting animation.
  root_animation_.driver().RemoveAllKeyframeModels();

  // We will use the ease in or ease out timing function (used by CSS
  // transitions) depending on whether the the new content is being covered or
  // revealed. If it's being covered, then we want to immediately start moving,
  // but ease into position, eg.
  std::unique_ptr<gfx::CubicBezierTimingFunction> timing_function =
      gfx::CubicBezierTimingFunction::CreatePreset(
          opacity_property_id == AnimationState::kSrcOpacity
              ? gfx::CubicBezierTimingFunction::EaseType::EASE_IN
              : gfx::CubicBezierTimingFunction::EaseType::EASE_OUT);

  // Create the transform curve.
  base::TimeDelta transform_duration = kDefaultAnimationDuration;

  std::unique_ptr<gfx::KeyframedTransformAnimationCurve> transform_curve(
      gfx::KeyframedTransformAnimationCurve::Create());
  transform_curve->AddKeyframe(gfx::TransformKeyframe::Create(
      base::TimeDelta(), start_transform, timing_function->Clone()));
  transform_curve->AddKeyframe(gfx::TransformKeyframe::Create(
      transform_duration, end_transform, timing_function->Clone()));
  transform_curve->set_target(&root_animation_);
  root_animation_.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
      std::move(transform_curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
      transform_property_id));

  // Create the opacity curve. Somewhat more complicated because it may be
  // delayed wrt to the transform curve. See description of
  // |kOpacityTransitionDurationScaleFactor| above.
  base::TimeDelta opacity_duration =
      transform_duration * kOpacityTransitionDurationScaleFactor;
  base::TimeDelta opacity_delay = start_opacity == 0.0f
                                      ? base::TimeDelta()
                                      : transform_duration - opacity_duration;

  // Opacity transitions do not need to ease in or out. By passing nullptr for
  // the timing function here, we are choosing the "linear" timing function.
  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> float_curve(
      gfx::KeyframedFloatAnimationCurve::Create());
  if (!opacity_delay.is_zero()) {
    float_curve->AddKeyframe(
        gfx::FloatKeyframe::Create(base::TimeDelta(), start_opacity, nullptr));
  }
  float_curve->AddKeyframe(
      gfx::FloatKeyframe::Create(opacity_delay, start_opacity, nullptr));
  float_curve->AddKeyframe(
      gfx::FloatKeyframe::Create(opacity_duration, end_opacity, nullptr));
  float_curve->set_target(&root_animation_);
  root_animation_.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
      std::move(float_curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
      opacity_property_id));

  // We should now have animations queued up.
  DCHECK(root_animation_.driver().IsAnimating());

  // To ensure we don't flicker at the beginning of the animation, ensure that
  // our initial state is correct before we start ticking.
  root_animation_.Reset();
  root_animation_.OnTransformAnimated(start_transform, transform_property_id,
                                      nullptr);
  root_animation_.OnFloatAnimated(start_opacity, opacity_property_id, nullptr);
}

void SurfaceAnimationManager::CreateSharedElementCurves() {
  DCHECK(animate_directive_.has_value());
  // Clear and resize, to reset the shared animations state if any.
  shared_animations_.clear();
  shared_animations_.resize(
      animate_directive_->shared_render_pass_ids().size());

  // Since we don't have a target state yet, create animations as if all of the
  // shared elements are targeted to stay in place with opacity going to 0.
  for (size_t i = 0; i < saved_textures_->shared.size(); ++i) {
    auto& shared = saved_textures_->shared[i];
    auto& state = shared_animations_[i];

    // Opacity goes from 1 to 0 linearly on the source.
    float start_opacity = 1.f;
    float end_opacity = 0.f;

    auto float_curve = gfx::KeyframedFloatAnimationCurve::Create();
    float_curve->set_target(&state);

    // The curve starts at opacity delay and runs for opacity animation, so it
    // potentially has 4 points:
    // time 0 == start opacity
    // time 'delay' == start opacity
    // time 'delay' + 'duration' == end opacity
    // time end of animation == end opacity
    float_curve->AddKeyframe(
        gfx::FloatKeyframe::Create(base::TimeDelta(), start_opacity, nullptr));
    if (!kSharedOpacityAnimationDelay.is_zero()) {
      float_curve->AddKeyframe(gfx::FloatKeyframe::Create(
          kSharedOpacityAnimationDelay, start_opacity, nullptr));
    }
    float_curve->AddKeyframe(gfx::FloatKeyframe::Create(
        kSharedOpacityAnimationDuration + kSharedOpacityAnimationDelay,
        end_opacity, nullptr));
    float_curve->AddKeyframe(gfx::FloatKeyframe::Create(
        kDefaultAnimationDuration, end_opacity, nullptr));

    state.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
        std::move(float_curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
        AnimationState::kSrcOpacity));

    // If we don't have a source, we will always use the destination
    // rect/transform, so don't create the animation curves for those.
    if (!shared.has_value())
      continue;

    // Set transform value to be the same at the start and end; we will
    // re-target the end transform when we update the curves for a given
    // compositor frame if needed.
    // The specific timing function is fine tuned for the effect.
    auto ease_timing =
        gfx::CubicBezierTimingFunction::Create(0.4, 0.0, 0.2, 1.0);

    gfx::TransformOperations transform_ops;
    transform_ops.AppendMatrix(shared->draw_data.target_transform);

    auto transform_curve = gfx::KeyframedTransformAnimationCurve::Create();
    transform_curve->set_target(&state);
    transform_curve->AddKeyframe(gfx::TransformKeyframe::Create(
        base::TimeDelta(), transform_ops, ease_timing->Clone()));
    transform_curve->AddKeyframe(gfx::TransformKeyframe::Create(
        kDefaultAnimationDuration, transform_ops, ease_timing->Clone()));
    // Note that src and dst share the transform, but we use src value here.
    state.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
        std::move(transform_curve),
        gfx::KeyframeEffect::GetNextKeyframeModelId(),
        AnimationState::kSrcTransform));

    const gfx::Rect& rect = shared->draw_data.rect;

    auto rect_curve = gfx::KeyframedRectAnimationCurve::Create();
    rect_curve->set_target(&state);
    rect_curve->AddKeyframe(gfx::RectKeyframe::Create(base::TimeDelta(), rect,
                                                      ease_timing->Clone()));
    rect_curve->AddKeyframe(gfx::RectKeyframe::Create(
        kDefaultAnimationDuration, rect, ease_timing->Clone()));
    // Note that src and dst share the rect, but we use src value here.
    state.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
        std::move(rect_curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
        AnimationState::kRect));
  }
}

// AnimationState
SurfaceAnimationManager::AnimationState::AnimationState() = default;
SurfaceAnimationManager::AnimationState::AnimationState(AnimationState&&) =
    default;
SurfaceAnimationManager::AnimationState::~AnimationState() = default;

void SurfaceAnimationManager::AnimationState::OnFloatAnimated(
    const float& value,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == kDstOpacity) {
    dst_opacity_ = value;
  } else {
    src_opacity_ = value;
  }
}

void SurfaceAnimationManager::AnimationState::OnTransformAnimated(
    const gfx::TransformOperations& operations,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == kDstTransform) {
    dst_transform_ = operations;
  } else {
    src_transform_ = operations;
  }
}

void SurfaceAnimationManager::AnimationState::OnRectAnimated(
    const gfx::Rect& value,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  DCHECK_EQ(target_property_id, kRect);
  rect_ = value;
}

void SurfaceAnimationManager::AnimationState::Reset() {
  src_opacity_ = 1.0f;
  dst_opacity_ = 1.0f;
  src_transform_ = gfx::TransformOperations();
  dst_transform_ = gfx::TransformOperations();
  rect_ = gfx::Rect();
}

SurfaceAnimationManager::RenderPassDrawData::RenderPassDrawData() = default;
SurfaceAnimationManager::RenderPassDrawData::RenderPassDrawData(
    RenderPassDrawData&&) = default;
SurfaceAnimationManager::RenderPassDrawData::~RenderPassDrawData() = default;

}  // namespace viz
