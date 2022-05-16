// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/transitions/surface_animation_manager.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/transition_utils.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_saved_frame_storage.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_operations.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {
namespace {

// Scale the overall duration to produce the opacity duration. Opacity
// transitions which reveal an element (i.e., transition opacity from 0 -> 1)
// should finish ahead of a translation. This way, you'll see the next page fade
// into view and settle while fully opaque. Similarly, transitions which hide an
// element (i.e., transition opacity from 1 -> 0) should, for a brief period,
// animate with full opacity so the user can get a sense of the motion before
// the element disappears.
constexpr float kOpacityTransitionDurationScaleFactor = 0.8f;

// When transitioning two elements we perform a set of animations :
// a) An interpolation between the size of the elements.
// b) A cross-fade between the pixel content of the elements, which includes
//    opacity inherited from ancestors.
// c) A transform animation to interpolate the screen space transform of the
//    elements.
// The animation at b) starts at a delay and runs for a duration proportional to
// the total duration of the animation. The following constants define that
// proportion.
constexpr float kSharedOpacityTransitionDurationScaleFactor = 0.24f;
constexpr float kSharedOpacityTransitionDelayScaleFactor = 0.24f;

// When performing slides, the amount moved is proportional to the minimum
// viewport dimension -- this controls that proportion.
constexpr float kTranslationProportion = 0.05f;

// When performing implosions or explosions layers grow or shrink. This value
// determines the scaling done to achieve the larger of the two sizes.
constexpr float kScaleProportion = 1.1f;

void CreateAndAppendSrcTextureQuad(CompositorRenderPass* render_pass,
                                   const gfx::Rect& output_rect,
                                   const gfx::Transform& src_transform,
                                   SkBlendMode blend_mode,
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
      /*blend_mode=*/blend_mode, /*sorting_context_id=*/0);

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
      /*nearest_neighbor=*/false,
      /*secure_output_only=*/false,
      /*protected_video_type=*/gfx::ProtectedVideoType::kClear);
}

void CreateAndAppendSharedRenderPassDrawQuad(
    CompositorRenderPass* render_pass,
    const gfx::Rect& rect,
    gfx::Transform transform,
    float opacity,
    CompositorRenderPassId render_pass_id,
    SkBlendMode blend_mode,
    const CompositorRenderPassDrawQuad& sample_quad) {
  auto* quad_state = render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(
      /*quad_to_target_transform=*/transform,
      /*quad_layer_rect=*/rect,
      /*visible_layer_rect=*/rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/absl::nullopt,
      /*are_contents_opaque=*/false,
      /*opacity=*/opacity,
      /*blend_mode=*/blend_mode, /*sorting_context_id=*/0);

  auto* quad =
      render_pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  quad->SetNew(
      /*shared_quad_state=*/quad_state,
      /*rect=*/rect,
      /*visible_rect=*/rect,
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

// This function swaps a SharedElementDrawQuad with a RenderPassDrawQuad.
// |target_render_pass| is the render pass where the SharedElementDrawQuad is
// drawn.
// |shared_element_quad| is the quad providing the geometry to draw this shared
// element's content.
// |shared_element_content_pass| is the render pass which provides the content
// for this shared element.
void ReplaceSharedElementWithRenderPass(
    CompositorRenderPass* target_render_pass,
    const SharedElementDrawQuad& shared_element_quad,
    const CompositorRenderPass* shared_element_content_pass) {
  auto pass_id = shared_element_content_pass->id;
  const gfx::Rect& shared_pass_output_rect =
      shared_element_content_pass->output_rect;

  gfx::RectF quad_rect(shared_element_quad.rect);
  shared_element_quad.shared_quad_state->quad_to_target_transform.TransformRect(
      &quad_rect);

  gfx::RectF visible_quad_rect(shared_element_quad.visible_rect);
  shared_element_quad.shared_quad_state->quad_to_target_transform.TransformRect(
      &visible_quad_rect);

  auto* copied_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  *copied_quad_state = *shared_element_quad.shared_quad_state;

  gfx::Transform transform;
  transform.Scale(shared_element_quad.rect.width() /
                      static_cast<SkScalar>(shared_pass_output_rect.width()),
                  shared_element_quad.rect.height() /
                      static_cast<SkScalar>(shared_pass_output_rect.height()));
  transform.Translate(-shared_pass_output_rect.x(),
                      -shared_pass_output_rect.y());

  copied_quad_state->quad_to_target_transform.PreconcatTransform(transform);

  auto* render_pass_quad =
      target_render_pass
          ->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  gfx::RectF tex_coord_rect(gfx::SizeF(shared_element_quad.rect.size()));
  tex_coord_rect.Offset(-shared_pass_output_rect.x(),
                        -shared_pass_output_rect.y());
  render_pass_quad->SetNew(
      /*shared_quad_state=*/copied_quad_state,
      /*rect=*/shared_element_quad.rect,
      /*visible_rect=*/shared_pass_output_rect,
      /*render_pass_id=*/pass_id,
      /*mask_resource_id=*/kInvalidResourceId,
      /*mask_uv_rect=*/gfx::RectF(),
      /*mask_texture_size=*/gfx::Size(),
      /*filters_scale=*/gfx::Vector2dF(),
      /*filters_origin=*/gfx::PointF(),
      /*tex_coord_rect=*/tex_coord_rect,
      /*force_anti_aliasing_off=*/false,
      /*backdrop_filter_quality*/ 1.f);
}

// This function swaps a SharedElementDrawQuad with a TextureDrawQuad.
// |target_render_pass| is the render pass where the SharedElementDrawQuad is
// drawn.
// |shared_element_quad| is the quad providing the geometry to draw this shared
// element's content.
// |y_flipped| indicates if the texture should be flipped vertically when
// composited.
// |id| is a reference to the texture which provides the content for this shared
// element.
void ReplaceSharedElementWithTexture(
    CompositorRenderPass* target_render_pass,
    const SharedElementDrawQuad& shared_element_quad,
    bool y_flipped,
    ResourceId resource_id) {
  auto* copied_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  *copied_quad_state = *shared_element_quad.shared_quad_state;

  auto* texture_quad =
      target_render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  float vertex_opacity[] = {1.f, 1.f, 1.f, 1.f};
  texture_quad->SetNew(
      /*shared_quad_state=*/copied_quad_state,
      /*rect=*/shared_element_quad.rect,
      /*visible_rect=*/shared_element_quad.visible_rect,
      /*needs_blending=*/shared_element_quad.needs_blending,
      /*resource_id=*/resource_id,
      /*premultiplied_alpha=*/true,
      /*uv_top_left=*/gfx::PointF(0, 0),
      /*uv_bottom_right=*/gfx::PointF(1, 1),
      /*background_color=*/SK_ColorTRANSPARENT,
      /*vertex_opacity=*/vertex_opacity, y_flipped,
      /*nearest_neighbor=*/false,
      /*secure_output_only=*/false,
      /*protected_video_type=*/gfx::ProtectedVideoType::kClear);
}

std::unique_ptr<gfx::AnimationCurve> CreateOpacityCurve(
    float start_opacity,
    float end_opacity,
    base::TimeDelta duration,
    base::TimeDelta delay,
    gfx::FloatAnimationCurve::Target* target) {
  auto float_curve = gfx::KeyframedFloatAnimationCurve::Create();

  // The curve starts at opacity delay and runs for opacity animation, so it
  // potentially has 3 points:
  // time 0 == start opacity
  // time 'delay' == start opacity
  // time 'delay' + 'opacity_duration' == end opacity
  // Opacity transitions do not need to ease in or out. By passing nullptr for
  // the timing function here, we are choosing the "linear" timing function.
  float_curve->AddKeyframe(
      gfx::FloatKeyframe::Create(base::TimeDelta(), start_opacity, nullptr));
  if (!delay.is_zero()) {
    float_curve->AddKeyframe(
        gfx::FloatKeyframe::Create(delay, start_opacity, nullptr));
  }
  float_curve->AddKeyframe(
      gfx::FloatKeyframe::Create(duration + delay, end_opacity, nullptr));
  float_curve->set_target(target);
  return float_curve;
}

std::unique_ptr<gfx::AnimationCurve> CreateSizeCurve(
    const gfx::SizeF& start_size,
    base::TimeDelta duration,
    base::TimeDelta delay,
    std::unique_ptr<gfx::TimingFunction> timing_function,
    gfx::SizeAnimationCurve::Target* target) {
  auto size_curve = gfx::KeyframedSizeAnimationCurve::Create();
  size_curve->AddKeyframe(gfx::SizeKeyframe::Create(
      base::TimeDelta(), start_size, timing_function->Clone()));
  if (!delay.is_zero()) {
    size_curve->AddKeyframe(
        gfx::SizeKeyframe::Create(delay, start_size, timing_function->Clone()));
  }
  size_curve->AddKeyframe(gfx::SizeKeyframe::Create(
      duration + delay, start_size, std::move(timing_function)));
  size_curve->set_target(target);
  return size_curve;
}

std::unique_ptr<gfx::AnimationCurve> CreateTransformCurve(
    const gfx::TransformOperations& start_transform,
    const gfx::TransformOperations& end_transform,
    base::TimeDelta duration,
    base::TimeDelta delay,
    std::unique_ptr<gfx::TimingFunction> timing_function,
    gfx::TransformAnimationCurve::Target* target) {
  auto transform_curve = gfx::KeyframedTransformAnimationCurve::Create();
  transform_curve->AddKeyframe(gfx::TransformKeyframe::Create(
      base::TimeDelta(), start_transform, timing_function->Clone()));
  if (!delay.is_zero()) {
    transform_curve->AddKeyframe(gfx::TransformKeyframe::Create(
        delay, start_transform, timing_function->Clone()));
  }
  transform_curve->AddKeyframe(gfx::TransformKeyframe::Create(
      duration + delay, end_transform, std::move(timing_function)));
  transform_curve->set_target(target);
  return transform_curve;
}

}  // namespace

class SurfaceAnimationManager::StorageWithSurface {
 public:
  StorageWithSurface(SurfaceSavedFrameStorage* storage, Surface* surface)
      : storage_(storage) {
    DCHECK(!storage_->has_active_surface());
    storage_->set_active_surface(surface);
  }

  ~StorageWithSurface() { storage_->set_active_surface(nullptr); }

  SurfaceSavedFrameStorage* operator->() { return storage_; }

 private:
  raw_ptr<SurfaceSavedFrameStorage> storage_;
};

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
    Surface* active_surface) {
  bool started_animation = false;
  StorageWithSurface storage(&surface_saved_frame_storage_, active_surface);
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
      case CompositorFrameTransitionDirective::Type::kAnimateRenderer:
        handled = ProcessAnimateRendererDirective(directive, storage);
        break;
      case CompositorFrameTransitionDirective::Type::kRelease:
        handled = ProcessReleaseDirective();
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
    StorageWithSurface& storage) {
  // We can only have one saved frame. It is the job of the client to ensure the
  // correct API usage. So if we are receiving a save directive while we already
  // have a saved frame, release it first. That ensures that any subsequent
  // animate directives which presumably rely on this save directive will
  // succeed.
  ProcessReleaseDirective();

  // We need to be in the idle state in order to save.
  if (state_ != State::kIdle)
    return false;
  empty_resource_ids_ =
      storage->ProcessSaveDirective(directive, sequence_id_finished_callback_);
  return true;
}

bool SurfaceAnimationManager::ProcessAnimateDirective(
    const CompositorFrameTransitionDirective& directive,
    StorageWithSurface& storage) {
  // We can only begin an animate if we are currently idle.
  if (state_ != State::kIdle)
    return false;

  // |saved_textures_| are created before the animate directive if we're in
  // a mode where the animation is driven by the renderer.
  if (saved_textures_.has_value()) {
    return false;
  }

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

  CreateRootAnimationCurves(saved_textures_->root.draw_data.size);
  CreateSharedElementCurves();
  TickAnimations(latest_time_);
  state_ = State::kAnimating;

  // If all animations are set with a 0 duration, we can directly jump to the
  // last frame.
  FinishAnimationIfNeeded();
  return true;
}

bool SurfaceAnimationManager::ProcessAnimateRendererDirective(
    const CompositorFrameTransitionDirective& directive,
    StorageWithSurface& storage) {
  // We can only begin an animate if we are currently idle. The renderer sends
  // this in response to a notification of the capture completing successfully.
  if (state_ != State::kIdle)
    return false;

  DCHECK(!saved_textures_);
  state_ = State::kAnimatingRenderer;
  auto saved_frame = storage->TakeSavedFrame();
  if (!saved_frame || !saved_frame->IsValid()) {
    LOG(ERROR) << "Failure in caching shared element snapshots";
    return false;
  }

  // Import the saved frame, which converts it to a ResourceFrame -- a
  // structure which has transferable resources.
  saved_textures_.emplace(
      transferable_resource_tracker_.ImportResources(std::move(saved_frame)));
  return true;
}

bool SurfaceAnimationManager::ProcessReleaseDirective() {
  if (state_ != State::kAnimatingRenderer)
    return false;

  state_ = State::kIdle;
  if (saved_textures_)
    transferable_resource_tracker_.ReturnFrame(*saved_textures_);
  saved_textures_.reset();
  return true;
}

bool SurfaceAnimationManager::NeedsBeginFrame() const {
  // If we're animating we need to keep pumping frames to advance the animation.
  // If we're done, we require one more frame to switch back to idle state.
  return HasRunningAnimations() || state_ == State::kLastFrame;
}

void SurfaceAnimationManager::TickAnimations(base::TimeTicks new_time) {
  root_animation_.driver().Tick(new_time);
  for (auto& shared : shared_animations_)
    shared.driver().Tick(new_time);
}

bool SurfaceAnimationManager::HasRunningAnimations() const {
  // We need to check root and all shared animations here since any of these
  // animations could finish last.
  if (root_animation_.driver().IsAnimating())
    return true;

  for (const auto& animation : shared_animations_) {
    if (animation.driver().IsAnimating())
      return true;
  }

  return false;
}

void SurfaceAnimationManager::NotifyFrameAdvanced() {
  TickAnimations(latest_time_);

  switch (state_) {
    case State::kIdle:
      NOTREACHED() << "We should not advance frames when idle";
      break;
    case State::kAnimatingRenderer:
      NOTREACHED()
          << "We should not advance frames during renderer driven animations";
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

  if (HasRunningAnimations())
    return;

  state_ = State::kLastFrame;
  sequence_id_finished_callback_.Run(animate_directive_->sequence_id());
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
                                  src_transform, SkBlendMode::kSrcOver,
                                  root_animation_.src_opacity(), y_flipped,
                                  saved_textures_->root.resource.id);
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
                                  src_transform, SkBlendMode::kSrcOver,
                                  root_animation_.src_opacity(), y_flipped,
                                  saved_textures_->root.resource.id);
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
  for (const auto& shared_element : animate_directive_->shared_elements()) {
    shared_draw_data.emplace(shared_element.render_pass_id,
                             RenderPassDrawData());
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
    auto pass_copy = TransitionUtils::CopyPassWithQuadFiltering(
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

  const auto& shared_elements = animate_directive_->shared_elements();
  for (size_t i = 0; i < shared_elements.size(); ++i) {
    const CompositorRenderPassId& shared_pass_id =
        shared_elements[i].render_pass_id;
    SharedAnimationState& animation = shared_animations_[i];
    auto& draw_data = shared_draw_data[shared_pass_id];
    const absl::optional<TransferableResourceTracker::PositionedResource>&
        src_texture = saved_textures_->shared[i];

    const bool has_destination_pass =
        draw_data.render_pass && draw_data.draw_quad;
    const bool should_have_destination_pass = !shared_pass_id.is_null();
    DCHECK(!has_destination_pass || should_have_destination_pass);

    // We have to retarget the animations, whether or not we have a destination
    // pass. Ideally we should only need an opacity curve (to fade out the src
    // element) if the dest element is missing but we try to handle the dest
    // element getting added or removed during the transition gracefully by
    // pausing the animation to it's current state.
    auto* opacity_model = animation.driver().GetKeyframeModel(
        SharedAnimationState::kCombinedOpacity);
    float target_opacity = 0.f;
    if (has_destination_pass)
      target_opacity = draw_data.opacity;
    else if (should_have_destination_pass)
      target_opacity = animation.combined_opacity();

    // The |opacity_model| may be null since this animation can finish before
    // animations for other elements.
    if (opacity_model) {
      opacity_model->Retarget(
          latest_time_, SharedAnimationState::kCombinedOpacity, target_opacity);
    }

    auto* size_model =
        animation.driver().GetKeyframeModel(SharedAnimationState::kContentSize);
    gfx::SizeF target_size =
        has_destination_pass
            ? gfx::SizeF(draw_data.render_pass->output_rect.size())
            : animation.content_size();
    if (size_model) {
      size_model->Retarget(latest_time_, SharedAnimationState::kContentSize,
                           target_size);
    } else {
      animation.OnSizeAnimated(target_size, SharedAnimationState::kContentSize,
                               nullptr);
    }

    auto* transform_model = animation.driver().GetKeyframeModel(
        SharedAnimationState::kCombinedTransform);
    gfx::TransformOperations target_transform_ops;
    gfx::Transform end_transform(gfx::Transform::kSkipInitialization);
    if (has_destination_pass) {
      end_transform = draw_data.render_pass->transform_to_root_target;
      // The mapping from dest pass origin to target buffer is done when drawing
      // the dest pass to the intermediate transition pass (if used). So we
      // exclude this translation from the transform here and add it when
      // drawing the dest pass.
      auto origin = draw_data.render_pass->output_rect.origin();
      end_transform.Translate(origin.x(), origin.y());
    } else {
      end_transform = animation.combined_transform().Apply();
    }
    target_transform_ops.AppendMatrix(end_transform);

    if (transform_model) {
      transform_model->Retarget(latest_time_,
                                SharedAnimationState::kCombinedTransform,
                                target_transform_ops);
    } else {
      animation.OnTransformAnimated(target_transform_ops,
                                    SharedAnimationState::kCombinedTransform,
                                    nullptr);
    }

    // Now that we have updated the animations, we can append the interpolations
    // to the animation pass.
    const float content_opacity = animation.content_opacity();
    const gfx::Size content_size = gfx::ToFlooredSize(animation.content_size());
    const float combined_opacity = animation.combined_opacity();
    const gfx::Transform combined_transform =
        animation.combined_transform().Apply();

    std::unique_ptr<CompositorRenderPass> transition_pass;
    if (src_texture.has_value() && has_destination_pass) {
      size_t num_of_quads = 2;
      transition_pass =
          CompositorRenderPass::Create(num_of_quads, num_of_quads);
      gfx::Rect output_rect(content_size);
      max_id = TransitionUtils::NextRenderPassId(max_id);
      transition_pass->SetNew(max_id, output_rect, output_rect,
                              combined_transform);
    }

    // The quad list is in front to back order. So adding the src texture first
    // makes it so it draws on top of the dest texture.
    if (src_texture.has_value()) {
      bool y_flipped = !src_texture->resource.is_software;
      gfx::Transform src_transform;
      src_transform.Scale(static_cast<float>(content_size.width()) /
                              src_texture->draw_data.size.width(),
                          static_cast<float>(content_size.height()) /
                              src_texture->draw_data.size.height());
      float src_opacity = 1.f - content_opacity;

      // Use kPlus mode to add the pixel values from src and destination
      // textures. This ensures the blending pass is a no-op if the 2 have the
      // same content.
      SkBlendMode blend_mode = SkBlendMode::kPlus;

      auto* pass_for_draw = transition_pass.get();
      if (!pass_for_draw) {
        pass_for_draw = animation_pass;
        src_transform.ConcatTransform(combined_transform);
        src_opacity *= combined_opacity;
        blend_mode = SkBlendMode::kSrcOver;
      }

      CreateAndAppendSrcTextureQuad(
          pass_for_draw, gfx::Rect(src_texture->draw_data.size), src_transform,
          blend_mode, src_opacity, y_flipped, src_texture->resource.id);
      interpolated_frame->resource_list.push_back(src_texture->resource);
    }

    if (!has_destination_pass)
      continue;

    // Now that we know we have a render pass destination, create a copy of the
    // shared render pass, and update it with all the right values.
    auto pass_copy = draw_data.render_pass->DeepCopy();
    max_id = pass_copy->id = TransitionUtils::NextRenderPassId(max_id);
    gfx::Vector2dF dest_scale(static_cast<float>(content_size.width()) /
                                  pass_copy->output_rect.width(),
                              static_cast<float>(content_size.height()) /
                                  pass_copy->output_rect.height());
    gfx::Point dest_origin = pass_copy->output_rect.origin();
    pass_copy->transform_to_root_target = combined_transform;
    pass_copy->transform_to_root_target.Scale(dest_scale.x(), dest_scale.y());
    pass_copy->transform_to_root_target.Translate(-dest_origin.x(),
                                                  -dest_origin.y());

    auto* pass_for_draw = transition_pass.get();
    gfx::Transform dest_transform;
    dest_transform.Scale(dest_scale.x(), dest_scale.y());
    dest_transform.Translate(-dest_origin.x(), -dest_origin.y());
    float dest_opacity = content_opacity;

    // Use kSrc mode to clear the intermediate texture used for blending with
    // dest content.
    SkBlendMode blend_mode = SkBlendMode::kSrc;
    if (!pass_for_draw) {
      pass_for_draw = animation_pass;
      dest_transform.ConcatTransform(combined_transform);
      dest_opacity *= combined_opacity;
      blend_mode = SkBlendMode::kSrcOver;
    }

    CreateAndAppendSharedRenderPassDrawQuad(
        pass_for_draw, draw_data.render_pass->output_rect, dest_transform,
        dest_opacity, pass_copy->id, blend_mode, *draw_data.draw_quad);

    // Finally, add the pass into the interpolated frame. Make sure this comes
    // after CreateAndAppend* call, because we use a pass id, so we need to
    // access the pass before moving it here.
    interpolated_frame->render_pass_list.emplace_back(std::move(pass_copy));

    if (transition_pass) {
      auto* quad_state = animation_pass->CreateAndAppendSharedQuadState();
      gfx::Rect rect(content_size);
      quad_state->SetAll(
          /*quad_to_target_transform=*/combined_transform,
          /*quad_layer_rect=*/rect,
          /*visible_layer_rect=*/rect,
          /*mask_filter_info=*/gfx::MaskFilterInfo(),
          /*clip_rect=*/absl::nullopt,
          /*are_contents_opaque=*/false,
          /*opacity=*/combined_opacity,
          /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

      auto* quad =
          animation_pass
              ->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
      quad->SetNew(
          /*shared_quad_state=*/quad_state,
          /*rect=*/rect,
          /*visible_rect=*/rect,
          /*render_pass_id=*/transition_pass->id,
          /*mask_resource_id=*/kInvalidResourceId,
          /*mask_uv_rect=*/gfx::RectF(),
          /*mask_texture_size=*/gfx::Size(),
          /*filters_scale=*/gfx::Vector2dF(),
          /*filters_origin=*/gfx::PointF(),
          /*tex_coord_rect=*/gfx::RectF(rect),
          /*force_anti_aliasing_off=*/
          draw_data.draw_quad->force_anti_aliasing_off,
          /*backdrop_filter_quality=*/0.f);

      interpolated_frame->render_pass_list.emplace_back(
          std::move(transition_pass));
    }
  }
}

// static
bool SurfaceAnimationManager::FilterSharedElementQuads(
    base::flat_map<CompositorRenderPassId, RenderPassDrawData>*
        shared_draw_data,
    const DrawQuad& quad,
    CompositorRenderPass& copy_pass) {
  if (quad.material != DrawQuad::Material::kCompositorRenderPass)
    return false;

  const auto& pass_quad = *CompositorRenderPassDrawQuad::MaterialCast(&quad);
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
  if (save_directive_->root_config().duration.is_zero())
    return;

  // A small translation. We want to roughly scale this with screen size, but
  // we choose the minimum screen dimension to keep horizontal and vertical
  // transitions consistent and to avoid the impact of very oblong screen.
  const float delta = std::min(output_size.width(), output_size.height()) *
                      kTranslationProportion;

  gfx::TransformOperations start_transform;
  gfx::TransformOperations end_transform;
  int transform_property_id = RootAnimationState::kDstTransform;

  float start_opacity = 0.0f;
  float end_opacity = 1.0f;
  int opacity_property_id = RootAnimationState::kDstOpacity;

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
      transform_property_id = RootAnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = RootAnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealRight: {
      end_transform.AppendTranslate(delta, 0.0f, 0.0f);
      transform_property_id = RootAnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = RootAnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealUp: {
      end_transform.AppendTranslate(0.0f, -delta, 0.0f);
      transform_property_id = RootAnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = RootAnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kRevealDown: {
      end_transform.AppendTranslate(0.0f, delta, 0.0f);
      transform_property_id = RootAnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = RootAnimationState::kSrcOpacity;
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
      transform_property_id = RootAnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = RootAnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kFade: {
      // Fade is effectively an explode with no scaling.
      transform_property_id = RootAnimationState::kSrcTransform;
      std::swap(start_opacity, end_opacity);
      opacity_property_id = RootAnimationState::kSrcOpacity;
      break;
    }
    case CompositorFrameTransitionDirective::Effect::kNone: {
      transform_property_id = RootAnimationState::kSrcTransform;
      start_opacity = end_opacity = 0.0f;
      opacity_property_id = RootAnimationState::kSrcOpacity;
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
          opacity_property_id == RootAnimationState::kSrcOpacity
              ? gfx::CubicBezierTimingFunction::EaseType::EASE_IN
              : gfx::CubicBezierTimingFunction::EaseType::EASE_OUT);

  // Create the transform curve.
  base::TimeDelta total_duration = save_directive_->root_config().duration;
  base::TimeDelta total_delay = save_directive_->root_config().delay;

  // The transform animation runs for the entire duration of the root
  // transition.
  auto transform_curve = CreateTransformCurve(
      start_transform, end_transform, total_duration, total_delay,
      timing_function->Clone(), &root_animation_);
  root_animation_.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
      std::move(transform_curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
      transform_property_id));

  // Create the opacity curve. Somewhat more complicated because it may be
  // delayed wrt to the transform curve. See description of
  // |kOpacityTransitionDurationScaleFactor| above.
  base::TimeDelta opacity_duration =
      total_duration * kOpacityTransitionDurationScaleFactor;
  base::TimeDelta opacity_delay = start_opacity == 0.0f
                                      ? base::TimeDelta()
                                      : total_duration - opacity_duration;

  // Add a delay to offset the opacity animation by the delay for the entire
  // root transition.
  opacity_delay += total_delay;

  auto opacity_curve =
      CreateOpacityCurve(start_opacity, end_opacity, opacity_duration,
                         opacity_delay, &root_animation_);
  root_animation_.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
      std::move(opacity_curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
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
  shared_animations_.resize(animate_directive_->shared_elements().size());

  // Since we don't have a target state yet, create animations as if all of the
  // shared elements are targeted to stay in place with opacity going to 0.
  for (size_t i = 0; i < saved_textures_->shared.size(); ++i) {
    if (save_directive_->shared_elements()[i].config.duration.is_zero())
      continue;

    auto& shared = saved_textures_->shared[i];
    auto& state = shared_animations_[i];
    const bool has_src_element = shared.has_value();

    const auto& config = save_directive_->shared_elements()[i].config;
    const auto total_duration = config.duration;
    const auto total_delay = config.delay;

    const auto opacity_duration =
        total_duration * kSharedOpacityTransitionDurationScaleFactor;
    const auto opacity_delay =
        total_delay +
        (total_duration * kSharedOpacityTransitionDelayScaleFactor);

    // The kSrcOpacity curve animates the screen space opacity applied to the
    // blended content from src and dest elements. The value goes from the
    // src element's opacity value to dest element's opacity value.
    // - If the src element is missing, the start opacity is 0 to allow the dest
    //   element to gradually fade in.
    // - If the dest element is missing, the end opacity is 0 to allow the src
    //   element to gradually fade out.
    // The animation is re-targeted once the dest element values are known.
    float start_opacity = has_src_element ? shared->draw_data.opacity : 0.f;
    auto opacity_curve = CreateOpacityCurve(
        start_opacity, 1.f, opacity_duration, opacity_delay, &state);
    state.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
        std::move(opacity_curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
        SharedAnimationState::kCombinedOpacity));

    if (!has_src_element)
      continue;

    // The specific timing function is fine tuned for these effects.
    auto ease_timing =
        gfx::CubicBezierTimingFunction::Create(0.4, 0.0, 0.2, 1.0);

    // Interpolation between the 2 textures involves an opacity animation (to
    // cross-fade the content) and a scale animation to transition the content
    // size.
    auto content_size_curve =
        CreateSizeCurve(gfx::SizeF(shared->draw_data.size), total_duration,
                        total_delay, ease_timing->Clone(), &state);
    state.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
        std::move(content_size_curve),
        gfx::KeyframeEffect::GetNextKeyframeModelId(),
        SharedAnimationState::kContentSize));
    auto content_opacity_curve = CreateOpacityCurve(
        /*start_opacity=*/0.f, /*end_opacity=*/1.f, opacity_duration,
        opacity_delay, &state);
    state.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
        std::move(content_opacity_curve),
        gfx::KeyframeEffect::GetNextKeyframeModelId(),
        SharedAnimationState::kContentOpacity));

    // The screen space transform for the interpolated texture is animated from
    // src element to dest element value. The animation is re-targeted once the
    // dest element values are known.
    // We use the same value for start and end transform since this animation
    // will be targeted on the first frame when the end state for each shared
    // element is known.
    gfx::TransformOperations start_transform_ops;
    start_transform_ops.AppendMatrix(shared->draw_data.target_transform);
    auto transform_curve = CreateTransformCurve(
        start_transform_ops, start_transform_ops, total_duration, total_delay,
        ease_timing->Clone(), &state);
    state.driver().AddKeyframeModel(gfx::KeyframeModel::Create(
        std::move(transform_curve),
        gfx::KeyframeEffect::GetNextKeyframeModelId(),
        SharedAnimationState::kCombinedTransform));
  }
}

bool SurfaceAnimationManager::FilterSharedElementsWithRenderPassOrResource(
    std::vector<TransferableResource>* resource_list,
    const base::flat_map<SharedElementResourceId, const CompositorRenderPass*>*
        element_id_to_pass,
    const DrawQuad& quad,
    CompositorRenderPass& copy_pass) {
  if (quad.material != DrawQuad::Material::kSharedElement)
    return false;

  const auto& shared_element_quad = *SharedElementDrawQuad::MaterialCast(&quad);

  // Look up the shared element in live render passes first.
  auto pass_it = element_id_to_pass->find(shared_element_quad.resource_id);
  if (pass_it != element_id_to_pass->end()) {
    ReplaceSharedElementWithRenderPass(&copy_pass, shared_element_quad,
                                       pass_it->second);
    return true;
  }

  if (saved_textures_) {
    auto texture_it = saved_textures_->element_id_to_resource.find(
        shared_element_quad.resource_id);

    if (texture_it != saved_textures_->element_id_to_resource.end()) {
      const auto& transferable_resource = texture_it->second;
      resource_list->push_back(transferable_resource);

      // GPU textures are flipped but software bitmaps are not.
      bool y_flipped = !transferable_resource.is_software;
      ReplaceSharedElementWithTexture(&copy_pass, shared_element_quad,
                                      y_flipped, resource_list->back().id);
      return true;
    }
  }

  if (empty_resource_ids_.count(shared_element_quad.resource_id) > 0)
    return true;

#if DCHECK_IS_ON()
  LOG(ERROR) << "Content not found for shared element: "
             << shared_element_quad.resource_id.ToString();
  LOG(ERROR) << "Known shared element ids:";
  for (const auto& [shared_resource_id, render_pass] : *element_id_to_pass) {
    LOG(ERROR) << " " << shared_resource_id.ToString()
               << " -> RenderPassId: " << render_pass->id.GetUnsafeValue();
  }

  if (saved_textures_) {
    LOG(ERROR) << "Known saved textures:";
    for (const auto& [shared_resource_id, transferable_resource] :
         saved_textures_->element_id_to_resource) {
      LOG(ERROR) << " " << shared_resource_id.ToString();
    }
  }

  // The DCHECK below is for debugging in dev builds. This can happen in
  // production code because of a compromised renderer.
  NOTREACHED();
#endif

  return true;
}

void SurfaceAnimationManager::ReplaceSharedElementResources(Surface* surface) {
  const auto& active_frame = surface->GetActiveFrame();
  if (!active_frame.metadata.has_shared_element_resources)
    return;

  // Replacing shared elements with resources is done in the following states :
  // 1) When a transition is initiated. Shared elements are displayed using a
  //    render pass until copy requests to cache them finishes executing.
  // 2) When a renderer driven animation is in progress. The old shared elements
  //    are represented using cached copy results while elements in the new DOM
  //    use a render pass from the same frame.
  // We shouldn't need to replace elements during a Viz driven animation.
  if (state_ == State::kAnimating || state_ == State::kLastFrame) {
    LOG(ERROR) << "Unexpected frame with shared element resources during viz "
                  "animation";
    return;
  }

  // A frame created by resolving SharedElementResourceIds to their
  // corresponding static or live snapshot.
  DCHECK(!surface->HasInterpolatedFrame())
      << "Can not override interpolated frame";
  CompositorFrame resolved_frame;
  resolved_frame.metadata = active_frame.metadata.Clone();
  resolved_frame.resource_list = active_frame.resource_list;

  base::flat_map<SharedElementResourceId, const CompositorRenderPass*>
      element_id_to_pass;
  TransitionUtils::FilterCallback filter_callback = base::BindRepeating(
      &SurfaceAnimationManager::FilterSharedElementsWithRenderPassOrResource,
      base::Unretained(this), base::Unretained(&resolved_frame.resource_list),
      base::Unretained(&element_id_to_pass));

  for (auto& render_pass : active_frame.render_pass_list) {
    auto copy_requests = std::move(render_pass->copy_requests);
    auto pass_copy = TransitionUtils::CopyPassWithQuadFiltering(
        *render_pass, filter_callback);
    pass_copy->copy_requests = std::move(copy_requests);

    // This must be done after copying the render pass so we use the render pass
    // id of |pass_copy| when replacing SharedElementDrawQuads.
    if (pass_copy->shared_element_resource_id.IsValid()) {
      DCHECK(element_id_to_pass.find(pass_copy->shared_element_resource_id) ==
             element_id_to_pass.end());
      element_id_to_pass.emplace(pass_copy->shared_element_resource_id,
                                 pass_copy.get());
    }

    resolved_frame.render_pass_list.push_back(std::move(pass_copy));
  }

  surface->SetInterpolatedFrame(std::move(resolved_frame));
}

SurfaceSavedFrameStorage*
SurfaceAnimationManager::GetSurfaceSavedFrameStorageForTesting() {
  return &surface_saved_frame_storage_;
}

// RootAnimationState
SurfaceAnimationManager::RootAnimationState::RootAnimationState() = default;
SurfaceAnimationManager::RootAnimationState::RootAnimationState(
    RootAnimationState&&) = default;
SurfaceAnimationManager::RootAnimationState::~RootAnimationState() = default;

void SurfaceAnimationManager::RootAnimationState::OnFloatAnimated(
    const float& value,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == kDstOpacity) {
    dst_opacity_ = value;
  } else {
    src_opacity_ = value;
  }
}

void SurfaceAnimationManager::RootAnimationState::OnTransformAnimated(
    const gfx::TransformOperations& operations,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == kDstTransform) {
    dst_transform_ = operations;
  } else {
    src_transform_ = operations;
  }
}

void SurfaceAnimationManager::RootAnimationState::Reset() {
  src_opacity_ = 1.0f;
  dst_opacity_ = 1.0f;
  src_transform_ = gfx::TransformOperations();
  dst_transform_ = gfx::TransformOperations();
}

SurfaceAnimationManager::SharedAnimationState::SharedAnimationState() = default;
SurfaceAnimationManager::SharedAnimationState::SharedAnimationState(
    SharedAnimationState&&) = default;
SurfaceAnimationManager::SharedAnimationState::~SharedAnimationState() =
    default;

void SurfaceAnimationManager::SharedAnimationState::OnFloatAnimated(
    const float& value,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  if (target_property_id == kContentOpacity) {
    content_opacity_ = value;
  } else {
    DCHECK_EQ(target_property_id, kCombinedOpacity);
    combined_opacity_ = value;
  }
}

void SurfaceAnimationManager::SharedAnimationState::OnTransformAnimated(
    const gfx::TransformOperations& operations,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  DCHECK_EQ(target_property_id, kCombinedTransform);
  combined_transform_ = operations;
}

void SurfaceAnimationManager::SharedAnimationState::OnSizeAnimated(
    const gfx::SizeF& value,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  DCHECK_EQ(target_property_id, kContentSize);
  content_size_ = value;
}

void SurfaceAnimationManager::SharedAnimationState::Reset() {
  content_opacity_ = 1.0f;
  content_size_ = gfx::SizeF();
  combined_opacity_ = 1.0f;
  combined_transform_ = gfx::TransformOperations();
}

SurfaceAnimationManager::RenderPassDrawData::RenderPassDrawData() = default;
SurfaceAnimationManager::RenderPassDrawData::RenderPassDrawData(
    RenderPassDrawData&&) = default;
SurfaceAnimationManager::RenderPassDrawData::~RenderPassDrawData() = default;

}  // namespace viz
