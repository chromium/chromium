// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/transitions/surface_animation_manager.h"

#include <algorithm>
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
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/animation/keyframe/timing_function.h"

namespace viz {
namespace {

CompositorRenderPassId NextRenderPassId(const CompositorRenderPassId& id) {
  return CompositorRenderPassId(id.GetUnsafeValue() + 1);
}

constexpr base::TimeDelta kDefaultAnimationDuration =
    base::TimeDelta::FromMilliseconds(300);

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
                                   ResourceId id) {
  auto* src_quad_state = render_pass->CreateAndAppendSharedQuadState();
  src_quad_state->SetAll(
      /*quad_to_target_transform=*/src_transform,
      /*quad_layer_rect=*/output_rect,
      /*visible_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
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
      /*background_color=*/SK_ColorWHITE,
      /*vertex_opacity=*/vertex_opacity,
      /*y_flipped=*/true,
      /*nearest_neighbor=*/true,
      /*secure_output_only=*/false,
      /*protected_video_type=*/gfx::ProtectedVideoType::kClear);
}

}  // namespace

SurfaceAnimationManager::SurfaceAnimationManager() = default;
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

  UpdateAnimationCurves(saved_textures_->root.rect.size());
  state_ = State::kAnimating;
  return true;
}

bool SurfaceAnimationManager::NeedsBeginFrame() const {
  // If we're animating we need to keep pumping frames to advance the animation.
  // If we're done, we require one more frame to switch back to idle state.
  return animator_.IsAnimating() || state_ == State::kLastFrame;
}

void SurfaceAnimationManager::NotifyFrameAdvanced(base::TimeTicks new_time) {
  animator_.Tick(new_time);

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
  if (!animator_.IsAnimating()) {
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
  CompositorRenderPassId max_id = CompositorRenderPassId(0);
  for (auto& render_pass : active_frame.render_pass_list) {
    if (render_pass->id > max_id)
      max_id = render_pass->id;
    // TODO(vmpstr): If we are doing an interpolation, we fail requested copy
    // requests, since we can't copy them into an interpolated frame below. The
    // todo is to change DeepCopy into a function that copies everything and
    // moves copy output requests, since we can satisfy the requests on the
    // interpolated frame.
    render_pass->copy_requests.clear();
    interpolated_frame.render_pass_list.emplace_back(render_pass->DeepCopy());
  }

  gfx::Rect output_rect =
      interpolated_frame.render_pass_list.back()->output_rect;

  auto animation_pass = CompositorRenderPass::Create(2, 2);
  animation_pass->SetNew(NextRenderPassId(max_id), output_rect, output_rect,
                         gfx::Transform());

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

  gfx::Transform src_transform = src_transform_.Apply();
  gfx::Transform dst_transform = dst_transform_.Apply();

  if (src_on_top) {
    CreateAndAppendSrcTextureQuad(animation_pass.get(), output_rect,
                                  src_transform, src_opacity_,
                                  saved_textures_->root.resource.id);
  }

  auto* dst_quad_state = animation_pass->CreateAndAppendSharedQuadState();
  dst_quad_state->SetAll(
      /*quad_to_target_transform=*/dst_transform,
      /*quad_layer_rect=*/output_rect,
      /*visible_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false,
      /*are_contents_opaque=*/false,
      /*opacity=*/dst_opacity_,
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

  if (!src_on_top) {
    CreateAndAppendSrcTextureQuad(animation_pass.get(), output_rect,
                                  src_transform, src_opacity_,
                                  saved_textures_->root.resource.id);
  }

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
void SurfaceAnimationManager::OnFloatAnimated(
    const float& value,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == kDstOpacity) {
    dst_opacity_ = value;
  } else {
    src_opacity_ = value;
  }
}

void SurfaceAnimationManager::OnTransformAnimated(
    const gfx::TransformOperations& operations,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == kDstTransform) {
    dst_transform_ = operations;
  } else {
    src_transform_ = operations;
  }
}

void SurfaceAnimationManager::UpdateAnimationCurves(
    const gfx::Size& output_size) {
  // A small translation. We want to roughly scale this with screen size, but
  // we choose the minimum screen dimension to keep horizontal and vertical
  // transitions consistent and to avoid the impact of very oblong screen.
  const float delta = std::min(output_size.width(), output_size.height()) *
                      kTranslationProportion;

  gfx::TransformOperations start_transform;
  gfx::TransformOperations end_transform;
  int transform_property_id = kDstTransform;

  float start_opacity = 0.0f;
  float end_opacity = 1.0f;
  int opacity_property_id = kDstOpacity;

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
      transform_property_id = kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealRight: {
      end_transform.AppendTranslate(delta, 0.0f, 0.0f);
      transform_property_id = kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealUp: {
      end_transform.AppendTranslate(0.0f, -delta, 0.0f);
      transform_property_id = kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealDown: {
      end_transform.AppendTranslate(0.0f, delta, 0.0f);
      transform_property_id = kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = kSrcOpacity;
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
      transform_property_id = kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kFade: {
      // Fade is effectively an explode with no scaling.
      transform_property_id = kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kNone: {
      transform_property_id = kSrcTransform;
      start_opacity = end_opacity = 0.0f;
      opacity_property_id = kSrcOpacity;
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
  animator_.RemoveAllKeyframeModels();

  // We will use the ease in or ease out timing function (used by CSS
  // transitions) depending on whether the the new content is being covered or
  // revealed. If it's being covered, then we want to immediately start moving,
  // but ease into position, eg.
  std::unique_ptr<gfx::CubicBezierTimingFunction> timing_function =
      gfx::CubicBezierTimingFunction::CreatePreset(
          opacity_property_id == kSrcOpacity
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
  transform_curve->set_target(this);
  animator_.AddKeyframeModel(gfx::KeyframeModel::Create(
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
  float_curve->set_target(this);
  animator_.AddKeyframeModel(gfx::KeyframeModel::Create(
      std::move(float_curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
      opacity_property_id));

  // We should now have animations queued up.
  DCHECK(animator_.IsAnimating());

  // To ensure we don't flicker at the beginning of the animation, ensure that
  // our initial state is correct before we start ticking.
  src_opacity_ = 1.0f;
  dst_opacity_ = 1.0f;
  src_transform_ = gfx::TransformOperations();
  dst_transform_ = gfx::TransformOperations();
  OnTransformAnimated(start_transform, transform_property_id, nullptr);
  OnFloatAnimated(start_opacity, opacity_property_id, nullptr);
}

}  // namespace viz
