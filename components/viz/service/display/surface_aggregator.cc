// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/surface_aggregator.h"

#include <stddef.h>

#include <algorithm>
#include <map>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "components/viz/common/display/de_jelly.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_allocation_group.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"

namespace viz {
namespace {

// Used for determine when to treat opacity close to 1.f as opaque. The value is
// chosen to be smaller than 1/255.
constexpr float kOpacityEpsilon = 0.001f;

void MoveMatchingRequests(
    CompositorRenderPassId render_pass_id,
    std::multimap<CompositorRenderPassId, std::unique_ptr<CopyOutputRequest>>*
        copy_requests,
    std::vector<std::unique_ptr<CopyOutputRequest>>* output_requests) {
  auto request_range = copy_requests->equal_range(render_pass_id);
  for (auto it = request_range.first; it != request_range.second; ++it) {
    DCHECK(it->second);
    output_requests->push_back(std::move(it->second));
  }
  copy_requests->erase(request_range.first, request_range.second);
}

// Returns true if the damage rect is valid.
bool CalculateQuadSpaceDamageRect(
    const gfx::Transform& quad_to_target_transform,
    const gfx::Transform& target_to_root_transform,
    const gfx::Rect& root_damage_rect,
    gfx::Rect* quad_space_damage_rect) {
  gfx::Transform quad_to_root_transform(target_to_root_transform,
                                        quad_to_target_transform);
  gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
  bool inverse_valid = quad_to_root_transform.GetInverse(&inverse_transform);
  if (!inverse_valid)
    return false;

  *quad_space_damage_rect = cc::MathUtil::ProjectEnclosingClippedRect(
      inverse_transform, root_damage_rect);
  return true;
}

gfx::Rect GetExpandedRectWithPixelMovingForegroundFilter(
    const CompositorRenderPassDrawQuad* rpdq,
    const CompositorRenderPass& child_render_pass) {
  const SharedQuadState* shared_quad_state = rpdq->shared_quad_state;
  float max_pixel_movement = child_render_pass.filters.MaximumPixelMovement();
  gfx::RectF rect(rpdq->rect);
  rect.Inset(-max_pixel_movement, -max_pixel_movement);
  gfx::Rect expanded_rect = gfx::ToEnclosingRect(rect);

  // expanded_rect in the target space
  return cc::MathUtil::MapEnclosingClippedRect(
      shared_quad_state->quad_to_target_transform, expanded_rect);
}

}  // namespace

struct SurfaceAggregator::ClipData {
  std::string ToString() const {
    return is_clipped ? "clip " + rect.ToString() : "no clip";
  }

  bool is_clipped = false;
  gfx::Rect rect;
};

struct SurfaceAggregator::PrewalkResult {
  // This is the set of Surfaces that were referenced by another Surface, but
  // not included in a SurfaceDrawQuad.
  base::flat_set<SurfaceId> undrawn_surfaces;
  bool may_contain_video = false;
  bool frame_sinks_changed = false;
  gfx::ContentColorUsage content_color_usage = gfx::ContentColorUsage::kSRGB;
};

struct SurfaceAggregator::MaskFilterInfoExt {
  MaskFilterInfoExt() = default;
  MaskFilterInfoExt(const gfx::MaskFilterInfo& mask_filter_info_arg,
                    bool is_fast_rounded_corner_arg,
                    const gfx::Transform target_transform)
      : mask_filter_info(mask_filter_info_arg),
        is_fast_rounded_corner(is_fast_rounded_corner_arg) {
    if (mask_filter_info.IsEmpty())
      return;
    bool success = mask_filter_info.Transform(target_transform);
    DCHECK(success);
  }

  gfx::MaskFilterInfo mask_filter_info;
  bool is_fast_rounded_corner;
};

struct SurfaceAggregator::RenderPassMapEntry {
  explicit RenderPassMapEntry(CompositorRenderPass* render_pass)
      : render_pass(render_pass) {}

  // Make this move-only.
  RenderPassMapEntry(RenderPassMapEntry&&) = default;
  RenderPassMapEntry(const RenderPassMapEntry&) = delete;
  RenderPassMapEntry& operator=(RenderPassMapEntry&&) = default;
  RenderPassMapEntry& operator=(const RenderPassMapEntry&) = delete;

  CompositorRenderPass* render_pass;
  bool is_visited = false;
};

SurfaceAggregator::SurfaceAggregator(SurfaceManager* manager,
                                     DisplayResourceProvider* provider,
                                     bool aggregate_only_damaged,
                                     bool needs_surface_damage_rect_list)
    : manager_(manager),
      provider_(provider),
      aggregate_only_damaged_(aggregate_only_damaged),
      needs_surface_damage_rect_list_(needs_surface_damage_rect_list),
      de_jelly_enabled_(DeJellyEnabled()) {
  DCHECK(manager_);
}

SurfaceAggregator::~SurfaceAggregator() {
  // Notify client of all surfaces being removed.
  contained_surfaces_.clear();
  contained_frame_sinks_.clear();
  ProcessAddedAndRemovedSurfaces();
}

// static
base::flat_map<CompositorRenderPassId, SurfaceAggregator::RenderPassMapEntry>
SurfaceAggregator::GenerateRenderPassMap(
    const CompositorRenderPassList& render_pass_list,
    bool is_root_surface) {
  const auto* root_pass_in_root_surface =
      is_root_surface ? render_pass_list.back().get() : nullptr;
  // This data is created once and typically small or empty. Collect all items
  // and pass to a flat_map to sort once.
  std::vector<std::pair<CompositorRenderPassId, RenderPassMapEntry>>
      render_pass_data;
  render_pass_data.reserve(render_pass_list.size());
  for (const auto& render_pass : render_pass_list) {
    if (render_pass->backdrop_filters.HasFilterThatMovesPixels()) {
      DCHECK_NE(render_pass.get(), root_pass_in_root_surface)
          << "The root render pass on the root surface can not have backdrop "
             "affecting filters";
    }
    render_pass_data.emplace_back(std::piecewise_construct,
                                  std::forward_as_tuple(render_pass->id),
                                  std::forward_as_tuple(render_pass.get()));
  }
  return base::flat_map<CompositorRenderPassId, RenderPassMapEntry>(
      std::move(render_pass_data));
}

// Create a clip rect for an aggregated quad from the original clip rect and
// the clip rect from the surface it's on.
SurfaceAggregator::ClipData SurfaceAggregator::CalculateClipRect(
    const ClipData& surface_clip,
    const ClipData& quad_clip,
    const gfx::Transform& target_transform) {
  ClipData out_clip;
  if (surface_clip.is_clipped)
    out_clip = surface_clip;

  if (quad_clip.is_clipped) {
    // TODO(jamesr): This only works if target_transform maps integer
    // rects to integer rects.
    gfx::Rect final_clip =
        cc::MathUtil::MapEnclosingClippedRect(target_transform, quad_clip.rect);
    if (out_clip.is_clipped)
      out_clip.rect.Intersect(final_clip);
    else
      out_clip.rect = final_clip;
    out_clip.is_clipped = true;
  }

  return out_clip;
}

int SurfaceAggregator::ChildIdForSurface(Surface* surface) {
  auto it = surface_id_to_resource_child_id_.find(surface->surface_id());
  if (it == surface_id_to_resource_child_id_.end()) {
    int child_id = provider_->CreateChild(base::BindRepeating(
        &SurfaceAggregator::UnrefResources, surface->client()));
    surface_id_to_resource_child_id_[surface->surface_id()] = child_id;
    return child_id;
  } else {
    return it->second;
  }
}

bool SurfaceAggregator::IsSurfaceFrameIndexSameAsPrevious(
    const Surface* surface) const {
  auto it = previous_contained_surfaces_.find(surface->surface_id());
  if (it != previous_contained_surfaces_.end()) {
    uint64_t previous_index = it->second;
    if (previous_index == surface->GetActiveFrameIndex())
      return true;
  }
  return false;
}

gfx::Rect SurfaceAggregator::DamageRectForSurface(
    const Surface* surface,
    const CompositorRenderPass& source,
    const gfx::Rect& full_rect) const {
  if (IsSurfaceFrameIndexSameAsPrevious(surface))
    return gfx::Rect();

  auto it = previous_contained_surfaces_.find(surface->surface_id());
  const SurfaceId& previous_surface_id = surface->previous_frame_surface_id();

  if (surface->surface_id() != previous_surface_id) {
    it = previous_contained_surfaces_.find(previous_surface_id);
  }
  if (it != previous_contained_surfaces_.end()) {
    uint64_t previous_index = it->second;
    if (previous_index == surface->GetActiveFrameIndex() - 1)
      return source.damage_rect;
  }

  return full_rect;
}

// This function is called at each render pass - CopyQuadsToPass().
void SurfaceAggregator::AddRenderPassFilterDamageToDamageList(
    const gfx::Transform& parent_target_transform,
    const CompositorRenderPass* source_pass,
    AggregatedRenderPass* dest_pass) {
  // Add damages from render passes with pixel-moving foreground filters or
  // backdrop filters to the surface damage list.
  if (!source_pass->filters.HasFilterThatMovesPixels() &&
      !source_pass->backdrop_filters.HasFilterThatMovesPixels()) {
    return;
  }

  gfx::Transform parent_to_root_target_transform = gfx::Transform(
      dest_pass->transform_to_root_target, parent_target_transform);

  gfx::Rect damage_rect = source_pass->output_rect;
  if (source_pass->filters.HasFilterThatMovesPixels()) {
    float max_pixel_movement = source_pass->filters.MaximumPixelMovement();
    gfx::RectF damage_rect_f(damage_rect);
    damage_rect_f.Inset(-max_pixel_movement, -max_pixel_movement);
    damage_rect = gfx::ToEnclosingRect(damage_rect_f);
  }

  gfx::Rect damage_rect_in_root_target_space =
      cc::MathUtil::MapEnclosingClippedRect(parent_to_root_target_transform,
                                            damage_rect);

  // The whole render pass rect with pixel-moving foreground filters or
  // backdrop filters is considered damaged if it intersects with the other
  // damages.
  if (damage_rect_in_root_target_space.Intersects(root_damage_rect_)) {
    // Transform will be performed again in AddSurfaceDamageToDamageList()
    // Just pass in damage_rect instead of damage_rect_in_root_target_space.
    AddSurfaceDamageToDamageList(damage_rect, gfx::Transform(), {}, source_pass,
                                 dest_pass, /*surface=*/nullptr);
  }
}

// This is different from the |root_damage_rect_| which is the union of all
// surface damages. This function records per-surface damage rects to
// |surface_damage_rect_list_| in a top-to-bottom order.
// it's called at each surface in the frame.
void SurfaceAggregator::AddSurfaceDamageToDamageList(
    const gfx::Rect& default_damage_rect,
    const gfx::Transform& parent_target_transform,
    const ClipData& clip_rect,
    const CompositorRenderPass* source_pass,
    AggregatedRenderPass* dest_pass,
    Surface* surface) {
  gfx::Rect damage_rect;
  if (!surface) {
    // When the surface is null, it's either the surface is lost or it comes
    // from a render pass with filters.
    damage_rect = default_damage_rect;
  } else {
    if (RenderPassNeedsFullDamage(dest_pass->id,
                                  dest_pass->cache_render_pass)) {
      damage_rect = source_pass->output_rect;
    } else {
      damage_rect =
          DamageRectForSurface(surface, *source_pass, source_pass->output_rect);
    }
  }

  if (damage_rect.IsEmpty()) {
    current_zero_damage_rect_is_not_recorded_ = true;
    return;
  }
  current_zero_damage_rect_is_not_recorded_ = false;

  gfx::Transform parent_to_root_target_transform = gfx::Transform(
      dest_pass->transform_to_root_target, parent_target_transform);

  gfx::Rect damage_rect_in_root_target_space =
      cc::MathUtil::MapEnclosingClippedRect(parent_to_root_target_transform,
                                            damage_rect);

  if (clip_rect.is_clipped) {
    gfx::Rect root_clip_rect = cc::MathUtil::MapEnclosingClippedRect(
        dest_pass->transform_to_root_target, clip_rect.rect);
    damage_rect_in_root_target_space.Intersect(root_clip_rect);
  }

  surface_damage_rect_list_->push_back(damage_rect_in_root_target_space);
}

// This function returns the overlay candidate quad ptr which has an
// overlay_damage_index pointing to the its damage rect in
// surface_damage_rect_list_. |overlay_damage_index| will be saved in the shared
// quad state later.
// This function is called at CopyQuadsToPass().
const DrawQuad* SurfaceAggregator::FindQuadWithOverlayDamage(
    const CompositorRenderPass& source_pass,
    AggregatedRenderPass* dest_pass,
    const gfx::Transform& parent_target_transform,
    const SurfaceId& surface_id,
    const ClipData& clip_rect,
    size_t* overlay_damage_index) {
  Surface* surface = manager_->GetSurfaceForId(surface_id);

  // Only process the damage rect at the root render pass, once per surface.
  const CompositorFrame& frame = surface->GetActiveFrame();
  bool is_last_pass_on_src_surface =
      &source_pass == frame.render_pass_list.back().get();
  if (!is_last_pass_on_src_surface)
    return nullptr;

  // The occluding damage optimization currently relies on two things - there
  // can't be any damage above the quad within the surface, and the quad needs
  // its own SQS for the occluding_damage_rect metadata.
  const DrawQuad* target_quad = nullptr;
  if (source_pass.quad_list.size() == 1) {
    // If there's only one quad in the root render pass, then the conditions
    // are clearly satisfied.
    target_quad = source_pass.quad_list.back();
  } else {
    // If there are multiple quads in the surface, if exactly one quad is
    // marked as having damage, then we know that quad doesn't have damage
    // above it, and we know that it has its own SQS (because its
    // sqs->no_damage is unique).
    for (auto* quad : source_pass.quad_list) {
      if (quad->shared_quad_state->no_damage) {
        continue;
      }

      if (target_quad == nullptr) {
        target_quad = quad;
      } else {
        target_quad = nullptr;
        break;
      }
    }
  }

  // No overlay candidate is found.
  if (!target_quad)
    return nullptr;

  // Zero damage is not recorded in the surface_damage_rect_list_.
  // In this case, add an empty damage rect to the list so
  // |overlay_damage_index| can save this index.
  if (current_zero_damage_rect_is_not_recorded_) {
    current_zero_damage_rect_is_not_recorded_ = false;
    surface_damage_rect_list_->push_back(gfx::Rect());
  }

  // The latest surface damage rect.
  *overlay_damage_index = surface_damage_rect_list_->size() - 1;

  return target_quad;
}

bool SurfaceAggregator::RenderPassNeedsFullDamage(
    const AggregatedRenderPassId& id,
    bool cache_render_pass) const {
  return cache_render_pass || copy_request_passes_.count(id) ||
         moved_pixel_passes_.count(id);
}

// static
void SurfaceAggregator::UnrefResources(
    base::WeakPtr<SurfaceClient> surface_client,
    const std::vector<ReturnedResource>& resources) {
  if (surface_client)
    surface_client->UnrefResources(resources);
}

bool SurfaceAggregator::CanPotentiallyMergePass(
    const SurfaceDrawQuad& surface_quad) {
  const SharedQuadState* sqs = surface_quad.shared_quad_state;
  return surface_quad.allow_merge &&
         base::IsApproximatelyEqual(sqs->opacity, 1.f, kOpacityEpsilon) &&
         sqs->de_jelly_delta_y == 0;
}

void SurfaceAggregator::HandleSurfaceQuad(
    const SurfaceDrawQuad* surface_quad,
    float parent_device_scale_factor,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    AggregatedRenderPass* dest_pass,
    bool ignore_undamaged,
    gfx::Rect* damage_rect_in_quad_space,
    bool* damage_rect_in_quad_space_valid,
    const MaskFilterInfoExt& mask_filter_info) {
  SurfaceId primary_surface_id = surface_quad->surface_range.end();
  Surface* latest_surface =
      manager_->GetLatestInFlightSurface(surface_quad->surface_range);

  // If a new surface is going to be emitted, add the surface_quad rect to
  // |surface_damage_rect_list_| for overlays. The whole quad is considered
  // damaged.
  if (needs_surface_damage_rect_list_ &&
      (!latest_surface || !latest_surface->HasActiveFrame() ||
       (latest_surface->surface_id() != primary_surface_id))) {
    gfx::Transform transform(
        target_transform,
        surface_quad->shared_quad_state->quad_to_target_transform);

    AddSurfaceDamageToDamageList(
        /*default_damage_rect=*/surface_quad->rect, transform, clip_rect,
        /*source_pass =*/nullptr, dest_pass, /*surface=*/nullptr);
  }

  // If there's no fallback surface ID available, then simply emit a
  // SolidColorDrawQuad with the provided default background color. This
  // can happen after a Viz process crash.
  if (!latest_surface || !latest_surface->HasActiveFrame()) {
    EmitDefaultBackgroundColorQuad(surface_quad, target_transform, clip_rect,
                                   dest_pass, mask_filter_info);
    return;
  }

  if (latest_surface->surface_id() != primary_surface_id &&
      !surface_quad->stretch_content_to_fill_bounds) {
    const CompositorFrame& fallback_frame = latest_surface->GetActiveFrame();

    gfx::Rect fallback_rect(latest_surface->GetActiveFrame().size_in_pixels());

    float scale_ratio =
        parent_device_scale_factor / fallback_frame.device_scale_factor();
    fallback_rect =
        gfx::ScaleToEnclosingRect(fallback_rect, scale_ratio, scale_ratio);
    fallback_rect = gfx::IntersectRects(fallback_rect, surface_quad->rect);

    EmitGutterQuadsIfNecessary(surface_quad->rect, fallback_rect,
                               surface_quad->shared_quad_state,
                               target_transform, clip_rect,
                               fallback_frame.metadata.root_background_color,
                               dest_pass, mask_filter_info);
  }

  EmitSurfaceContent(latest_surface, parent_device_scale_factor, surface_quad,
                     target_transform, clip_rect, dest_pass, ignore_undamaged,
                     damage_rect_in_quad_space, damage_rect_in_quad_space_valid,
                     mask_filter_info);
}

void SurfaceAggregator::EmitSurfaceContent(
    Surface* surface,
    float parent_device_scale_factor,
    const SurfaceDrawQuad* surface_quad,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    AggregatedRenderPass* dest_pass,
    bool ignore_undamaged,
    gfx::Rect* damage_rect_in_quad_space,
    bool* damage_rect_in_quad_space_valid,
    const MaskFilterInfoExt& mask_filter_info) {
  // If this surface's id is already in our referenced set then it creates
  // a cycle in the graph and should be dropped.
  SurfaceId surface_id = surface->surface_id();
  if (referenced_surfaces_.count(surface_id))
    return;

  // If we are stretching content to fill the SurfaceDrawQuad, or if the device
  // scale factor mismatches between content and SurfaceDrawQuad, we appply an
  // additional scale.
  float extra_content_scale_x, extra_content_scale_y;
  if (surface_quad->stretch_content_to_fill_bounds) {
    const gfx::Rect& source_rect = surface_quad->rect;
    // Stretches the surface contents to exactly fill the layer bounds,
    // regardless of scale or aspect ratio differences.
    extra_content_scale_x =
        source_rect.width() /
        static_cast<float>(surface->GetActiveFrame().size_in_pixels().width());
    extra_content_scale_y =
        source_rect.height() /
        static_cast<float>(surface->GetActiveFrame().size_in_pixels().height());
  } else {
    extra_content_scale_x = extra_content_scale_y =
        parent_device_scale_factor /
        surface->GetActiveFrame().device_scale_factor();
  }
  float inverse_extra_content_scale_x = SK_Scalar1 / extra_content_scale_x;
  float inverse_extra_content_scale_y = SK_Scalar1 / extra_content_scale_y;

  const SharedQuadState* source_sqs = surface_quad->shared_quad_state;
  gfx::Transform scaled_quad_to_target_transform(
      source_sqs->quad_to_target_transform);
  scaled_quad_to_target_transform.Scale(extra_content_scale_x,
                                        extra_content_scale_y);

  const CompositorFrame& frame = surface->GetActiveFrame();
  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Graphics.Pipeline",
      TRACE_ID_GLOBAL(frame.metadata.begin_frame_ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SurfaceAggregation", "display_trace", display_trace_id_);

  const gfx::Rect& source_visible_rect = surface_quad->visible_rect;
  if (ignore_undamaged) {
    gfx::Transform quad_to_target_transform(
        target_transform, source_sqs->quad_to_target_transform);
    *damage_rect_in_quad_space_valid = CalculateQuadSpaceDamageRect(
        quad_to_target_transform, dest_pass->transform_to_root_target,
        root_damage_rect_, damage_rect_in_quad_space);
    if (*damage_rect_in_quad_space_valid &&
        !damage_rect_in_quad_space->Intersects(source_visible_rect)) {
      return;
    }
  }

  // A map keyed by RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);

  const CompositorRenderPassList& render_pass_list = frame.render_pass_list;
  if (!valid_surfaces_.count(surface_id)) {
    // As |copy_requests| goes out-of-scope, all copy requests in that container
    // will auto-send an empty result upon destruction.
    return;
  }

  referenced_surfaces_.insert(surface_id);
  // TODO(vmpstr): provider check is a hack for unittests that don't set up a
  // resource provider.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> empty_map;
  const auto& child_to_parent_map =
      provider_ ? provider_->GetChildToParentMap(ChildIdForSurface(surface))
                : empty_map;
  gfx::Transform combined_transform = scaled_quad_to_target_transform;
  combined_transform.ConcatTransform(target_transform);

  // If the SurfaceDrawQuad is marked as being reflected and surface contents
  // are going to be scaled then keep the RenderPass. This allows the reflected
  // surface to be drawn with AA enabled for smooth scaling and preserves the
  // original reflector scaling behaviour which scaled a TextureLayer.
  bool reflected_and_scaled =
      surface_quad->is_reflection &&
      !scaled_quad_to_target_transform.IsIdentityOrTranslation();

  // We cannot merge passes if de-jelly is being applied, as we must have a
  // renderpass to skew.
  bool merge_pass =
      CanPotentiallyMergePass(*surface_quad) && !reflected_and_scaled &&
      copy_requests.empty() && combined_transform.Preserves2dAxisAlignment() &&
      CanMergeMaskFilterInfo(mask_filter_info, *render_pass_list.back());

  ClipData quads_clip;
  if (merge_pass) {
    // Intersect the transformed visible rect and the clip rect to create a
    // smaller cliprect for the quad.
    ClipData surface_quad_clip_rect = {
        true, cc::MathUtil::MapEnclosingClippedRect(
                  source_sqs->quad_to_target_transform, source_visible_rect)};
    if (source_sqs->is_clipped) {
      surface_quad_clip_rect.rect.Intersect(source_sqs->clip_rect);
    }

    quads_clip =
        CalculateClipRect(clip_rect, surface_quad_clip_rect, target_transform);
  }

  if (needs_surface_damage_rect_list_) {
    AddSurfaceDamageToDamageList(
        /*default_damage_rect=*/gfx::Rect(), combined_transform, quads_clip,
        /*source_pass =*/render_pass_list.back().get(), dest_pass, surface);
  }

  if (frame.metadata.delegated_ink_metadata) {
    // The metadata must be taken off of the surface, rather than a copy being
    // made, in order to ensure that the delegated ink metadata is used for
    // exactly one frame. Otherwise, it could potentially end up being used to
    // draw the same trail on multiple frames if a new CompositorFrame wasn't
    // generated.
    TransformAndStoreDelegatedInkMetadata(
        gfx::Transform(dest_pass->transform_to_root_target, combined_transform),
        surface->TakeDelegatedInkMetadata());
  }

  const CompositorRenderPassList& referenced_passes = render_pass_list;
  // TODO(fsamuel): Move this to a separate helper function.
  size_t passes_to_copy =
      merge_pass ? referenced_passes.size() - 1 : referenced_passes.size();
  for (size_t j = 0; j < passes_to_copy; ++j) {
    const CompositorRenderPass& source = *referenced_passes[j];

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    auto copy_pass = std::make_unique<AggregatedRenderPass>(sqs_size, dq_size);

    auto remapped_pass_id = pass_id_remapper_.Remap(source.id, surface_id);

    gfx::Rect output_rect = source.output_rect;
    if (max_render_target_size_ > 0) {
      output_rect.set_width(
          std::min(output_rect.width(), max_render_target_size_));
      output_rect.set_height(
          std::min(output_rect.height(), max_render_target_size_));
    }
    copy_pass->SetAll(
        remapped_pass_id, output_rect, output_rect,
        source.transform_to_root_target, source.filters,
        source.backdrop_filters, source.backdrop_filter_bounds,
        root_content_color_usage_, source.has_transparent_background,
        source.cache_render_pass, source.has_damage_from_contributing_content,
        source.generate_mipmap);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    // Contributing passes aggregated in to the pass list need to take the
    // transform of the surface quad into account to update their transform to
    // the root surface.
    copy_pass->transform_to_root_target.ConcatTransform(
        scaled_quad_to_target_transform);
    copy_pass->transform_to_root_target.ConcatTransform(target_transform);
    copy_pass->transform_to_root_target.ConcatTransform(
        dest_pass->transform_to_root_target);

    CopyQuadsToPass(source, copy_pass.get(), frame.device_scale_factor(),
                    child_to_parent_map, gfx::Transform(), {}, surface_id,
                    MaskFilterInfoExt());

    // If the render pass has copy requests, or should be cached, or has
    // moving-pixel filters, or in a moving-pixel surface, we should damage the
    // whole output rect so that we always drawn the full content. Otherwise, we
    // might have incompleted copy request, or cached patially drawn render
    // pass.
    if (!RenderPassNeedsFullDamage(copy_pass->id,
                                   copy_pass->cache_render_pass)) {
      gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
      if (copy_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
        gfx::Rect damage_rect_in_render_pass_space =
            cc::MathUtil::ProjectEnclosingClippedRect(inverse_transform,
                                                      root_damage_rect_);
        copy_pass->damage_rect.Intersect(damage_rect_in_render_pass_space);
      }
    }

    if (copy_pass->has_damage_from_contributing_content)
      contributing_content_damaged_passes_.insert(copy_pass->id);
    dest_pass_list_->push_back(std::move(copy_pass));
  }

  const auto& last_pass = *render_pass_list.back();
  // This will check if all the surface_quads (including child surfaces) has
  // damage because HandleSurfaceQuad is a recursive call by calling
  // CopyQuadsToPass in it.
  dest_pass->has_damage_from_contributing_content |=
      !DamageRectForSurface(surface, last_pass, last_pass.output_rect)
           .IsEmpty();

  if (merge_pass) {
    CopyQuadsToPass(last_pass, dest_pass, frame.device_scale_factor(),
                    child_to_parent_map, combined_transform, quads_clip,
                    surface_id, mask_filter_info);
  } else {
    auto* shared_quad_state = CopyAndScaleSharedQuadState(
        source_sqs, scaled_quad_to_target_transform, target_transform,
        gfx::ScaleToEnclosingRect(source_sqs->quad_layer_rect,
                                  inverse_extra_content_scale_x,
                                  inverse_extra_content_scale_y),
        gfx::ScaleToEnclosingRect(source_sqs->visible_quad_layer_rect,
                                  inverse_extra_content_scale_x,
                                  inverse_extra_content_scale_y),
        clip_rect, dest_pass, mask_filter_info);

    // At this point, we need to calculate three values in order to construct
    // the CompositorRenderPassDrawQuad:

    // |quad_rect| - A rectangle representing the RenderPass's output area in
    //   content space. This is equal to the root render pass (|last_pass|)
    //   output rect.
    gfx::Rect quad_rect = last_pass.output_rect;

    // |quad_visible_rect| - A rectangle representing the visible portion of
    //   the RenderPass, in content space. As the SurfaceDrawQuad being
    //   embedded may be clipped further than its root render pass, we use the
    //   surface quad's value - |source_visible_rect|.
    //
    //   There may be an |extra_content_scale_x| applied when going from this
    //   render pass's content space to the surface's content space, we remove
    //   this so that |quad_visible_rect| is in the render pass's content
    //   space.
    gfx::Rect quad_visible_rect(gfx::ScaleToEnclosingRect(
        source_visible_rect, inverse_extra_content_scale_x,
        inverse_extra_content_scale_y));

    // |tex_coord_rect| - A rectangle representing the bounds of the texture
    //   in the RenderPass's |quad_rect|. Not in content space, instead as an
    //   offset within |quad_rect|.
    gfx::RectF tex_coord_rect = gfx::RectF(gfx::SizeF(quad_rect.size()));

    // We can't produce content outside of |quad_rect|, so clip the visible
    // rect if necessary.
    quad_visible_rect.Intersect(quad_rect);
    auto remapped_pass_id = pass_id_remapper_.Remap(last_pass.id, surface_id);
    if (quad_visible_rect.IsEmpty()) {
      dest_pass_list_->erase(
          std::remove_if(
              dest_pass_list_->begin(), dest_pass_list_->end(),
              [&remapped_pass_id](
                  const std::unique_ptr<AggregatedRenderPass>& pass) {
                return pass->id == remapped_pass_id;
              }),
          dest_pass_list_->end());
    } else {
      auto* quad =
          dest_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      quad->SetNew(shared_quad_state, quad_rect, quad_visible_rect,
                   remapped_pass_id, kInvalidResourceId, gfx::RectF(),
                   gfx::Size(), gfx::Vector2dF(), gfx::PointF(), tex_coord_rect,
                   /*force_anti_aliasing_off=*/false,
                   /* backdrop_filter_quality*/ 1.0f);
    }
  }

  referenced_surfaces_.erase(surface_id);
}

void SurfaceAggregator::EmitDefaultBackgroundColorQuad(
    const SurfaceDrawQuad* surface_quad,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    AggregatedRenderPass* dest_pass,
    const MaskFilterInfoExt& mask_filter_info) {
  // The primary surface is unavailable and there is no fallback
  // surface specified so create a SolidColorDrawQuad with the default
  // background color.
  SkColor background_color = surface_quad->default_background_color;
  auto* shared_quad_state =
      CopySharedQuadState(surface_quad->shared_quad_state, target_transform,
                          clip_rect, dest_pass, mask_filter_info);

  auto* solid_color_quad =
      dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_color_quad->SetNew(shared_quad_state, surface_quad->rect,
                           surface_quad->visible_rect, background_color, false);
}

void SurfaceAggregator::EmitGutterQuadsIfNecessary(
    const gfx::Rect& primary_rect,
    const gfx::Rect& fallback_rect,
    const SharedQuadState* primary_shared_quad_state,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    SkColor background_color,
    AggregatedRenderPass* dest_pass,
    const MaskFilterInfoExt& mask_filter_info) {
  bool has_transparent_background = background_color == SK_ColorTRANSPARENT;

  // If the fallback Surface's active CompositorFrame has a non-transparent
  // background then compute gutter.
  if (has_transparent_background)
    return;

  if (fallback_rect.width() < primary_rect.width()) {
    // The right gutter also includes the bottom-right corner, if necessary.
    gfx::Rect right_gutter_rect(fallback_rect.right(), primary_rect.y(),
                                primary_rect.width() - fallback_rect.width(),
                                primary_rect.height());

    SharedQuadState* shared_quad_state = CopyAndScaleSharedQuadState(
        primary_shared_quad_state,
        primary_shared_quad_state->quad_to_target_transform, target_transform,
        right_gutter_rect, right_gutter_rect, clip_rect, dest_pass,
        mask_filter_info);

    auto* right_gutter =
        dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    right_gutter->SetNew(shared_quad_state, right_gutter_rect,
                         right_gutter_rect, background_color, false);
  }

  if (fallback_rect.height() < primary_rect.height()) {
    gfx::Rect bottom_gutter_rect(
        primary_rect.x(), fallback_rect.bottom(), fallback_rect.width(),
        primary_rect.height() - fallback_rect.height());

    SharedQuadState* shared_quad_state = CopyAndScaleSharedQuadState(
        primary_shared_quad_state,
        primary_shared_quad_state->quad_to_target_transform, target_transform,
        bottom_gutter_rect, bottom_gutter_rect, clip_rect, dest_pass,
        mask_filter_info);

    auto* bottom_gutter =
        dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bottom_gutter->SetNew(shared_quad_state, bottom_gutter_rect,
                          bottom_gutter_rect, background_color, false);
  }
}

void SurfaceAggregator::AddColorConversionPass() {
  auto* root_render_pass = dest_pass_list_->back().get();
  gfx::Rect output_rect = root_render_pass->output_rect;

  // An extra color conversion pass is only done if the display's color
  // space is unsuitable as a blending color space.
  bool needs_color_conversion_pass =
      !display_color_spaces_
           .GetOutputColorSpace(root_render_pass->content_color_usage,
                                root_render_pass->has_transparent_background)
           .IsSuitableForBlending();

  // If we added or removed the color conversion pass, we need to add full
  // damage to the current-root renderpass (and also the new-root renderpass,
  // if the current-root renderpass becomes and intermediate renderpass).
  if (needs_color_conversion_pass != last_frame_had_color_conversion_pass_)
    root_render_pass->damage_rect = output_rect;

  last_frame_had_color_conversion_pass_ = needs_color_conversion_pass;
  if (!needs_color_conversion_pass)
    return;
  CHECK(root_render_pass->transform_to_root_target == gfx::Transform());

  if (!color_conversion_render_pass_id_)
    color_conversion_render_pass_id_ = pass_id_remapper_.NextAvailableId();

  auto color_conversion_pass = std::make_unique<AggregatedRenderPass>(1, 1);
  color_conversion_pass->SetNew(color_conversion_render_pass_id_, output_rect,
                                root_render_pass->damage_rect,
                                root_render_pass->transform_to_root_target);
  color_conversion_pass->has_transparent_background =
      root_render_pass->has_transparent_background;
  color_conversion_pass->content_color_usage = root_content_color_usage_;
  color_conversion_pass->is_color_conversion_pass = true;

  auto* shared_quad_state =
      color_conversion_pass->CreateAndAppendSharedQuadState();
  // Do NOT set blend mode here to SkBlendMode::kSrcOver, which will cause
  // blending with empty (black) root pass when child pass has alpha.
  shared_quad_state->SetAll(
      /*quad_to_target_transform=*/gfx::Transform(),
      /*quad_layer_rect=*/output_rect,
      /*visible_quad_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false, /*are_contents_opaque=*/false, /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrc, /*sorting_context_id=*/0);

  auto* quad = color_conversion_pass
                   ->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, output_rect, output_rect,
               root_render_pass->id, kInvalidResourceId, gfx::RectF(),
               gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF(output_rect),
               /*force_anti_aliasing_off=*/false,
               /*backdrop_filter_quality*/ 1.0f);
  dest_pass_list_->push_back(std::move(color_conversion_pass));
}

void SurfaceAggregator::AddDisplayTransformPass() {
  if (dest_pass_list_->empty())
    return;

  auto* root_render_pass = dest_pass_list_->back().get();
  gfx::Rect output_rect = root_render_pass->output_rect;
  DCHECK(root_render_pass->transform_to_root_target == root_surface_transform_);

  if (!display_transform_render_pass_id_)
    display_transform_render_pass_id_ = pass_id_remapper_.NextAvailableId();

  auto display_transform_pass = std::make_unique<AggregatedRenderPass>(1, 1);
  display_transform_pass->SetAll(
      display_transform_render_pass_id_,
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, root_render_pass->output_rect),
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, root_render_pass->damage_rect),
      gfx::Transform(),
      /*filters=*/cc::FilterOperations(),
      /*backdrop_filters=*/cc::FilterOperations(),
      /*backdrop_filter_bounds=*/gfx::RRectF(),
      root_render_pass->content_color_usage,
      root_render_pass->has_transparent_background,
      /*cache_render_pass=*/false,
      /*has_damage_from_contributing_content=*/false,
      /*generate_mipmap=*/false);

  bool are_contents_opaque = true;
  for (const auto* sqs : root_render_pass->shared_quad_state_list) {
    if (!sqs->are_contents_opaque) {
      are_contents_opaque = false;
      break;
    }
  }

  auto* shared_quad_state =
      display_transform_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(
      /*quad_to_target_transform=*/root_surface_transform_,
      /*quad_layer_rect=*/output_rect,
      /*visible_quad_layer_rect=*/output_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false, are_contents_opaque, /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  auto* quad = display_transform_pass
                   ->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, output_rect, output_rect,
               root_render_pass->id, kInvalidResourceId, gfx::RectF(),
               gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF(output_rect),
               /*force_anti_aliasing_off=*/false,
               /*backdrop_filter_quality*/ 1.0f);
  dest_pass_list_->push_back(std::move(display_transform_pass));
}

SharedQuadState* SurfaceAggregator::CopySharedQuadState(
    const SharedQuadState* source_sqs,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    AggregatedRenderPass* dest_render_pass,
    const MaskFilterInfoExt& mask_filter_info) {
  return CopyAndScaleSharedQuadState(
      source_sqs, source_sqs->quad_to_target_transform, target_transform,
      source_sqs->quad_layer_rect, source_sqs->visible_quad_layer_rect,
      clip_rect, dest_render_pass, mask_filter_info);
}

SharedQuadState* SurfaceAggregator::CopyAndScaleSharedQuadState(
    const SharedQuadState* source_sqs,
    const gfx::Transform& scaled_quad_to_target_transform,
    const gfx::Transform& target_transform,
    const gfx::Rect& quad_layer_rect,
    const gfx::Rect& visible_quad_layer_rect,
    const ClipData& clip_rect,
    AggregatedRenderPass* dest_render_pass,
    const MaskFilterInfoExt& mask_filter_info_ext) {
  auto* shared_quad_state = dest_render_pass->CreateAndAppendSharedQuadState();
  ClipData new_clip_rect = CalculateClipRect(
      clip_rect, {source_sqs->is_clipped, source_sqs->clip_rect},
      target_transform);

  // target_transform contains any transformation that may exist
  // between the context that these quads are being copied from (i.e. the
  // surface's draw transform when aggregated from within a surface) to the
  // target space of the pass. This will be identity except when copying the
  // root draw pass from a surface into a pass when the surface draw quad's
  // transform is not identity.
  gfx::Transform new_transform = scaled_quad_to_target_transform;
  new_transform.ConcatTransform(target_transform);

  shared_quad_state->SetAll(
      new_transform, quad_layer_rect, visible_quad_layer_rect,
      mask_filter_info_ext.mask_filter_info, new_clip_rect.rect,
      new_clip_rect.is_clipped, source_sqs->are_contents_opaque,
      source_sqs->opacity, source_sqs->blend_mode,
      source_sqs->sorting_context_id);
  shared_quad_state->is_fast_rounded_corner =
      mask_filter_info_ext.is_fast_rounded_corner,
  shared_quad_state->de_jelly_delta_y = source_sqs->de_jelly_delta_y;

  return shared_quad_state;
}

void SurfaceAggregator::CopyQuadsToPass(
    const CompositorRenderPass& source_pass,
    AggregatedRenderPass* dest_pass,
    float parent_device_scale_factor,
    const std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>&
        child_to_parent_map,
    const gfx::Transform& target_transform,
    const ClipData& clip_rect,
    const SurfaceId& surface_id,
    const MaskFilterInfoExt& parent_mask_filter_info_ext) {
  const QuadList& source_quad_list = source_pass.quad_list;
  const SharedQuadState* last_copied_source_shared_quad_state = nullptr;

  // If the current frame has copy requests or cached render passes, then
  // aggregate the entire thing, as otherwise parts of the copy requests may be
  // ignored and we could cache partially drawn render pass.
  // If there are pixel-moving backdrop filters then the damage rect might be
  // expanded later, so we can't drop quads that are outside the current damage
  // rect safely.
  const bool ignore_undamaged =
      aggregate_only_damaged_ && !has_copy_requests_ &&
      !has_cached_render_passes_ && !has_pixel_moving_backdrop_filter_ &&
      !moved_pixel_passes_.count(dest_pass->id);
  // Damage rect in the quad space of the current shared quad state.
  // TODO(jbauman): This rect may contain unnecessary area if
  // transform isn't axis-aligned.
  gfx::Rect damage_rect_in_quad_space;
  bool damage_rect_in_quad_space_valid = false;

#if DCHECK_IS_ON()
  const SharedQuadStateList& source_shared_quad_state_list =
      source_pass.shared_quad_state_list;
  // If quads have come in with SharedQuadState out of order, or when quads have
  // invalid SharedQuadState pointer, it should DCHECK.
  auto sqs_iter = source_shared_quad_state_list.cbegin();
  for (auto* quad : source_quad_list) {
    while (sqs_iter != source_shared_quad_state_list.cend() &&
           quad->shared_quad_state != *sqs_iter) {
      ++sqs_iter;
    }
    DCHECK(sqs_iter != source_shared_quad_state_list.cend());
  }
#endif

  size_t overlay_damage_index = 0;
  const DrawQuad* quad_with_overlay_damage_index = nullptr;
  if (needs_surface_damage_rect_list_) {
    AddRenderPassFilterDamageToDamageList(target_transform, &source_pass,
                                          dest_pass);
    quad_with_overlay_damage_index =
        FindQuadWithOverlayDamage(source_pass, dest_pass, target_transform,
                                  surface_id, clip_rect, &overlay_damage_index);
  }

  MaskFilterInfoExt new_mask_filter_info_ext = parent_mask_filter_info_ext;
  for (auto* quad : source_quad_list) {
    // Both cannot be set at once. If this happens then a surface is being
    // merged when it should not.
    DCHECK(quad->shared_quad_state->mask_filter_info.IsEmpty() ||
           parent_mask_filter_info_ext.mask_filter_info.IsEmpty());

    if (quad->material == DrawQuad::Material::kSurfaceContent) {
      const auto* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
      // HandleSurfaceQuad may add other shared quad state, so reset the
      // current data.
      last_copied_source_shared_quad_state = nullptr;

      if (!surface_quad->surface_range.end().is_valid())
        continue;

      if (parent_mask_filter_info_ext.mask_filter_info.IsEmpty()) {
        new_mask_filter_info_ext = MaskFilterInfoExt(
            quad->shared_quad_state->mask_filter_info,
            quad->shared_quad_state->is_fast_rounded_corner, target_transform);
      }

      HandleSurfaceQuad(
          surface_quad, parent_device_scale_factor, target_transform, clip_rect,
          dest_pass, ignore_undamaged, &damage_rect_in_quad_space,
          &damage_rect_in_quad_space_valid, new_mask_filter_info_ext);
    } else {
      if (quad->shared_quad_state != last_copied_source_shared_quad_state) {
        if (parent_mask_filter_info_ext.mask_filter_info.IsEmpty()) {
          new_mask_filter_info_ext =
              MaskFilterInfoExt(quad->shared_quad_state->mask_filter_info,
                                quad->shared_quad_state->is_fast_rounded_corner,
                                target_transform);
        }
        SharedQuadState* dest_shared_quad_state =
            CopySharedQuadState(quad->shared_quad_state, target_transform,
                                clip_rect, dest_pass, new_mask_filter_info_ext);

        if (quad == quad_with_overlay_damage_index)
          dest_shared_quad_state->overlay_damage_index = overlay_damage_index;

        if (de_jelly_enabled_) {
          // If a surface is being drawn for a second time, clear our
          // |de_jelly_delta_y|, as de-jelly is only needed the first time
          // a surface draws.
          if (!new_surfaces_.count(surface_id))
            dest_shared_quad_state->de_jelly_delta_y = 0.0f;
        }

        last_copied_source_shared_quad_state = quad->shared_quad_state;
        if (ignore_undamaged) {
          damage_rect_in_quad_space_valid = CalculateQuadSpaceDamageRect(
              dest_shared_quad_state->quad_to_target_transform,
              dest_pass->transform_to_root_target, root_damage_rect_,
              &damage_rect_in_quad_space);
        }
      }

      if (ignore_undamaged) {
        if (damage_rect_in_quad_space_valid &&
            !damage_rect_in_quad_space.Intersects(quad->visible_rect))
          continue;
      }

      DrawQuad* dest_quad;
      if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
        const auto* pass_quad =
            CompositorRenderPassDrawQuad::MaterialCast(quad);
        CompositorRenderPassId original_pass_id = pass_quad->render_pass_id;
        AggregatedRenderPassId remapped_pass_id =
            pass_id_remapper_.Remap(original_pass_id, surface_id);

        // If the CompositorRenderPassDrawQuad is referring to other render pass
        // with the |has_damage_from_contributing_content| set on it, then the
        // dest_pass should have the flag set on it as well.
        if (contributing_content_damaged_passes_.count(remapped_pass_id))
          dest_pass->has_damage_from_contributing_content = true;

        dest_quad = dest_pass->CopyFromAndAppendRenderPassDrawQuad(
            pass_quad, remapped_pass_id);
      } else if (quad->material == DrawQuad::Material::kTextureContent) {
        const auto* texture_quad = TextureDrawQuad::MaterialCast(quad);
        if (texture_quad->secure_output_only &&
            (!output_is_secure_ || copy_request_passes_.count(dest_pass->id))) {
          auto* solid_color_quad =
              dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
          solid_color_quad->SetNew(dest_pass->shared_quad_state_list.back(),
                                   quad->rect, quad->visible_rect,
                                   SK_ColorBLACK, false);
          dest_quad = solid_color_quad;
        } else {
          dest_quad = dest_pass->CopyFromAndAppendDrawQuad(quad);
        }
      } else {
        dest_quad = dest_pass->CopyFromAndAppendDrawQuad(quad);
      }
      if (!child_to_parent_map.empty()) {
        for (ResourceId& resource_id : dest_quad->resources) {
          auto it = child_to_parent_map.find(resource_id);
          DCHECK(it != child_to_parent_map.end());

          DCHECK_EQ(it->first, resource_id);
          ResourceId remapped_id = it->second;
          resource_id = remapped_id;
        }
      }
    }
  }
}

void SurfaceAggregator::CopyPasses(const CompositorFrame& frame,
                                   Surface* surface) {
  // The root surface is allowed to have copy output requests, so grab them
  // off its render passes. This map contains a set of CopyOutputRequests
  // keyed by each RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);

  const auto& source_pass_list = frame.render_pass_list;
  DCHECK(valid_surfaces_.count(surface->surface_id()));
  if (!valid_surfaces_.count(surface->surface_id()))
    return;

  // TODO(vmpstr): provider check is a hack for unittests that don't set up a
  // resource provider.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> empty_map;
  const auto& child_to_parent_map =
      provider_ ? provider_->GetChildToParentMap(ChildIdForSurface(surface))
                : empty_map;

  const gfx::Transform surface_transform =
      IsRootSurface(surface) ? root_surface_transform_ : gfx::Transform();

  if (frame.metadata.delegated_ink_metadata) {
    DCHECK(surface->GetActiveFrameMetadata().delegated_ink_metadata ==
           frame.metadata.delegated_ink_metadata);
    // The metadata must be taken off of the surface, rather than a copy being
    // made, in order to ensure that the delegated ink metadata is used for
    // exactly one frame. Otherwise, it could potentially end up being used to
    // draw the same trail on multiple frames if a new CompositorFrame wasn't
    // generated.
    TransformAndStoreDelegatedInkMetadata(
        gfx::Transform(source_pass_list.back()->transform_to_root_target,
                       surface_transform),
        surface->TakeDelegatedInkMetadata());
  }

  bool apply_surface_transform_to_root_pass = true;
  for (size_t i = 0; i < source_pass_list.size(); ++i) {
    const auto& source = *source_pass_list[i];
    const bool is_root_pass = (i == source_pass_list.size() - 1);

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    auto copy_pass = std::make_unique<AggregatedRenderPass>(sqs_size, dq_size);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    // We add an additional render pass for the transform if the root render
    // pass has any copy requests.
    apply_surface_transform_to_root_pass =
        is_root_pass &&
        (copy_pass->copy_requests.empty() || surface_transform.IsIdentity());

    auto remapped_pass_id =
        pass_id_remapper_.Remap(source.id, surface->surface_id());

    gfx::Rect output_rect = source.output_rect;
    gfx::Transform transform_to_root_target = source.transform_to_root_target;
    if (apply_surface_transform_to_root_pass) {
      // If we don't need an additional render pass to apply the surface
      // transform, adjust the root pass's rects to account for it.
      output_rect = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          surface_transform, output_rect);
    } else {
      // For the non-root render passes, the transform to root target needs to
      // be adjusted to include the root surface transform. This is also true if
      // we will be adding another render pass for the surface transform, in
      // which this will no longer be the root.
      transform_to_root_target =
          gfx::Transform(surface_transform, source.transform_to_root_target);
    }

    copy_pass->SetAll(
        remapped_pass_id, output_rect, output_rect, transform_to_root_target,
        source.filters, source.backdrop_filters, source.backdrop_filter_bounds,
        root_content_color_usage_, source.has_transparent_background,
        source.cache_render_pass, source.has_damage_from_contributing_content,
        source.generate_mipmap);

    if (needs_surface_damage_rect_list_ && is_root_pass) {
      AddSurfaceDamageToDamageList(
          /*default_damage_rect=*/gfx::Rect(), surface_transform,
          /*clip_rect=*/{}, &source, copy_pass.get(), surface);
    }

    CopyQuadsToPass(source, copy_pass.get(), frame.device_scale_factor(),
                    child_to_parent_map,
                    apply_surface_transform_to_root_pass ? surface_transform
                                                         : gfx::Transform(),
                    {}, surface->surface_id(), MaskFilterInfoExt());

    // If the render pass has copy requests, or should be cached, or has
    // moving-pixel filters, or in a moving-pixel surface, we should damage the
    // whole output rect so that we always drawn the full content. Otherwise, we
    // might have incompleted copy request, or cached patially drawn render
    // pass.
    if (!RenderPassNeedsFullDamage(copy_pass->id,
                                   copy_pass->cache_render_pass)) {
      gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
      if (copy_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
        gfx::Rect damage_rect_in_render_pass_space =
            cc::MathUtil::ProjectEnclosingClippedRect(inverse_transform,
                                                      root_damage_rect_);
        copy_pass->damage_rect.Intersect(damage_rect_in_render_pass_space);
      }
    }

    if (copy_pass->has_damage_from_contributing_content)
      contributing_content_damaged_passes_.insert(copy_pass->id);
    dest_pass_list_->push_back(std::move(copy_pass));
  }

  if (!apply_surface_transform_to_root_pass)
    AddDisplayTransformPass();
}

void SurfaceAggregator::ProcessAddedAndRemovedSurfaces() {
  for (const auto& surface : previous_contained_surfaces_) {
    if (!contained_surfaces_.count(surface.first))
      // Release resources of removed surface.
      ReleaseResources(surface.first);
  }
}

gfx::Rect SurfaceAggregator::PrewalkRenderPass(
    RenderPassMapEntry* render_pass_entry,
    const Surface* surface,
    base::flat_map<CompositorRenderPassId, RenderPassMapEntry>* render_pass_map,
    bool will_draw,
    const gfx::Rect& damage_from_parent,
    const gfx::Transform& target_to_root_transform,
    bool in_moved_pixel_rp,
    PrewalkResult* result) {
  if (render_pass_entry->is_visited) {
    // This render pass is an ancestor of itself and is not supported.
    return gfx::Rect();
  }

  base::AutoReset<bool> reset_visited(&render_pass_entry->is_visited, true);
  const CompositorRenderPass& render_pass = *render_pass_entry->render_pass;

  if (render_pass.backdrop_filters.HasFilterThatMovesPixels()) {
    has_pixel_moving_backdrop_filter_ = true;
  }

  auto remapped_pass_id =
      pass_id_remapper_.Remap(render_pass.id, surface->surface_id());
  // |moved_pixel_passes_| stores all the render passes affected by filters
  // that move pixels, so |in_moved_pixel_rp| should be set to true either
  // if the current render pass has pixel_moving_filter(s) or if it is inside an
  // ancestor render pass that has pixel_moving_filter(s).
  in_moved_pixel_rp |= render_pass.filters.HasFilterThatMovesPixels();
  if (in_moved_pixel_rp)
    moved_pixel_passes_.insert(remapped_pass_id);

  const CompositorFrame& frame = surface->GetActiveFrame();
  CompositorRenderPass* last_pass = frame.render_pass_list.back().get();
  gfx::Rect full_damage = last_pass->output_rect;

  // The damage on the root render pass of the surface comes from damage
  // accumulated from all quads in the surface, and needs to be expanded by any
  // pixel-moving backdrop filter in the render pass if intersecting. Transform
  // this damage into the local space of the render pass for this purpose.
  gfx::Rect surface_root_rp_damage =
      DamageRectForSurface(surface, *last_pass, full_damage);
  if (!surface_root_rp_damage.IsEmpty()) {
    gfx::Transform root_to_target_transform(
        gfx::Transform::kSkipInitialization);
    if (target_to_root_transform.GetInverse(&root_to_target_transform)) {
      surface_root_rp_damage = cc::MathUtil::ProjectEnclosingClippedRect(
          root_to_target_transform, surface_root_rp_damage);
    }
  }

  gfx::Rect damage_rect;
  // Iterate through the quad list back-to-front and accumulate damage from
  // all quads (only SurfaceDrawQuads and RenderPassDrawQuads can have damage
  // at this point). |damage_rect| has damage from all quads below the current
  // iterated quad, and can be used to determine if there's any intersection
  // with the current quad when needed.
  for (QuadList::ConstReverseIterator it = render_pass.quad_list.rbegin();
       it != render_pass.quad_list.rend(); ++it) {
    const DrawQuad* quad = *it;
    gfx::Rect quad_damage_rect;
    if (quad->material == DrawQuad::Material::kSurfaceContent) {
      const auto* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
      Surface* child_surface =
          manager_->GetLatestInFlightSurface(surface_quad->surface_range);
      // If the primary surface is not available then we assume the damage is
      // the full size of the SurfaceDrawQuad because we might need to introduce
      // gutter.
      if (!child_surface ||
          child_surface->surface_id() != surface_quad->surface_range.end()) {
        quad_damage_rect = quad->rect;
      }

      if (child_surface) {
        gfx::Rect child_rect;
        float x_scale = SK_Scalar1;
        float y_scale = SK_Scalar1;
        if (surface_quad->stretch_content_to_fill_bounds) {
          if (!child_surface->size_in_pixels().IsEmpty()) {
            x_scale = static_cast<float>(surface_quad->rect.width()) /
                      child_surface->size_in_pixels().width();
            y_scale = static_cast<float>(surface_quad->rect.height()) /
                      child_surface->size_in_pixels().height();
          }
        }
        // If the surface quad is to be merged potentially, the current
        // effective accumulated damage needs to be taken into account. This
        // includes the damage from quads under the surface quad, i.e.
        // |damage_rect|, |surface_root_rp_damage|, which can contain damage
        // contributed by quads under the surface quad in the previous stage
        // (cc), and |damage_from_parent|. The damage is first transformed into
        // the local space of the surface quad and then passed to the embedding
        // surface. The condition for deciding if the surface quad will merge is
        // loose here, so for those quads passed this condition but eventually
        // don't merge, there is over-contribution of the damage passed from
        // parent, but this shouldn't affect correctness.
        gfx::Rect accumulated_damage_in_child_space;

        if (CanPotentiallyMergePass(*surface_quad)) {
          accumulated_damage_in_child_space.Union(damage_rect);
          accumulated_damage_in_child_space.Union(damage_from_parent);
          accumulated_damage_in_child_space.Union(surface_root_rp_damage);
          if (!accumulated_damage_in_child_space.IsEmpty()) {
            gfx::Transform inverse(gfx::Transform::kSkipInitialization);
            bool inverted =
                quad->shared_quad_state->quad_to_target_transform.GetInverse(
                    &inverse);
            DCHECK(inverted);
            inverse.PostScale(SK_Scalar1 / x_scale, SK_Scalar1 / y_scale);
            accumulated_damage_in_child_space =
                cc::MathUtil::ProjectEnclosingClippedRect(
                    inverse, accumulated_damage_in_child_space);
          }
        }
        child_rect = PrewalkSurface(child_surface, in_moved_pixel_rp,
                                    remapped_pass_id, will_draw,
                                    accumulated_damage_in_child_space, result);
        child_rect = gfx::ScaleToEnclosingRect(child_rect, x_scale, y_scale);
        quad_damage_rect.Union(child_rect);
      }

      if (quad_damage_rect.IsEmpty())
        continue;
    } else if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
      auto* render_pass_quad = CompositorRenderPassDrawQuad::MaterialCast(quad);

      CompositorRenderPassId child_pass_id = render_pass_quad->render_pass_id;
      auto child_it = render_pass_map->find(child_pass_id);
      DCHECK(child_it != render_pass_map->end());
      RenderPassMapEntry& child_render_pass_entry = child_it->second;
      const CompositorRenderPass& child_render_pass =
          *child_render_pass_entry.render_pass;

      gfx::Rect rect_in_target_space = cc::MathUtil::MapEnclosingClippedRect(
          quad->shared_quad_state->quad_to_target_transform,
          child_render_pass.output_rect);

      // |damage_rect|, |damage_from_parent| and |surface_root_rp_damage|
      // either are or can possible contain damage from under the quad, so if
      // they intersect the quad render pass output rect, we have to invalidate
      // the |intersects_damage_under| flag. Note the intersection test can be
      // done against backdrop filter bounds as an improvement.
      bool intersects_current_damage =
          rect_in_target_space.Intersects(damage_rect);
      bool intersects_damage_from_parent =
          rect_in_target_space.Intersects(damage_from_parent);
      // The |intersects_damage_under| flag hints if the current quad intersects
      // any damage from any quads below in the same surface. If the flag is
      // false, it means the intersecting damage is from quads above it or from
      // itself.
      bool intersects_damage_from_surface =
          rect_in_target_space.Intersects(surface_root_rp_damage);
      if (intersects_current_damage || intersects_damage_from_parent ||
          intersects_damage_from_surface) {
        render_pass_quad->intersects_damage_under = true;

        if (child_render_pass.backdrop_filters.HasFilterThatMovesPixels()) {
          // The damage from under the quad intersects quad render pass output
          // rect and it has to be expanded because of the pixel-moving
          // backdrop filters. We expand the |damage_rect| to include quad
          // render pass output rect (which can be optimized to be backdrop
          // filter bounds). |damage_from_parent| and |surface_root_rp_damage|
          // only have to be included when they also have intersection with the
          // quad.
          damage_rect.Union(rect_in_target_space);
          if (intersects_damage_from_parent) {
            damage_rect.Union(damage_from_parent);
          }
          if (intersects_damage_from_surface) {
            damage_rect.Union(surface_root_rp_damage);
          }
        }
      }
      // For the pixel-moving backdrop filters, all effects are limited to the
      // size of the RenderPassDrawQuad rect. Therefore when we find the damage
      // under the quad intersects quad render pass output rect, we extend the
      // damage rect to include the rpdq->rect.

      // For the pixel-moving foreground filters, all effects can be expanded
      // outside the RenderPassDrawQuad rect to the size of rect +
      // filters.MaximumPixelMovement(). Therefore, we have to check if
      // (rpdq->rect + MaximumPixelMovement()) intersects the damage under it.
      // Then we extend the damage rect to include the (rpdq->rect +
      // MaximumPixelMovement()).

      // Expand the damage to cover entire |output_rect| if the |render_pass|
      // has pixel-moving foreground filter.
      if (child_render_pass.filters.HasFilterThatMovesPixels()) {
        gfx::Rect expanded_rect_in_target_space =
            GetExpandedRectWithPixelMovingForegroundFilter(render_pass_quad,
                                                           child_render_pass);

        if (expanded_rect_in_target_space.Intersects(damage_rect) ||
            expanded_rect_in_target_space.Intersects(damage_from_parent) ||
            expanded_rect_in_target_space.Intersects(surface_root_rp_damage)) {
          damage_rect.Union(expanded_rect_in_target_space);
        }
      }

      auto remapped_child_pass_id =
          pass_id_remapper_.Remap(child_pass_id, surface->surface_id());

      render_pass_dependencies_[remapped_pass_id].insert(
          remapped_child_pass_id);

      const gfx::Transform child_to_root_transform(
          target_to_root_transform,
          quad->shared_quad_state->quad_to_target_transform);
      quad_damage_rect = PrewalkRenderPass(
          &child_render_pass_entry, surface, render_pass_map, will_draw,
          gfx::Rect(), child_to_root_transform, in_moved_pixel_rp, result);

    } else {
      continue;
    }
    // Convert the quad damage rect into its target space and clip it if
    // needed. Ignore tiny errors to avoid artificially inflating the
    // damage due to floating point math.
    constexpr float kEpsilon = 0.001f;
    gfx::Rect rect_in_target_space =
        cc::MathUtil::MapEnclosingClippedRectIgnoringError(
            quad->shared_quad_state->quad_to_target_transform, quad_damage_rect,
            kEpsilon);
    if (quad->shared_quad_state->is_clipped) {
      rect_in_target_space.Intersect(quad->shared_quad_state->clip_rect);
    }
    damage_rect.Union(rect_in_target_space);
  }

  // Expand the damage to cover entire |output_rect| if the |render_pass| has
  // pixel-moving foreground filter.
  if (!damage_rect.IsEmpty() && render_pass.filters.HasFilterThatMovesPixels())
    damage_rect.Union(render_pass.output_rect);
  return damage_rect;
}

bool SurfaceAggregator::DeclareResourcesToProvider(
    Surface* surface,
    const std::vector<TransferableResource>& resource_list,
    const CompositorRenderPassList& render_passes) {
  // |provider_| may be null in tests.
  if (!provider_)
    return true;

  int child_id = ChildIdForSurface(surface);

  // Ref the resources in the surface, and let the provider know we've received
  // new resources from the compositor frame.
  if (surface->client())
    surface->client()->RefResources(resource_list);
  provider_->ReceiveFromChild(child_id, resource_list);

  // Figure out which resources are actually used in the render pass.
  // Note that we first gather them in a vector, since ResourceIdSet (which we
  // actually need) is a flat_set, which means bulk insertion we do at the end
  // is more efficient.
  std::vector<ResourceId> referenced_resources;
  referenced_resources.reserve(resource_list.size());

  const auto& child_to_parent_map = provider_->GetChildToParentMap(child_id);
  for (const auto& render_pass : render_passes) {
    for (auto* quad : render_pass->quad_list) {
      for (ResourceId resource_id : quad->resources) {
        // If we're using a resource which was not declared in the
        // |resource_list| then this is an invalid frame, we can abort.
        if (!child_to_parent_map.count(resource_id))
          return false;
        referenced_resources.push_back(resource_id);
      }
    }
  }

  // Declare the used resources to the provider. This will cause all resources
  // that were received but not used in the render passes to be unreferenced in
  // the surface, and returned to the child in the resource provider.
  ResourceIdSet resource_set(std::move(referenced_resources));
  provider_->DeclareUsedResourcesFromChild(child_id, resource_set);
  return true;
}

gfx::Rect SurfaceAggregator::PrewalkSurface(
    Surface* surface,
    bool in_moved_pixel_rp,
    AggregatedRenderPassId parent_pass_id,
    bool will_draw,
    const gfx::Rect& damage_from_parent,
    PrewalkResult* result) {
  if (referenced_surfaces_.count(surface->surface_id()))
    return gfx::Rect();

  contained_surfaces_[surface->surface_id()] = surface->GetActiveFrameIndex();
  LocalSurfaceId& local_surface_id =
      contained_frame_sinks_[surface->surface_id().frame_sink_id()];
  result->frame_sinks_changed |= (!previous_contained_frame_sinks_.contains(
      surface->surface_id().frame_sink_id()));
  local_surface_id =
      std::max(surface->surface_id().local_surface_id(), local_surface_id);

  if (!surface->HasActiveFrame())
    return gfx::Rect();

  const CompositorFrame& frame = surface->GetActiveFrame();
  auto remapped_pass_id = pass_id_remapper_.Remap(
      frame.render_pass_list.back()->id, surface->surface_id());
  if (parent_pass_id)
    render_pass_dependencies_[parent_pass_id].insert(remapped_pass_id);

  const gfx::Transform& root_pass_transform =
      IsRootSurface(surface) ? root_surface_transform_ : gfx::Transform();

  base::flat_map<CompositorRenderPassId, RenderPassMapEntry> render_pass_map =
      GenerateRenderPassMap(frame.render_pass_list, IsRootSurface(surface));

  bool valid_frame = DeclareResourcesToProvider(surface, frame.resource_list,
                                                frame.render_pass_list);
  if (!valid_frame)
    return gfx::Rect();
  valid_surfaces_.insert(surface->surface_id());

  CompositorRenderPass* last_pass = frame.render_pass_list.back().get();
  gfx::Rect full_damage = last_pass->output_rect;
  gfx::Rect damage_rect =
      DamageRectForSurface(surface, *last_pass, full_damage);

  // Avoid infinite recursion by adding current surface to
  // |referenced_surfaces_|.
  referenced_surfaces_.insert(surface->surface_id());

  auto it = render_pass_map.find(frame.render_pass_list.back()->id);
  DCHECK(it != render_pass_map.end());
  RenderPassMapEntry& entry = it->second;

  damage_rect.Union(PrewalkRenderPass(
      &entry, surface, &render_pass_map, will_draw, damage_from_parent,
      gfx::Transform(), in_moved_pixel_rp, result));
  damage_rect = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
      root_pass_transform, damage_rect);

  if (!damage_rect.IsEmpty()) {
    // The following call can cause one or more copy requests to be added to the
    // Surface. Therefore, no code before this point should have assumed
    // anything about the presence or absence of copy requests after this point.

    // The damage reported to the surface is in pre-display transform space
    // since it is used by clients which are not aware of the display transform.
    gfx::Transform inverse(gfx::Transform::kSkipInitialization);
    bool inverted = root_pass_transform.GetInverse(&inverse);
    DCHECK(inverted);
    surface->NotifyAggregatedDamage(
        cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(inverse,
                                                                damage_rect),
        expected_display_time_);
  }

  // If any CopyOutputRequests were made at FrameSink level, make sure we grab
  // them too.
  surface->TakeCopyOutputRequestsFromClient();

  if (de_jelly_enabled_ && surface->HasUndrawnActiveFrame())
    new_surfaces_.insert(surface->surface_id());

  if (will_draw)
    surface->OnWillBeDrawn();

  for (const SurfaceRange& surface_range : frame.metadata.referenced_surfaces) {
    damage_ranges_[surface_range.end().frame_sink_id()].push_back(
        surface_range);
    if (surface_range.HasDifferentFrameSinkIds()) {
      damage_ranges_[surface_range.start()->frame_sink_id()].push_back(
          surface_range);
    }
  }

  for (const SurfaceId& surface_id : surface->active_referenced_surfaces()) {
    if (!contained_surfaces_.count(surface_id)) {
      result->undrawn_surfaces.insert(surface_id);
      Surface* undrawn_surface = manager_->GetSurfaceForId(surface_id);
      if (undrawn_surface)
        PrewalkSurface(undrawn_surface, false, AggregatedRenderPassId(),
                       /*will_draw=*/false, gfx::Rect(), result);
    }
  }

  for (const auto& render_pass : frame.render_pass_list) {
    if (!render_pass->copy_requests.empty()) {
      auto remapped_pass_id =
          pass_id_remapper_.Remap(render_pass->id, surface->surface_id());
      copy_request_passes_.insert(remapped_pass_id);
    }
    if (render_pass->cache_render_pass)
      has_cached_render_passes_ = true;
  }

  referenced_surfaces_.erase(surface->surface_id());
  if (!damage_rect.IsEmpty() && frame.metadata.may_contain_video)
    result->may_contain_video = true;
  result->content_color_usage =
      std::max(result->content_color_usage, frame.metadata.content_color_usage);

  return damage_rect;
}

void SurfaceAggregator::CopyUndrawnSurfaces(PrewalkResult* prewalk_result) {
  // undrawn_surfaces are Surfaces that were identified by prewalk as being
  // referenced by a drawn Surface, but aren't contained in a SurfaceDrawQuad.
  // They need to be iterated over to ensure that any copy requests on them
  // (or on Surfaces they reference) are executed.
  std::vector<SurfaceId> surfaces_to_copy(
      prewalk_result->undrawn_surfaces.begin(),
      prewalk_result->undrawn_surfaces.end());
  DCHECK(referenced_surfaces_.empty());

  for (size_t i = 0; i < surfaces_to_copy.size(); i++) {
    SurfaceId surface_id = surfaces_to_copy[i];
    Surface* surface = manager_->GetSurfaceForId(surface_id);
    if (!surface)
      continue;
    if (!surface->HasActiveFrame())
      continue;
    if (!surface->HasCopyOutputRequests()) {
      // Children are not necessarily included in undrawn_surfaces (because
      // they weren't referenced directly from a drawn surface), but may have
      // copy requests, so make sure to check them as well.
      for (const SurfaceId& child_id : surface->active_referenced_surfaces()) {
        // Don't iterate over the child Surface if it was already listed as a
        // child of a different Surface, or in the case where there's infinite
        // recursion.
        if (!prewalk_result->undrawn_surfaces.count(child_id)) {
          surfaces_to_copy.push_back(child_id);
          prewalk_result->undrawn_surfaces.insert(child_id);
        }
      }
    } else {
      prewalk_result->undrawn_surfaces.erase(surface_id);
      referenced_surfaces_.insert(surface_id);
      CopyPasses(surface->GetActiveFrame(), surface);
      referenced_surfaces_.erase(surface_id);
    }
  }
}

void SurfaceAggregator::PropagateCopyRequestPasses() {
  std::vector<AggregatedRenderPassId> copy_requests_to_iterate(
      copy_request_passes_.begin(), copy_request_passes_.end());
  while (!copy_requests_to_iterate.empty()) {
    auto id = copy_requests_to_iterate.back();
    copy_requests_to_iterate.pop_back();
    auto it = render_pass_dependencies_.find(id);
    if (it == render_pass_dependencies_.end())
      continue;
    for (auto pass : it->second) {
      if (copy_request_passes_.insert(pass).second) {
        copy_requests_to_iterate.push_back(pass);
      }
    }
  }
}

bool SurfaceAggregator::CanMergeMaskFilterInfo(
    const MaskFilterInfoExt& mask_filter_info_ext,
    const CompositorRenderPass& root_render_pass) {
  // If the quad has no mask filter, then we do not have to block merging.
  if (mask_filter_info_ext.mask_filter_info.IsEmpty())
    return true;

  // If the quad has rounded corner and it is not a fast rounded corner, we
  // cannot merge.
  if (mask_filter_info_ext.mask_filter_info.HasRoundedCorners() &&
      !mask_filter_info_ext.is_fast_rounded_corner)
    return false;

  // If any of the quads in the root render pass has a mask filter of its
  // own, then we cannot merge.
  const SharedQuadStateList& sqs_list = root_render_pass.shared_quad_state_list;
  for (const auto* sqs : sqs_list) {
    if (!sqs->mask_filter_info.IsEmpty())
      return false;
  }
  return true;
}

AggregatedFrame SurfaceAggregator::Aggregate(
    const SurfaceId& surface_id,
    base::TimeTicks expected_display_time,
    gfx::OverlayTransform display_transform,
    const gfx::Rect& target_damage,
    int64_t display_trace_id) {
  DCHECK(!expected_display_time.is_null());

  root_surface_id_ = surface_id;
  Surface* surface = manager_->GetSurfaceForId(surface_id);
  DCHECK(surface);
  DCHECK(contained_surfaces_.empty());
  contained_surfaces_[surface_id] = surface->GetActiveFrameIndex();

  LocalSurfaceId& local_surface_id =
      contained_frame_sinks_[surface_id.frame_sink_id()];
  local_surface_id =
      std::max(surface->surface_id().local_surface_id(), local_surface_id);

  if (!surface->HasActiveFrame())
    return {};

  display_trace_id_ = display_trace_id;
  expected_display_time_ = expected_display_time;

  const CompositorFrame& root_surface_frame = surface->GetActiveFrame();
  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Graphics.Pipeline",
      TRACE_ID_GLOBAL(root_surface_frame.metadata.begin_frame_ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SurfaceAggregation", "display_trace", display_trace_id_);

  AggregatedFrame frame;
  frame.top_controls_visible_height =
      root_surface_frame.metadata.top_controls_visible_height;

  dest_pass_list_ = &frame.render_pass_list;
  surface_damage_rect_list_ = &frame.surface_damage_rect_list_;

  const gfx::Size viewport_bounds =
      root_surface_frame.render_pass_list.back()->output_rect.size();
  root_surface_transform_ = gfx::OverlayTransformToTransform(
      display_transform, gfx::SizeF(viewport_bounds));

  // Reset state that couldn't be reset in ResetAfterAggregate().
  damage_ranges_.clear();

  DCHECK(referenced_surfaces_.empty());

  PrewalkResult prewalk_result;
  gfx::Rect surfaces_damage_rect = PrewalkSurface(
      surface, /*in_moved_pixel_rp=*/false,
      /*parent_pass=*/AggregatedRenderPassId(),
      /*will_draw=*/true, /*damage_from_parent=*/gfx::Rect(), &prewalk_result);
  root_damage_rect_ = surfaces_damage_rect;
  // |root_damage_rect_| is used to restrict aggregating quads only if they
  // intersect this area.
  root_damage_rect_.Union(target_damage);

  // Changing color usage will cause the renderer to reshape the output surface,
  // therefore the renderer might expand the damage to the whole frame. The
  // following makes sure SA will produce all the quads to cover the full frame.
  bool color_usage_changed =
      root_content_color_usage_ != prewalk_result.content_color_usage;
  if (color_usage_changed) {
    root_damage_rect_ = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
        root_surface_transform_,
        gfx::Rect(root_surface_frame.size_in_pixels()));
  }

  root_content_color_usage_ = prewalk_result.content_color_usage;

  if (prewalk_result.frame_sinks_changed)
    manager_->AggregatedFrameSinksChanged();

  PropagateCopyRequestPasses();
  has_copy_requests_ = !copy_request_passes_.empty();
  frame.has_copy_requests = has_copy_requests_;
  frame.may_contain_video = prewalk_result.may_contain_video;
  frame.content_color_usage = prewalk_result.content_color_usage;

  CopyUndrawnSurfaces(&prewalk_result);
  referenced_surfaces_.insert(surface_id);
  CopyPasses(root_surface_frame, surface);
  referenced_surfaces_.erase(surface_id);
  DCHECK(referenced_surfaces_.empty());

  if (dest_pass_list_->empty()) {
    ResetAfterAggregate();
    return {};
  }

  // The root render pass damage might have been expanded by target_damage (the
  // area that might need to be recomposited on the target surface). We restrict
  // the damage_rect of the root render pass to the one caused by the source
  // surfaces, except when drawing delegated ink trails.
  // The damage on the root render pass should not include the expanded area
  // since Renderer and OverlayProcessor expect the non expanded damage. The
  // only exception is when delegated ink trails are being drawn, in which case
  // the root render pass needs to contain the expanded area, as |target_damage|
  // also reflects the delegated ink trail damage rect.
  auto* last_pass = dest_pass_list_->back().get();

  if (!color_usage_changed && !last_frame_had_delegated_ink_ &&
      !RenderPassNeedsFullDamage(last_pass->id, last_pass->cache_render_pass))
    dest_pass_list_->back()->damage_rect.Intersect(surfaces_damage_rect);

  // Now that we've handled our main surface aggregation, apply de-jelly effect
  // if enabled.
  if (de_jelly_enabled_)
    HandleDeJelly(surface);

  AddColorConversionPass();

  ProcessAddedAndRemovedSurfaces();
  contained_surfaces_.swap(previous_contained_surfaces_);
  contained_frame_sinks_.swap(previous_contained_frame_sinks_);

  ResetAfterAggregate();

  for (auto it : previous_contained_surfaces_) {
    Surface* surface = manager_->GetSurfaceForId(it.first);
    if (surface) {
      surface->allocation_group()->TakeAggregatedLatencyInfoUpTo(
          surface, &frame.latency_info);
    }
    if (!ui::LatencyInfo::Verify(frame.latency_info,
                                 "SurfaceAggregator::Aggregate")) {
      break;
    }
  }

  if (delegated_ink_metadata_) {
    frame.delegated_ink_metadata = std::move(delegated_ink_metadata_);
    last_frame_had_delegated_ink_ = true;
  } else {
    last_frame_had_delegated_ink_ = false;
  }

  if (frame_annotator_)
    frame_annotator_->AnnotateAggregatedFrame(&frame);

  return frame;
}

void SurfaceAggregator::ResetAfterAggregate() {
  dest_pass_list_ = nullptr;
  surface_damage_rect_list_ = nullptr;
  current_zero_damage_rect_is_not_recorded_ = false;
  expected_display_time_ = base::TimeTicks();
  valid_surfaces_.clear();
  has_cached_render_passes_ = false;
  has_pixel_moving_backdrop_filter_ = false;
  new_surfaces_.clear();
  moved_pixel_passes_.clear();
  copy_request_passes_.clear();
  contributing_content_damaged_passes_.clear();
  render_pass_dependencies_.clear();
  pass_id_remapper_.ClearUnusedMappings();
  contained_surfaces_.clear();
  contained_frame_sinks_.clear();
  display_trace_id_ = -1;
}

void SurfaceAggregator::ReleaseResources(const SurfaceId& surface_id) {
  auto it = surface_id_to_resource_child_id_.find(surface_id);
  if (it != surface_id_to_resource_child_id_.end()) {
    provider_->DestroyChild(it->second);
    surface_id_to_resource_child_id_.erase(it);
  }
}

void SurfaceAggregator::SetFullDamageForSurface(const SurfaceId& surface_id) {
  auto it = previous_contained_surfaces_.find(surface_id);
  if (it == previous_contained_surfaces_.end())
    return;
  // Set the last drawn index as 0 to ensure full damage next time it's drawn.
  it->second = 0;
}

void SurfaceAggregator::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  display_color_spaces_ = display_color_spaces;
}

void SurfaceAggregator::SetMaxRenderTargetSize(int max_size) {
  DCHECK_GE(max_size, 0);
  max_render_target_size_ = max_size;
}

bool SurfaceAggregator::NotifySurfaceDamageAndCheckForDisplayDamage(
    const SurfaceId& surface_id) {
  if (previous_contained_surfaces_.count(surface_id)) {
    Surface* surface = manager_->GetSurfaceForId(surface_id);
    if (surface) {
      DCHECK(surface->HasActiveFrame());
      if (surface->GetActiveFrame().resource_list.empty())
        ReleaseResources(surface_id);
    }
    return true;
  }

  auto it = damage_ranges_.find(surface_id.frame_sink_id());
  if (it == damage_ranges_.end())
    return false;

  for (const SurfaceRange& surface_range : it->second) {
    if (surface_range.IsInRangeInclusive(surface_id))
      return true;
  }

  return false;
}

bool SurfaceAggregator::HasFrameAnnotator() const {
  return !!frame_annotator_;
}

void SurfaceAggregator::SetFrameAnnotator(
    std::unique_ptr<FrameAnnotator> frame_annotator) {
  DCHECK(!frame_annotator_);
  frame_annotator_ = std::move(frame_annotator);
}

void SurfaceAggregator::DestroyFrameAnnotator() {
  DCHECK(frame_annotator_);
  frame_annotator_.reset();
}

bool SurfaceAggregator::IsRootSurface(const Surface* surface) const {
  return surface->surface_id() == root_surface_id_;
}

// Transform the point and presentation area of the metadata to be in the root
// target space. They need to be in the root target space because they will
// eventually be drawn directly onto the buffer just before being swapped onto
// the screen, so root target space is required so that they are positioned
// correctly. After transforming, they are stored in the
// |delegated_ink_metadata_| member in order to be placed on the final
// aggregated frame, after which the member is then cleared.
void SurfaceAggregator::TransformAndStoreDelegatedInkMetadata(
    const gfx::Transform& parent_quad_to_root_target_transform,
    std::unique_ptr<DelegatedInkMetadata> metadata) {
  if (delegated_ink_metadata_) {
    // This member could already be populated in two scenarios:
    //   1. The delegated ink metadata was committed to a frame's metadata that
    //      wasn't ultimately used to produce a frame, but is now being used.
    //   2. There are two or more ink strokes requesting a delegated ink trail
    //      simultaneously.
    // In both cases, we want to default to using a "last write wins" strategy
    // to determine the metadata to put on the final aggregated frame. This
    // avoids potential issues of using stale ink metadata in the first scenario
    // by always using the newest one. For the second scenario, it would be a
    // very niche use case to have more than one at a time, so the explainer
    // specifies using last write wins to decide.
    base::TimeTicks stored_time = delegated_ink_metadata_->timestamp();
    base::TimeTicks new_time = metadata->timestamp();
    if (new_time < stored_time)
      return;
  }

  gfx::PointF point(metadata->point());
  gfx::RectF area(metadata->presentation_area());
  parent_quad_to_root_target_transform.TransformPoint(&point);
  parent_quad_to_root_target_transform.TransformRect(&area);
  delegated_ink_metadata_ = std::make_unique<DelegatedInkMetadata>(
      point, metadata->diameter(), metadata->color(), metadata->timestamp(),
      area, metadata->frame_time(), metadata->is_hovering());

  TRACE_EVENT_INSTANT2(
      "viz", "SurfaceAggregator::TransformAndStoreDelegatedInkMetadata",
      TRACE_EVENT_SCOPE_THREAD, "original metadata", metadata->ToString(),
      "transformed metadata", delegated_ink_metadata_->ToString());
}

void SurfaceAggregator::HandleDeJelly(Surface* surface) {
  TRACE_EVENT0("viz", "SurfaceAggregator::HandleDeJelly");

  if (!DeJellyActive()) {
    SetLastFrameHadJelly(false);
    return;
  }

  // |jelly_clip| is the rect that contains all de-jelly'd quads. It is used as
  // an approximation for the containing non-skewed clip rect.
  gfx::Rect jelly_clip;
  // |max_skew| represents the maximum skew applied to an element. To prevent
  // tearing due to slight inaccuracies, we apply the max skew to all skewed
  // elements.
  float max_skew = 0.0f;

  // Iterate over each SharedQuadState in the root render pass and compute
  // |max_skew| and |jelly_clip|.
  auto* root_render_pass = dest_pass_list_->back().get();
  float screen_width = DeJellyScreenWidth();
  for (SharedQuadState* state : root_render_pass->shared_quad_state_list) {
    float delta_y = state->de_jelly_delta_y;
    if (delta_y == 0.0f)
      continue;

    // We are going to de-jelly this SharedQuadState. Expand the max clip.
    jelly_clip.Union(state->clip_rect);

    // Compute the skew angle and update |max_skew|.
    float de_jelly_angle = gfx::RadToDeg(atan2(delta_y, screen_width));
    float sign = de_jelly_angle / std::abs(de_jelly_angle);
    max_skew = std::max(std::abs(de_jelly_angle), std::abs(max_skew)) * sign;
  }

  // Exit if nothing was skewed.
  if (max_skew == 0.0f) {
    SetLastFrameHadJelly(false);
    return;
  }

  SetLastFrameHadJelly(true);

  // Remove the existing root render pass and create a new one which we will
  // re-copy skewed quads / render-passes to.
  // TODO(ericrk): Handle backdrop filters?
  // TODO(ericrk): This will end up skewing copy requests. Address if
  // necessary.
  auto old_root = std::move(dest_pass_list_->back());
  dest_pass_list_->pop_back();
  auto new_root = root_render_pass->Copy(root_render_pass->id);
  new_root->copy_requests = std::move(old_root->copy_requests);

  // Data tracking the current sub RenderPass (if any) which is being appended
  // to. We can keep re-using a sub RenderPass if the skew has not changed and
  // if we are in the typical kSrcOver blend mode.
  std::unique_ptr<AggregatedRenderPass> sub_render_pass;
  SkBlendMode sub_render_pass_blend_mode;
  float sub_render_pass_opacity;

  // Apply de-jelly to all quads, promoting quads into render passes as
  // necessary.
  for (auto it = root_render_pass->quad_list.begin();
       it != root_render_pass->quad_list.end();) {
    auto* state = it->shared_quad_state;
    bool has_skew = state->de_jelly_delta_y != 0.0f;

    // If we have a sub RenderPass which is not compatible with our current
    // quad, we must flush and clear it.
    if (sub_render_pass) {
      if (!has_skew || sub_render_pass_blend_mode != state->blend_mode ||
          state->blend_mode != SkBlendMode::kSrcOver) {
        AppendDeJellyRenderPass(max_skew, jelly_clip, sub_render_pass_opacity,
                                sub_render_pass_blend_mode, new_root.get(),
                                std::move(sub_render_pass));
        sub_render_pass.reset();
      }
    }

    // Create a new render pass if we have a skewed quad which is clipped more
    // than jelly_clip.
    bool create_render_pass =
        has_skew && state->is_clipped && state->clip_rect != jelly_clip;
    if (!sub_render_pass && create_render_pass) {
      sub_render_pass = std::make_unique<AggregatedRenderPass>(1, 1);
      gfx::Transform skew_transform;
      skew_transform.Skew(0.0f, max_skew);
      // Ignore rectangles for now, these are updated in
      // CreateDeJellyRenderPassQuads.
      sub_render_pass->SetNew(pass_id_remapper_.NextAvailableId(), gfx::Rect(),
                              gfx::Rect(), skew_transform);
      // If blend mode is not kSrcOver, we apply it in the render pass.
      if (state->blend_mode != SkBlendMode::kSrcOver) {
        sub_render_pass_opacity = state->opacity;
        sub_render_pass_blend_mode = state->blend_mode;
      } else {
        sub_render_pass_opacity = 1.0f;
        sub_render_pass_blend_mode = SkBlendMode::kSrcOver;
      }
    }

    if (sub_render_pass) {
      CreateDeJellyRenderPassQuads(&it, root_render_pass->quad_list.end(),
                                   jelly_clip, max_skew, sub_render_pass.get());
    } else {
      float skew = has_skew ? max_skew : 0.0f;
      CreateDeJellyNormalQuads(&it, root_render_pass->quad_list.end(),
                               new_root.get(), skew);
    }
  }
  if (sub_render_pass) {
    AppendDeJellyRenderPass(max_skew, jelly_clip, sub_render_pass_opacity,
                            sub_render_pass_blend_mode, new_root.get(),
                            std::move(sub_render_pass));
  }

  dest_pass_list_->push_back(std::move(new_root));
}

void SurfaceAggregator::CreateDeJellyRenderPassQuads(
    cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
    const cc::ListContainer<DrawQuad>::Iterator& end,
    const gfx::Rect& jelly_clip,
    float skew,
    AggregatedRenderPass* render_pass) {
  auto* quad = **quad_iterator;
  const auto* state = quad->shared_quad_state;

  // Heuristic - we may have over-clipped a quad. If a quad is clipped by the
  // |jelly_clip|, but contains content beyond |jelly_clip|, un-clip the quad by
  // MaxDeJellyHeight().
  int un_clip_top = 0;
  int un_clip_bottom = 0;
  if (state->clip_rect.y() <= jelly_clip.y()) {
    un_clip_top = MaxDeJellyHeight();
  }
  if (state->clip_rect.bottom() >= jelly_clip.bottom()) {
    un_clip_bottom = MaxDeJellyHeight();
  }

  // Compute the required renderpass rect in target space.
  // First, find the un-transformed visible rect.
  gfx::RectF render_pass_visible_rect_f(state->visible_quad_layer_rect);
  // Next, if this is a RenderPass quad, find any filters and expand the
  // visible rect.
  if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
    auto target_id = AggregatedRenderPassId(uint64_t{
        CompositorRenderPassDrawQuad::MaterialCast(quad)->render_pass_id});
    for (auto& rp : *dest_pass_list_) {
      if (rp->id == target_id) {
        render_pass_visible_rect_f = gfx::RectF(
            rp->filters.MapRect(state->visible_quad_layer_rect, SkMatrix()));
        break;
      }
    }
  }
  // Next, find the enclosing Rect for the transformed target space RectF.
  state->quad_to_target_transform.TransformRect(&render_pass_visible_rect_f);
  gfx::Rect render_pass_visible_rect =
      gfx::ToEnclosingRect(render_pass_visible_rect_f);
  // Finally, expand by our un_clip amounts.
  render_pass_visible_rect.Inset(0, -un_clip_top, 0, -un_clip_bottom);

  // Expand the |render_pass|'s rects.
  render_pass->output_rect =
      gfx::UnionRects(render_pass->output_rect, render_pass_visible_rect);
  render_pass->damage_rect = render_pass->output_rect;

  // Create a new SharedQuadState based on |state|.
  {
    auto* new_state = render_pass->CreateAndAppendSharedQuadState();
    *new_state = *state;
    // If blend mode is not kSrcOver, we apply it in the RenderPass.
    if (state->blend_mode != SkBlendMode::kSrcOver) {
      new_state->opacity = 1.0f;
      new_state->blend_mode = SkBlendMode::kSrcOver;
    }

    // Expand our clip by un clip amounts.
    new_state->clip_rect.Inset(0, -un_clip_top, 0, -un_clip_bottom);
  }

  // Append all quads sharing |new_state|.
  AppendDeJellyQuadsForSharedQuadState(quad_iterator, end, render_pass, state);
}

void SurfaceAggregator::CreateDeJellyNormalQuads(
    cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
    const cc::ListContainer<DrawQuad>::Iterator& end,
    AggregatedRenderPass* root_pass,
    float skew) {
  auto* quad = **quad_iterator;
  const auto* state = quad->shared_quad_state;

  // Crearte a new SharedQuadState on |root_pass| and apply skew if any.
  SharedQuadState* new_state = root_pass->CreateAndAppendSharedQuadState();
  *new_state = *state;
  if (skew != 0.0f) {
    gfx::Transform skew_transform;
    skew_transform.Skew(0.0f, skew);
    new_state->quad_to_target_transform =
        skew_transform * new_state->quad_to_target_transform;
  }

  // Append all quads sharing |new_state|.
  AppendDeJellyQuadsForSharedQuadState(quad_iterator, end, root_pass, state);
}

void SurfaceAggregator::AppendDeJellyRenderPass(
    float skew,
    const gfx::Rect& jelly_clip,
    float opacity,
    SkBlendMode blend_mode,
    AggregatedRenderPass* root_pass,
    std::unique_ptr<AggregatedRenderPass> render_pass) {
  // Create a new quad for this renderpass and append it to the pass list.
  auto* new_state = root_pass->CreateAndAppendSharedQuadState();
  gfx::Transform transform;
  new_state->SetAll(transform, render_pass->output_rect,
                    render_pass->output_rect, gfx::MaskFilterInfo(), jelly_clip,
                    true, false, opacity, blend_mode, 0);
  auto* quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(new_state, render_pass->output_rect, render_pass->output_rect,
               render_pass->id, kInvalidResourceId, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(), gfx::PointF(),
               gfx::RectF(gfx::SizeF(render_pass->output_rect.size())), false,
               1.0f);
  gfx::Transform skew_transform;
  skew_transform.Skew(0.0f, skew);
  new_state->quad_to_target_transform =
      skew_transform * new_state->quad_to_target_transform;
  dest_pass_list_->push_back(std::move(render_pass));
}

void SurfaceAggregator::AppendDeJellyQuadsForSharedQuadState(
    cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
    const cc::ListContainer<DrawQuad>::Iterator& end,
    AggregatedRenderPass* render_pass,
    const SharedQuadState* state) {
  auto* quad = **quad_iterator;
  while (quad->shared_quad_state == state) {
    // Since we're dealing with post-aggregated passes, we should not have any
    // RenderPassDrawQuads.
    DCHECK_NE(quad->material, DrawQuad::Material::kCompositorRenderPass);
    if (quad->material == DrawQuad::Material::kAggregatedRenderPass) {
      const auto* pass_quad = AggregatedRenderPassDrawQuad::MaterialCast(quad);
      render_pass->CopyFromAndAppendRenderPassDrawQuad(pass_quad);
    } else {
      render_pass->CopyFromAndAppendDrawQuad(quad);
    }

    ++(*quad_iterator);
    if (*quad_iterator == end)
      break;
    quad = **quad_iterator;
  }
}

void SurfaceAggregator::SetLastFrameHadJelly(bool had_jelly) {
  // If we've just rendererd a jelly-free frame after one with jelly, we must
  // damage the entire surface, as we may have removed jelly from an otherwise
  // unchanged quad.
  if (last_frame_had_jelly_ && !had_jelly) {
    auto* root_pass = dest_pass_list_->back().get();
    root_pass->damage_rect = root_pass->output_rect;
  }
  last_frame_had_jelly_ = had_jelly;
}
}  // namespace viz
