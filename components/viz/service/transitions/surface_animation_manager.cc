// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/transitions/surface_animation_manager.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
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
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/surfaces/surface.h"
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

  auto* copied_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  *copied_quad_state = *shared_element_quad.shared_quad_state;

  gfx::Transform transform = GetViewTransitionTransform(
      shared_element_quad.rect, shared_pass_output_rect);

  copied_quad_state->quad_to_target_transform.PreConcat(transform);

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
      /*background_color=*/SkColors::kTransparent,
      /*vertex_opacity=*/vertex_opacity, y_flipped,
      /*nearest_neighbor=*/false,
      /*secure_output_only=*/false,
      /*protected_video_type=*/gfx::ProtectedVideoType::kClear);
}

}  // namespace

// static
std::unique_ptr<SurfaceAnimationManager>
SurfaceAnimationManager::CreateWithSave(
    const CompositorFrameTransitionDirective& directive,
    Surface* surface,
    SharedBitmapManager* shared_bitmap_manager,
    TransitionDirectiveCompleteCallback sequence_id_finished_callback) {
  return base::WrapUnique(
      new SurfaceAnimationManager(directive, surface, shared_bitmap_manager,
                                  std::move(sequence_id_finished_callback)));
}

SurfaceAnimationManager::SurfaceAnimationManager(
    const CompositorFrameTransitionDirective& directive,
    Surface* surface,
    SharedBitmapManager* shared_bitmap_manager,
    TransitionDirectiveCompleteCallback sequence_id_finished_callback)
    : transferable_resource_tracker_(shared_bitmap_manager) {
  DCHECK(directive.type() == CompositorFrameTransitionDirective::Type::kSave);
  saved_frame_ = std::make_unique<SurfaceSavedFrame>(
      directive, std::move(sequence_id_finished_callback));
  saved_frame_->RequestCopyOfOutput(surface);
  empty_resource_ids_ = saved_frame_->GetEmptyResourceIds();
}

SurfaceAnimationManager::~SurfaceAnimationManager() {
  if (saved_textures_)
    transferable_resource_tracker_.ReturnFrame(*saved_textures_);
  saved_textures_.reset();
}

void SurfaceAnimationManager::Animate() {
  if (animating_)
    return;

  DCHECK(!saved_textures_);
  animating_ = true;
  if (!saved_frame_ || !saved_frame_->IsValid()) {
    LOG(ERROR) << "Failure in caching shared element snapshots";
    saved_frame_.reset();
    return;
  }

  // Import the saved frame, which converts it to a ResourceFrame -- a
  // structure which has transferable resources.
  saved_textures_.emplace(
      transferable_resource_tracker_.ImportResources(std::move(saved_frame_)));
  empty_resource_ids_.clear();
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

bool SurfaceAnimationManager::FilterSharedElementsWithRenderPassOrResource(
    std::vector<TransferableResource>* resource_list,
    const base::flat_map<ViewTransitionElementResourceId,
                         const CompositorRenderPass*>* element_id_to_pass,
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
      if (transferable_resource.is_null())
        return true;

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

  // A frame created by resolving ViewTransitionElementResourceIds to their
  // corresponding static or live snapshot.
  DCHECK(!surface->HasInterpolatedFrame())
      << "Can not override interpolated frame";
  CompositorFrame resolved_frame;
  resolved_frame.metadata = active_frame.metadata.Clone();
  resolved_frame.resource_list = active_frame.resource_list;

  base::flat_map<ViewTransitionElementResourceId, const CompositorRenderPass*>
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
    if (pass_copy->view_transition_element_resource_id.IsValid()) {
      DCHECK(element_id_to_pass.find(
                 pass_copy->view_transition_element_resource_id) ==
             element_id_to_pass.end());
      element_id_to_pass.emplace(pass_copy->view_transition_element_resource_id,
                                 pass_copy.get());
    }

    resolved_frame.render_pass_list.push_back(std::move(pass_copy));
  }

  surface->SetInterpolatedFrame(std::move(resolved_frame));
}

void SurfaceAnimationManager::CompleteSaveForTesting() {
  DCHECK(saved_frame_);
  saved_frame_->CompleteSavedFrameForTesting();  // IN-TEST
}

}  // namespace viz
