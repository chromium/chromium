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
#include "cc/base/math_util.h"
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
#include "ui/gfx/geometry/rect_conversions.h"
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
    CompositorRenderPass* shared_element_content_pass) {
  auto pass_id = shared_element_content_pass->id;
  const gfx::Rect& shared_pass_output_rect =
      shared_element_content_pass->output_rect;

  auto* copied_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  *copied_quad_state = *shared_element_quad.shared_quad_state;

  gfx::Transform transform = GetViewTransitionTransform(
      shared_element_quad.rect, shared_pass_output_rect);
  copied_quad_state->quad_to_target_transform.PreConcat(transform);
  copied_quad_state->quad_layer_rect = shared_pass_output_rect;
  copied_quad_state->visible_quad_layer_rect = shared_pass_output_rect;

  shared_element_content_pass->transform_to_root_target =
      copied_quad_state->quad_to_target_transform;
  shared_element_content_pass->transform_to_root_target.PostConcat(
      target_render_pass->transform_to_root_target);

  auto* render_pass_quad =
      target_render_pass
          ->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  gfx::RectF tex_coord_rect(gfx::Rect(shared_pass_output_rect.size()));
  render_pass_quad->SetNew(
      /*shared_quad_state=*/copied_quad_state,
      /*rect=*/shared_pass_output_rect,
      /*visible_rect=*/shared_pass_output_rect,
      /*render_pass_id=*/pass_id,
      /*mask_resource_id=*/kInvalidResourceId,
      /*mask_uv_rect=*/gfx::RectF(),
      /*mask_texture_size=*/gfx::Size(),
      /*filters_scale=*/gfx::Vector2dF(1.0f, 1.0f),
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
// |id| is a reference to the texture which provides the content for this shared
// element.
void ReplaceSharedElementWithTexture(
    CompositorRenderPass* target_render_pass,
    const SharedElementDrawQuad& shared_element_quad,
    ResourceId resource_id) {
  auto* copied_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  *copied_quad_state = *shared_element_quad.shared_quad_state;

  auto* texture_quad =
      target_render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
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
      /*y_flipped=*/false,
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
    gpu::SharedImageInterface* shared_image_interface,
    ReservedResourceIdTracker* id_tracker,
    SaveDirectiveCompleteCallback sequence_id_finished_callback) {
  return base::WrapUnique(new SurfaceAnimationManager(
      directive, surface, shared_bitmap_manager, shared_image_interface,
      id_tracker, std::move(sequence_id_finished_callback)));
}

SurfaceAnimationManager::SurfaceAnimationManager(
    const CompositorFrameTransitionDirective& directive,
    Surface* surface,
    SharedBitmapManager* shared_bitmap_manager,
    gpu::SharedImageInterface* shared_image_interface,
    ReservedResourceIdTracker* id_tracker,
    SaveDirectiveCompleteCallback sequence_id_finished_callback)
    : transferable_resource_tracker_(shared_bitmap_manager, id_tracker),
      saved_frame_(directive, shared_image_interface) {
  DCHECK(directive.type() == CompositorFrameTransitionDirective::Type::kSave);

  // The SurfaceSavedFrame can dispatch the result asynchronously so use a weak
  // ptr.
  auto copy_finished_callback =
      base::BindOnce(&SurfaceAnimationManager::OnSaveDirectiveProcessed,
                     weak_factory_.GetMutableWeakPtr(),
                     std::move(sequence_id_finished_callback));
  saved_frame_.RequestCopyOfOutput(surface, std::move(copy_finished_callback));
  empty_resource_ids_ = saved_frame_.GetEmptyResourceIds();
  if (saved_frame_.IsValid() && !directive.maybe_cross_frame_sink()) {
    ImportTextures();
  }
}

SurfaceAnimationManager::~SurfaceAnimationManager() {
  if (saved_textures_)
    transferable_resource_tracker_.ReturnFrame(*saved_textures_);
  saved_textures_.reset();
}

void SurfaceAnimationManager::OnSaveDirectiveProcessed(
    SaveDirectiveCompleteCallback callback,
    const CompositorFrameTransitionDirective& directive) {
  CHECK_EQ(stage_, Stage::kPendingCopy);
  stage_ = Stage::kWaitingForAnimate;

  // Importing textures must be deferred until the SurfaceAnimationManager is
  // bound to a frame sink. This is because ref-counting for textures
  // referenced in a Surface's frame is managed by the frame sink associated
  // with that Surface. So if this transition is potentially cross frame sink,
  // we need to defer importing textures until the animate directive. The
  // frame sink for the transition is finalized to the frame sink using the
  // animate directive.
  if (saved_frame_.IsValid() && !directive.maybe_cross_frame_sink()) {
    ImportTextures();
  }

  std::move(callback).Run(directive);
}

bool SurfaceAnimationManager::Animate() {
  if (stage_ != Stage::kWaitingForAnimate) {
    return false;
  }

  stage_ = Stage::kAnimating;
  if (saved_frame_.IsValid()) {
    ImportTextures();
  }
  return true;
}

void SurfaceAnimationManager::ImportTextures() {
  CHECK(!saved_textures_);
  CHECK(saved_frame_.IsValid());

  // Import the saved frame, which converts it to a ResourceFrame -- a
  // structure which has transferable resources.
  saved_textures_.emplace(transferable_resource_tracker_.ImportResources(
      saved_frame_.TakeResult(), saved_frame_.directive()));
  empty_resource_ids_.clear();
}

void SurfaceAnimationManager::ReceiveFromChild(
    const std::vector<TransferableResource>& resources) {
  // We don't do anything here, because resources are initially reffed via
  // `ImportResources`.
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
    if (resource.id >= kVizReservedRangeStartId) {
      transferable_resource_tracker_.UnrefResource(resource.id, resource.count,
                                                   resource.sync_token);
    }
  }
}

// static
bool SurfaceAnimationManager::FilterSharedElementsWithRenderPassOrResource(
    std::vector<TransferableResource>* resource_list,
    const base::flat_map<ViewTransitionElementResourceId,
                         CompositorRenderPass*>* element_id_to_pass,
    const base::flat_map<blink::ViewTransitionToken,
                         std::unique_ptr<SurfaceAnimationManager>>*
        token_to_animation_manager,
    const DrawQuad& quad,
    CompositorRenderPass& copy_pass) {
  if (quad.material != DrawQuad::Material::kSharedElement) {
    return false;
  }

  const auto& shared_element_quad = *SharedElementDrawQuad::MaterialCast(&quad);

  // Look up the shared element in textures first. This ordering is important
  // since there can be situations where we created a texture _and_ we have a
  // render pass (if we're using BlitRequests).
  auto manager_it = token_to_animation_manager->find(
      shared_element_quad.resource_id.transition_token());
  if (manager_it == token_to_animation_manager->end()) {
    LOG(ERROR) << "No SurfaceAnimationManager for token : "
               << shared_element_quad.resource_id.transition_token().ToString();
    return true;
  }

  auto& saved_textures = manager_it->second->saved_textures_;
  if (saved_textures) {
    auto texture_it = saved_textures->element_id_to_resource.find(
        shared_element_quad.resource_id);

    if (texture_it != saved_textures->element_id_to_resource.end()) {
      const auto& transferable_resource = texture_it->second;
      if (transferable_resource.is_empty()) {
        return true;
      }

      resource_list->push_back(transferable_resource);
      manager_it->second->RefResources({transferable_resource});

      ReplaceSharedElementWithTexture(&copy_pass, shared_element_quad,
                                      resource_list->back().id);
      return true;
    }
  }

  // Look up the shared element in live render passes second.
  auto pass_it = element_id_to_pass->find(shared_element_quad.resource_id);
  if (pass_it != element_id_to_pass->end()) {
    ReplaceSharedElementWithRenderPass(&copy_pass, shared_element_quad,
                                       pass_it->second);
    return true;
  }

  if (manager_it->second->empty_resource_ids_.count(
          shared_element_quad.resource_id) > 0) {
    return true;
  }

#if DCHECK_IS_ON()
  LOG(ERROR) << "Content not found for shared element: "
             << shared_element_quad.resource_id.ToString();
  LOG(ERROR) << "Known shared element ids:";
  for (const auto& [shared_resource_id, render_pass] : *element_id_to_pass) {
    LOG(ERROR) << " " << shared_resource_id.ToString()
               << " -> RenderPassId: " << render_pass->id.GetUnsafeValue();
  }

  if (saved_textures) {
    LOG(ERROR) << "Known saved textures:";
    for (const auto& [shared_resource_id, transferable_resource] :
         saved_textures->element_id_to_resource) {
      LOG(ERROR) << " " << shared_resource_id.ToString();
    }
  }

  // The DCHECK below is for debugging in dev builds. This can happen in
  // production code because of a compromised renderer.
  NOTREACHED_IN_MIGRATION();
#endif

  return true;
}

// static
void SurfaceAnimationManager::ReplaceSharedElementResources(
    Surface* surface,
    const base::flat_map<blink::ViewTransitionToken,
                         std::unique_ptr<SurfaceAnimationManager>>&
        token_to_animation_manager) {
  const auto& active_frame = surface->GetActiveFrame();
  if (!active_frame.metadata.has_shared_element_resources) {
    return;
  }

  CompositorFrame resolved_frame;
  resolved_frame.metadata = active_frame.metadata.Clone();
  resolved_frame.resource_list = active_frame.resource_list;

  base::flat_map<ViewTransitionElementResourceId, CompositorRenderPass*>
      element_id_to_pass;
  TransitionUtils::FilterCallback filter_callback = base::BindRepeating(
      &SurfaceAnimationManager::FilterSharedElementsWithRenderPassOrResource,
      base::Unretained(&resolved_frame.resource_list),
      base::Unretained(&element_id_to_pass),
      base::Unretained(&token_to_animation_manager));

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

  surface->SetActiveFrameForViewTransition(std::move(resolved_frame));
}

}  // namespace viz
