// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/surface_aggregator.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "cc/base/math_util.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/display/resolved_frame_data.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_allocation_group.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/overlay_transform_utils.h"

namespace viz {

struct MaskFilterInfoExt {
  MaskFilterInfoExt() = default;
  MaskFilterInfoExt(const gfx::MaskFilterInfo& mask_filter_info_arg,
                    bool is_fast_rounded_corner_arg,
                    const gfx::Transform target_transform)
      : mask_filter_info(mask_filter_info_arg),
        is_fast_rounded_corner(is_fast_rounded_corner_arg) {
    mask_filter_info.ApplyTransform(target_transform);
  }

  // Returns true if the quads from |merge_render_pass| can be merged into
  // the embedding render pass based on mask filter info.
  // |parent_target_transform| shall be used to translate mask filter infos of
  // |merge_render_pass.shared_quad_state_list| in the same coordinate space
  // as the |mask_filter_info| is.
  bool CanMergeMaskFilterInfo(
      const CompositorRenderPass& merge_render_pass,
      const gfx::Transform& parent_target_transform) const {
    DCHECK(parent_target_transform.Preserves2dAxisAlignment());

    // If the embedding quad has no mask filter, then we do not have to block
    // merging.
    if (mask_filter_info.IsEmpty()) {
      return true;
    }

    // If the embedding quad has rounded corner and it is not a fast rounded
    // corner, we cannot merge.
    if (mask_filter_info.HasRoundedCorners() && !is_fast_rounded_corner) {
      return false;
    }

    // If any of the quads in the render pass to merged has a mask filter of its
    // own, then we have to check if that has fast rounded corners and they fit
    // |mask_filter_info|'s ones. In that case, we can merge this render pass.
    // Merge is impossible in all the other cases.
    for (const auto* sqs : merge_render_pass.shared_quad_state_list) {
      if (sqs->mask_filter_info.IsEmpty()) {
        continue;
      }

      // We cannot handle rotation with mask filter as rotated content is unable
      // to apply correct clip.
      if (!sqs->quad_to_target_transform.Preserves2dAxisAlignment()) {
        return false;
      }

      // Those must be fast rounded corners that enables us to squash mask
      // filters.
      if (sqs->mask_filter_info.HasRoundedCorners() &&
          !sqs->is_fast_rounded_corner) {
        return false;
      }

      if (sqs->mask_filter_info.HasGradientMask()) {
        return false;
      }

      // Take the bounds of the sqs filter and apply clipping rect as it may
      // make current mask fit the |mask_filter_info|'s bounds. Not doing so may
      // result in marking this mask not suitable for merging while it never
      // spans outside another mask.
      auto rounded_corner_bounds = sqs->mask_filter_info.bounds();
      if (sqs->clip_rect.has_value()) {
        rounded_corner_bounds.Intersect(gfx::RectF(*sqs->clip_rect));
      }

      // Before checking if current mask's rounded corners do not intersect with
      // the upper level rounded corner mask, its system coordinate must be
      // transformed to that target's system coordinate.
      rounded_corner_bounds =
          parent_target_transform.MapRect(rounded_corner_bounds);

      // This is the only case when quads of this render pass with the mask
      // filter info that has fast rounded corners set can be merged into the
      // embedding render pass. So, if they don't intersect with the "toplevel"
      // rounded corners, we can merge.
      if (!mask_filter_info.rounded_corner_bounds().Contains(
              rounded_corner_bounds)) {
        return false;
      }
    }
    return true;
  }

  gfx::MaskFilterInfo mask_filter_info;
  bool is_fast_rounded_corner;
};

namespace {

// Enum used for UMA histogram. These enum values must not be changed or
// reused.
enum class RenderPassDamage {
  // Clipping at the root does not make the damage smaller than the output rect.
  kOutputRect = 0,
  // Clipping at the root will clip the render pass and make it smaller than
  // output rect.
  kRootClipped = 1,
  //  Full output rect damage was forced for this render pass.
  kForceFullOutputRect = 2,
  kMaxValue = kForceFullOutputRect,
};

// Used for determine when to treat opacity close to 1.f as opaque. The value is
// chosen to be smaller than 1/255.
constexpr float kOpacityEpsilon = 0.001f;

// Used as a limit for the amount of times the same delegated ink metadata can
// be attached to the aggregated frame.
constexpr int kMaxFramesWithIdenticalInkMetadata = 3;

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

// Create a clip rect for an aggregated quad from the original clip rect and
// the clip rect from the surface it's on.
std::optional<gfx::Rect> CalculateClipRect(
    const std::optional<gfx::Rect> surface_clip,
    const std::optional<gfx::Rect> quad_clip,
    const gfx::Transform& target_transform) {
  std::optional<gfx::Rect> out_clip;
  if (surface_clip)
    out_clip = surface_clip;

  if (quad_clip) {
    // TODO(jamesr): This only works if target_transform maps integer
    // rects to integer rects.
    gfx::Rect final_clip =
        cc::MathUtil::MapEnclosingClippedRect(target_transform, *quad_clip);
    if (out_clip)
      out_clip->Intersect(final_clip);
    else
      out_clip = final_clip;
  }

  return out_clip;
}

// Creates a new SharedQuadState in |dest_render_pass| based on |source_sqs|
// plus additional modified values.
// - |source_sqs| is the SharedQuadState to copy from.
// - |quad_to_target_transform| replaces the equivalent |source_sqs| value.
// - |target_transform| is an additional transform to add. Used when merging the
//    root render pass of a surface into the embedding render pass.
// - |quad_layer_rect| replaces the equivalent |source_sqs| value.
// - |visible_quad_layer_rect| replaces the equivalent |source_sqs| value.
// - |mask_filter_info_ext| replaces the equivalent |source_sqs| values.
// - |added_clip_rect| is an additional clip rect added to the quad clip rect.
// - |dest_render_pass| is where the new SharedQuadState will be created.
SharedQuadState* CopyAndScaleSharedQuadState(
    const SharedQuadState* source_sqs,
    uint32_t client_namespace_id,
    const gfx::Transform& quad_to_target_transform,
    const gfx::Transform& target_transform,
    const gfx::Rect& quad_layer_rect,
    const gfx::Rect& visible_quad_layer_rect,
    const std::optional<gfx::Rect> added_clip_rect,
    const MaskFilterInfoExt& mask_filter_info_ext,
    AggregatedRenderPass* dest_render_pass) {
  auto* shared_quad_state = dest_render_pass->CreateAndAppendSharedQuadState();
  auto new_clip_rect = CalculateClipRect(added_clip_rect, source_sqs->clip_rect,
                                         target_transform);

  // target_transform contains any transformation that may exist
  // between the context that these quads are being copied from (i.e. the
  // surface's draw transform when aggregated from within a surface) to the
  // target space of the pass. This will be identity except when copying the
  // root draw pass from a surface into a pass when the surface draw quad's
  // transform is not identity.
  gfx::Transform new_transform = quad_to_target_transform;
  new_transform.PostConcat(target_transform);

  shared_quad_state->SetAll(
      new_transform, quad_layer_rect, visible_quad_layer_rect,
      mask_filter_info_ext.mask_filter_info, new_clip_rect,
      source_sqs->are_contents_opaque, source_sqs->opacity,
      source_sqs->blend_mode, source_sqs->sorting_context_id,
      source_sqs->layer_id, mask_filter_info_ext.is_fast_rounded_corner);
  shared_quad_state->layer_namespace_id = client_namespace_id;
  return shared_quad_state;
}

// Creates a new SharedQuadState in |dest_render_pass| and copies |source_sqs|
// into it. See CopyAndScaleSharedQuadState() for full documentation.
SharedQuadState* CopySharedQuadState(
    const SharedQuadState* source_sqs,
    uint32_t client_namespace_id,
    const gfx::Transform& target_transform,
    const std::optional<gfx::Rect> added_clip_rect,
    const MaskFilterInfoExt& mask_filter_info,
    AggregatedRenderPass* dest_render_pass) {
  return CopyAndScaleSharedQuadState(
      source_sqs, client_namespace_id, source_sqs->quad_to_target_transform,
      target_transform, source_sqs->quad_layer_rect,
      source_sqs->visible_quad_layer_rect, added_clip_rect, mask_filter_info,
      dest_render_pass);
}

void UpdatePersistentPassDataMergeState(ResolvedPassData& resolved_pass,
                                        AggregatedRenderPass* dest_pass,
                                        bool is_merged_pass) {
  auto& persistent_data = resolved_pass.current_persistent_data();

  PersistentPassData::MergeState merge_state =
      is_merged_pass ? PersistentPassData::kAlwaysMerged
                     : PersistentPassData::kNotMerged;

  if (persistent_data.merge_state == PersistentPassData::kInitState) {
    // This is the first time it's embedded.
    persistent_data.merge_state = merge_state;
  } else if (persistent_data.merge_state != merge_state) {
    persistent_data.merge_state = PersistentPassData::kSomeTimesMerged;
  }
}

bool ChangeInMergeState(ResolvedPassData& resolved_pass) {
  DCHECK(resolved_pass.current_persistent_data().merge_state !=
         PersistentPassData::kInitState);
  // If this is the first frame and previous_merge_state is empty,
  // this function will returns false.
  auto current_merge_state =
      resolved_pass.current_persistent_data().merge_state;
  auto previous_merge_state =
      resolved_pass.previous_persistent_data().merge_state;

  // Check if this render pass is merged to its parent render pass in the
  // previous frame but is not in the current frame.
  bool change_in_merged_pass =
      previous_merge_state == PersistentPassData::kAlwaysMerged &&
      current_merge_state == PersistentPassData::kNotMerged;

  // If it's embedded multiple times and some are merged while some are not,
  // just redraw the render pass. It's complicated to track individual change.
  change_in_merged_pass |=
      resolved_pass.current_persistent_data().merge_state ==
          PersistentPassData::kSomeTimesMerged ||
      resolved_pass.previous_persistent_data().merge_state ==
          PersistentPassData::kSomeTimesMerged;

  return change_in_merged_pass;
}

void UpdateNeedsRedraw(
    ResolvedPassData& resolved_pass,
    AggregatedRenderPass* dest_pass,
    const std::optional<gfx::Rect> dest_root_target_clip_rect) {
  // |dest_root_target_clip_rect| is the bounding box on the root surface where
  // this render pass can be rendered into. It includes all ancestors' render
  // pass output rects, RenderPassDrawQuad rect, SurfaceDrawQuad rect, and clip
  // rects.
  DCHECK(dest_root_target_clip_rect.has_value());

  // Save the parent_clip_rect from the current frame.
  auto& current_parent_clip_rect =
      resolved_pass.current_persistent_data().parent_clip_rect;
  current_parent_clip_rect.Union(dest_root_target_clip_rect.value());

  // Get the parent_clip_rect from the previous frame;
  auto& previous_parent_clip_rect =
      resolved_pass.previous_persistent_data().parent_clip_rect;

  // If the parent clip rect expands, the new area of the render pass output
  // buffer has never been updated. Redraw is needed.
  bool parent_clip_rect_expands =
      !previous_parent_clip_rect.Contains(current_parent_clip_rect);

  // Whether the render pass is merged with its parent render pass and changes.
  bool change_in_merged_pass = ChangeInMergeState(resolved_pass);

  // 1. Needs redraw when the current parent clip rect expands from the
  // previous frame.
  // 2. Needs full damage and redraw when it switched from merged to
  // non-merged.
  // 3. Needs full damage and redraw when it is in_copy_request_pass.
  if (parent_clip_rect_expands ||
      resolved_pass.aggregation().in_copy_request_pass ||
      change_in_merged_pass) {
    dest_pass->has_damage_from_contributing_content = true;
  }
}

bool RenderPassNeedsFullDamage(ResolvedPassData& resolved_pass) {
  auto& aggregation = resolved_pass.aggregation();

  const bool can_skip_render_pass = base::FeatureList::IsEnabled(
      features::kAllowUndamagedNonrootRenderPassToSkip);
  if (can_skip_render_pass) {
    // Needs full damage when
    // 1. The render pass pixels will be saved, either by a copy request or into
    //    a cached render pass. This avoids a partially drawn render pass being
    //    saved.
    // 2. A render pass is merged to its parent render pass in the previous
    //    frame but it's not in this frame.
    return aggregation.in_cached_render_pass ||
           aggregation.in_copy_request_pass ||
           aggregation.in_pixel_moving_filter_pass ||
           ChangeInMergeState(resolved_pass);
  } else {
    // Returns true if |resolved_pass| needs full damage. This is because:
    // 1. The render pass pixels will be saved, either by a copy request or into
    //    a cached render pass. This avoids a partially drawn render pass being
    //    saved.
    // 2. The render pass pixels will have a pixel moving foreground filter
    //    applied to them. In this case pixels outside the damage_rect can be
    //    moved inside the damage_rect by the filter.

    return aggregation.in_cached_render_pass ||
           aggregation.in_copy_request_pass ||
           aggregation.in_pixel_moving_filter_pass;
  }
}

// Computes an enclosing rect in target render pass coordinate space that bounds
// where |quad| may contribute pixels. This rect is computed by transforming the
// quads |visible_rect|, which is known to be contained by the quads |rect|, and
// transforming it into target render pass coordinate space. The rect is then
// clipped by SharedQuadState |clip_rect| if one exists.
//
// Since a quad can only damage pixels it can draw to, the drawable rect is also
// the maximum damage rect a quad can contribute (ignoring pixel-moving
// filters).
gfx::Rect ComputeDrawableRectForQuad(const DrawQuad* quad) {
  const SharedQuadState* sqs = quad->shared_quad_state;

  gfx::Rect drawable_rect = cc::MathUtil::MapEnclosingClippedRect(
      sqs->quad_to_target_transform, quad->visible_rect);
  if (sqs->clip_rect)
    drawable_rect.Intersect(*sqs->clip_rect);

  return drawable_rect;
}

// This function transforms a rect from its target space to the destination
// root target space. If clip_rect is valid, clipping is applied after
// transform.
gfx::Rect TransformRectToDestRootTargetSpace(
    const gfx::Rect& rect_in_target_space,
    const gfx::Transform& target_to_dest_transform,
    const gfx::Transform& dest_to_root_target_transform,
    const std::optional<gfx::Rect> dest_root_target_clip_rect) {
  gfx::Transform target_to_dest_root_target_transform =
      dest_to_root_target_transform * target_to_dest_transform;

  gfx::Rect rect_in_root_target_space = cc::MathUtil::MapEnclosingClippedRect(
      target_to_dest_root_target_transform, rect_in_target_space);

  if (dest_root_target_clip_rect) {
    rect_in_root_target_space.Intersect(*dest_root_target_clip_rect);
  }

  return rect_in_root_target_space;
}

}  // namespace

constexpr base::TimeDelta SurfaceAggregator::kHistogramMinTime;
constexpr base::TimeDelta SurfaceAggregator::kHistogramMaxTime;

struct SurfaceAggregator::PrewalkResult {
  // This is the set of Surfaces that were referenced by another Surface, but
  // not included in a SurfaceDrawQuad.
  base::flat_set<SurfaceId> undrawn_surfaces;
  bool frame_sinks_changed = false;
  bool page_fullscreen_mode = false;
  gfx::ContentColorUsage content_color_usage = gfx::ContentColorUsage::kSRGB;
};

SurfaceAggregator::SurfaceAggregator(
    SurfaceManager* manager,
    DisplayResourceProvider* provider,
    bool needs_surface_damage_rect_list,
    ExtraPassForReadbackOption extra_pass_option,
    bool prevent_merging_surfaces_to_root_pass)
    : manager_(manager),
      provider_(provider),
      needs_surface_damage_rect_list_(needs_surface_damage_rect_list),
      extra_pass_for_readback_option_(extra_pass_option),
      prevent_merging_surfaces_to_root_pass_(
          prevent_merging_surfaces_to_root_pass) {
  DCHECK(manager_);
  DCHECK(provider_);
  manager_->AddObserver(this);
}

SurfaceAggregator::~SurfaceAggregator() {
  manager_->RemoveObserver(this);

  contained_surfaces_.clear();
  contained_frame_sinks_.clear();

  // Notify client of all surfaces being removed.
  ProcessAddedAndRemovedSurfaces();
}

// This function is called at each render pass - CopyQuadsToPass().
void SurfaceAggregator::AddRenderPassFilterDamageToDamageList(
    const ResolvedFrameData& resolved_frame,
    const CompositorRenderPassDrawQuad* render_pass_quad,
    const gfx::Transform& parent_target_transform,
    const std::optional<gfx::Rect> dest_root_target_clip_rect,
    const gfx::Transform& dest_transform_to_root_target) {
  const CompositorRenderPassId child_pass_id = render_pass_quad->render_pass_id;
  const ResolvedPassData& child_resolved_pass =
      resolved_frame.GetRenderPassDataById(child_pass_id);
  const CompositorRenderPass& child_render_pass =
      child_resolved_pass.render_pass();

  // Add damages from render passes with pixel-moving foreground filters or
  // backdrop filters to the surface damage list.
  if (!child_render_pass.filters.HasFilterThatMovesPixels() &&
      !child_render_pass.backdrop_filters.HasFilterThatMovesPixels()) {
    return;
  }

  gfx::Rect damage_rect = render_pass_quad->rect;
  gfx::Rect damage_rect_in_target_space;
  if (child_render_pass.filters.HasFilterThatMovesPixels()) {
    // The size of pixel-moving foreground filter is allowed to expand.
    // No intersecting shared_quad_state->clip_rect for the expanded rect.
    damage_rect_in_target_space =
        GetExpandedRectWithPixelMovingForegroundFilter(
            *render_pass_quad, child_render_pass.filters);
  } else if (child_render_pass.backdrop_filters.HasFilterThatMovesPixels()) {
    const auto* shared_quad_state = render_pass_quad->shared_quad_state;
    damage_rect_in_target_space = cc::MathUtil::MapEnclosingClippedRect(
        shared_quad_state->quad_to_target_transform, damage_rect);
    if (shared_quad_state->clip_rect) {
      damage_rect_in_target_space.Intersect(
          shared_quad_state->clip_rect.value());
    }
  }

  gfx::Rect damage_rect_in_root_target_space =
      TransformRectToDestRootTargetSpace(
          damage_rect_in_target_space, parent_target_transform,
          dest_transform_to_root_target, dest_root_target_clip_rect);

  // The whole render pass rect with pixel-moving foreground filters or
  // backdrop filters is considered damaged if it intersects with the other
  // damages.
  if (damage_rect_in_root_target_space.Intersects(root_damage_rect_)) {
    // Since |damage_rect_in_root_target_space| is available, just pass this
    // rect and reset the other arguments.
    AddSurfaceDamageToDamageList(
        damage_rect_in_root_target_space,
        /*parent_target_transform*/ gfx::Transform(),
        /*dest_root_target_clip_rect*/ {},
        /*dest_transform_to_root_target*/ gfx::Transform(),
        /*resolved_frame=*/nullptr);
  }
}

void SurfaceAggregator::AddSurfaceDamageToDamageList(
    const gfx::Rect& default_damage_rect,
    const gfx::Transform& parent_target_transform,
    const std::optional<gfx::Rect> dest_root_target_clip_rect,
    const gfx::Transform& dest_transform_to_root_target,
    ResolvedFrameData* resolved_frame) {
  gfx::Rect damage_rect;
  if (!resolved_frame) {
    // When the surface is null, it's either the surface is lost or it comes
    // from a render pass with filters.
    damage_rect = default_damage_rect;
  } else {
    if (RenderPassNeedsFullDamage(resolved_frame->GetRootRenderPassData())) {
      damage_rect = resolved_frame->GetOutputRect();
    } else {
      damage_rect = resolved_frame->GetSurfaceDamage();
    }
  }

  if (damage_rect.IsEmpty()) {
    current_zero_damage_rect_is_not_recorded_ = true;
    return;
  }
  current_zero_damage_rect_is_not_recorded_ = false;

  gfx::Rect damage_rect_in_root_target_space =
      TransformRectToDestRootTargetSpace(
          /*rect_in_target_space=*/damage_rect, parent_target_transform,
          dest_transform_to_root_target, dest_root_target_clip_rect);

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
    const gfx::Transform& pass_to_root_target_transform,
    size_t* overlay_damage_index) {
  // The occluding damage optimization currently relies on two things - there
  // can't be any damage above the quad within the surface, and the quad needs
  // its own SQS for the occluding_damage_rect metadata.
  const DrawQuad* target_quad = nullptr;
  for (auto* quad : source_pass.quad_list) {
    // Quads with |per_quad_damage| do not contribute to the |damage_rect| in
    // the |source_pass|. These quads are also assumed to have unique SQS
    // objects.
    if (source_pass.has_per_quad_damage) {
      auto optional_damage = GetOptionalDamageRectFromQuad(quad);
      if (optional_damage.has_value()) {
        continue;
      }
    }

    if (target_quad == nullptr) {
      target_quad = quad;
    } else {
      // More that one quad without per_quad_damage.
      target_quad = nullptr;
      break;
    }
  }

  // No overlay candidate is found.
  if (!target_quad)
    return nullptr;

  // Surface damage for a render pass quad does not include damage from its
  // children. We skip this quad to avoid the incomplete damage association.
  if (target_quad->material == DrawQuad::Material::kCompositorRenderPass ||
      target_quad->material == DrawQuad::Material::kSurfaceContent)
    return nullptr;

  // Zero damage is not recorded in the surface_damage_rect_list_.
  // In this case, add an empty damage rect to the list so
  // |overlay_damage_index| can save this index.
  if (current_zero_damage_rect_is_not_recorded_) {
    current_zero_damage_rect_is_not_recorded_ = false;
    surface_damage_rect_list_->push_back(gfx::Rect());
  }

  // Before assigning a surface damage rect to this quad, make sure that it is
  // not larger than the quad itself. This is possible when a quad is smaller
  // than it was last frame, or when it moves. The damage should be the size of
  // larger rect from last frame because we need to damage what's underneath the
  // quad. So if we promote the now smaller quad to an overlay this frame we
  // should not remove this damage rect. i.e. we should not assign the damage
  // rect to this quad.
  // For similar reasons, we should not assign damage to quads with non-axis
  // aligned transforms, because those won't be promoted to overlay.
  auto& damage_rect_in_root_space = surface_damage_rect_list_->back();
  if (!damage_rect_in_root_space.IsEmpty()) {
    gfx::Transform quad_to_root_transform =
        pass_to_root_target_transform *
        target_quad->shared_quad_state->quad_to_target_transform;
    if (!quad_to_root_transform.Preserves2dAxisAlignment()) {
      return nullptr;
    }

    gfx::RectF rect_in_root_space = cc::MathUtil::MapClippedRect(
        quad_to_root_transform, gfx::RectF(target_quad->rect));
    // Because OverlayCandidate.damage_rect is a gfx::Rect, we can't really
    // assign damage if the display_rect is not pixel-aligned.
    if (!gfx::IsNearestRectWithinDistance(rect_in_root_space, 0.01f)) {
      return nullptr;
    }
    // Now, in order to check whether the display rect (gfx::RectF) contains the
    // damage rect (gfx::Rect), we can safely round the former so that we do not
    // fail to assign the damage due to 4-6 digits difference.
    if (!gfx::ToEnclosingRectIgnoringError(rect_in_root_space)
             .Contains(damage_rect_in_root_space)) {
      return nullptr;
    }
  }

  // The latest surface damage rect.
  *overlay_damage_index = surface_damage_rect_list_->size() - 1;

  return target_quad;
}

bool SurfaceAggregator::CanPotentiallyMergePass(
    const SurfaceDrawQuad& surface_quad) {
  const SharedQuadState* sqs = surface_quad.shared_quad_state;
  return surface_quad.allow_merge &&
         base::IsApproximatelyEqual(sqs->opacity, 1.f, kOpacityEpsilon);
}

void SurfaceAggregator::OnSurfaceDestroyed(const SurfaceId& surface_id) {
  DCHECK(!is_inside_aggregate_);

  auto iter = resolved_frames_.find(surface_id);
  if (iter != resolved_frames_.end()) {
    TRACE_EVENT0("viz", "SurfaceAggregator::SurfaceDestroyed");
    resolved_frames_.erase(iter);
  }
}

const ResolvedFrameData* SurfaceAggregator::GetLatestFrameData(
    const SurfaceId& surface_id) {
  DCHECK(!is_inside_aggregate_);
  return GetResolvedFrame(surface_id);
}

ResolvedFrameData* SurfaceAggregator::GetResolvedFrame(
    const SurfaceRange& range) {
  // Find latest in flight surface and cache that result for the duration of
  // this aggregation, then find ResolvedFrameData for that surface.
  auto iter = resolved_surface_ranges_.find(range);
  if (iter == resolved_surface_ranges_.end()) {
    auto* surface = manager_->GetLatestInFlightSurface(range);
    SurfaceId surface_id = surface ? surface->surface_id() : SurfaceId();
    iter = resolved_surface_ranges_.emplace(range, surface_id).first;
  }

  if (!iter->second.is_valid()) {
    // There is no surface for `range`.
    return nullptr;
  }

  return GetResolvedFrame(iter->second);
}

ResolvedFrameData* SurfaceAggregator::GetResolvedFrame(
    const SurfaceId& surface_id) {
  DCHECK(surface_id.is_valid());

  auto iter = resolved_frames_.find(surface_id);
  if (iter == resolved_frames_.end()) {
    auto* surface = manager_->GetSurfaceForId(surface_id);
    if (!surface || !surface->HasActiveFrame()) {
      // If there is no resolved surface or the surface has no active frame
      // there is no resolved frame data to return.
      return nullptr;
    }

    AggregatedRenderPassId prev_root_pass_id;
    uint64_t prev_frame_index = 0u;
    // If this is the first frame in a new surface there might be damage
    // compared to the previous frame in a different surface.
    if (surface->surface_id() != surface->previous_frame_surface_id()) {
      auto prev_resolved_frame_iter =
          resolved_frames_.find(surface->previous_frame_surface_id());
      if (prev_resolved_frame_iter != resolved_frames_.end()) {
        auto& prev_resolved_frame = prev_resolved_frame_iter->second;
        if (prev_resolved_frame.is_valid()) {
          prev_frame_index = prev_resolved_frame.previous_frame_index();
          prev_root_pass_id =
              prev_resolved_frame.GetRootRenderPassData().remapped_id();
        }
      }
    }

    iter = resolved_frames_
               .emplace(
                   std::piecewise_construct, std::forward_as_tuple(surface_id),
                   std::forward_as_tuple(provider_, surface, prev_frame_index,
                                         prev_root_pass_id))
               .first;
  }

  ResolvedFrameData& resolved_frame = iter->second;

  if (is_inside_aggregate_ && !resolved_frame.WasUsedInAggregation()) {
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);

    // Lookup function allows ResolvedFrameData to find OffsetTagValues.
    auto lookup_fn = [this](const OffsetTagDefinition& tag_def) {
      if (auto* provider_frame = GetResolvedFrame(tag_def.provider)) {
        auto& tag_values = provider_frame->GetMetadata().offset_tag_values;
        for (auto& tag_value : tag_values) {
          if (tag_def.tag == tag_value.tag) {
            return tag_value.offset;
          }
        }
      }
      return gfx::Vector2dF();
    };
    resolved_frame.UpdateOffsetTags(lookup_fn);
  }

  return &resolved_frame;
}

void SurfaceAggregator::HandleSurfaceQuad(
    const CompositorRenderPass& source_pass,
    const SurfaceDrawQuad* surface_quad,
    uint32_t embedder_client_namespace_id,
    float parent_device_scale_factor,
    const gfx::Transform& target_transform,
    const std::optional<gfx::Rect> added_clip_rect,
    const std::optional<gfx::Rect> dest_root_target_clip_rect,
    AggregatedRenderPass* dest_pass,
    const MaskFilterInfoExt& mask_filter_info) {
  DCHECK(target_transform.Preserves2dAxisAlignment());

  SurfaceId primary_surface_id = surface_quad->surface_range.end();
  ResolvedFrameData* resolved_frame =
      GetResolvedFrame(surface_quad->surface_range);

  // |added_clip_rect| should be bounded by the output_rect of the render pass
  // that contains |surface_quad|.
  std::optional<gfx::Rect> surface_clip_rect = CalculateClipRect(
      added_clip_rect, source_pass.output_rect, target_transform);

  // If a new surface is going to be emitted, add the surface_quad rect to
  // |surface_damage_rect_list_| for overlays. The whole quad is considered
  // damaged.
  std::optional<gfx::Rect> combined_clip_rect;

  gfx::Rect surface_in_target_space = ComputeDrawableRectForQuad(surface_quad);
  surface_in_target_space.Intersect(source_pass.output_rect);

  if (needs_surface_damage_rect_list_ &&
      (!resolved_frame || resolved_frame->surface_id() != primary_surface_id)) {
    // If using a fallback surface the surface content may be stretched or
    // have gutter. If the surface is missing the content will be filled
    // with a solid color. In both cases we no longer have frame-to-frame
    // damage so treat the entire SurfaceDrawQuad visible_rect as damaged.
    // |combined_clip_rect| is the transforming and clipping result of the
    // entire SurfaceDrawQuad visible_rect on the root target space of the
    // root surface.
    AddSurfaceDamageToDamageList(surface_in_target_space, target_transform,
                                 dest_root_target_clip_rect,
                                 dest_pass->transform_to_root_target,
                                 /*resolved_frame=*/nullptr);
  }

    // combined_clip_rect is the result of |dest_root_target_clip_rect|
    // intersecting |surface_quad| on the root target space of the root surface.
  combined_clip_rect = TransformRectToDestRootTargetSpace(
      /*rect_in_target_space=*/surface_in_target_space, target_transform,
      dest_pass->transform_to_root_target, dest_root_target_clip_rect);

  // If there's no fallback surface ID available, then simply emit a
  // SolidColorDrawQuad with the provided default background color. This
  // can happen after a Viz process crash.
  if (!resolved_frame) {
    EmitDefaultBackgroundColorQuad(surface_quad, embedder_client_namespace_id,
                                   target_transform, surface_clip_rect,
                                   dest_pass, mask_filter_info);
    return;
  }

  if (resolved_frame->surface_id() != primary_surface_id &&
      !surface_quad->stretch_content_to_fill_bounds) {
    gfx::Rect fallback_rect(resolved_frame->size_in_pixels());

    float scale_ratio =
        parent_device_scale_factor / resolved_frame->device_scale_factor();
    fallback_rect =
        gfx::ScaleToEnclosingRect(fallback_rect, scale_ratio, scale_ratio);
    fallback_rect =
        gfx::IntersectRects(fallback_rect, surface_quad->visible_rect);

    auto background_color = resolved_frame->GetMetadata().root_background_color;

    // TODO(crbug.com/40219248): CompositorFrameMetadata to SkColor4f
    EmitGutterQuadsIfNecessary(surface_quad->visible_rect, fallback_rect,
                               surface_quad->shared_quad_state,
                               embedder_client_namespace_id, target_transform,
                               surface_clip_rect, background_color, dest_pass,
                               mask_filter_info);
  }

  EmitSurfaceContent(*resolved_frame, parent_device_scale_factor, surface_quad,
                     embedder_client_namespace_id, target_transform,
                     surface_clip_rect, combined_clip_rect, dest_pass,
                     mask_filter_info);
}

void SurfaceAggregator::EmitSurfaceContent(
    ResolvedFrameData& resolved_frame,
    float parent_device_scale_factor,
    const SurfaceDrawQuad* surface_quad,
    uint32_t embedder_client_namespace_id,
    const gfx::Transform& target_transform,
    const std::optional<gfx::Rect> added_clip_rect,
    const std::optional<gfx::Rect> dest_root_target_clip_rect,
    AggregatedRenderPass* dest_pass,
    const MaskFilterInfoExt& mask_filter_info) {
  Surface* surface = resolved_frame.surface();

  // If this surface's id is already in our referenced set then it creates
  // a cycle in the graph and should be dropped.
  SurfaceId surface_id = surface->surface_id();
  if (referenced_surfaces_.count(surface_id))
    return;

  ++stats_->copied_surface_count;


  // If we are stretching content to fill the SurfaceDrawQuad, or if the device
  // scale factor mismatches between content and SurfaceDrawQuad, we appply an
  // additional scale.
  float extra_content_scale_x, extra_content_scale_y;
  if (surface_quad->stretch_content_to_fill_bounds) {
    const gfx::Rect& surface_quad_rect = surface_quad->rect;
    // Stretches the surface contents to exactly fill the layer bounds,
    // regardless of scale or aspect ratio differences.
    extra_content_scale_x =
        surface_quad_rect.width() /
        static_cast<float>(resolved_frame.size_in_pixels().width());
    extra_content_scale_y =
        surface_quad_rect.height() /
        static_cast<float>(resolved_frame.size_in_pixels().height());
  } else {
    extra_content_scale_x = extra_content_scale_y =
        parent_device_scale_factor / resolved_frame.device_scale_factor();
  }
  float inverse_extra_content_scale_x = SK_Scalar1 / extra_content_scale_x;
  float inverse_extra_content_scale_y = SK_Scalar1 / extra_content_scale_y;

  const SharedQuadState* surface_quad_sqs = surface_quad->shared_quad_state;
  gfx::Transform scaled_quad_to_target_transform(
      surface_quad_sqs->quad_to_target_transform);
  scaled_quad_to_target_transform.Scale(extra_content_scale_x,
                                        extra_content_scale_y);

  // A map keyed by RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  if (take_copy_requests_) {
    surface->TakeCopyOutputRequests(&copy_requests);
  }

  if (!resolved_frame.is_valid()) {
    // As |copy_requests| goes out-of-scope, all copy requests in that container
    // will auto-send an empty result upon destruction.
    return;
  }

  const auto& frame_metadata = resolved_frame.GetMetadata();
  flow_ids_for_resolved_frames_.insert(frame_metadata.begin_frame_ack.trace_id);

  referenced_surfaces_.insert(surface_id);

  gfx::Transform combined_transform = scaled_quad_to_target_transform;
  combined_transform.PostConcat(target_transform);

  // If the SurfaceDrawQuad is marked as being reflected and surface contents
  // are going to be scaled then keep the RenderPass. This allows the reflected
  // surface to be drawn with AA enabled for smooth scaling and preserves the
  // original reflector scaling behaviour which scaled a TextureLayer.
  bool reflected_and_scaled =
      surface_quad->is_reflection &&
      !scaled_quad_to_target_transform.IsIdentityOrTranslation();

  const bool pass_is_mergeable =
      CanPotentiallyMergePass(*surface_quad) && !reflected_and_scaled &&
      combined_transform.Preserves2dAxisAlignment() &&
      mask_filter_info.CanMergeMaskFilterInfo(
          resolved_frame.GetRootRenderPassData().render_pass(),
          combined_transform) &&
      !resolved_frame.GetRootRenderPassData().aggregation().prevent_merge;

  // When a surface has video capture enabled, but no copy requests, we do not
  // require an intermediate surface. However, video capture being enabled is a
  // hint that we will have a copy request soon, so we prevent |merge_pass| to
  // avoid thrashing on the render pass backing allocation.
  const bool has_video_capture =
      !copy_requests.empty() || surface->IsVideoCaptureOnFromClient();

  const bool merge_pass = pass_is_mergeable && !has_video_capture;

  // Update PersistentPassData.merge_status of the root render pass of the
  // current frame before making a call to AddSurfaceDamageToDamageList() where
  // RenderPassNeedsFullDamage() is called and needs root pass |merge_state|
  // info.
  UpdatePersistentPassDataMergeState(resolved_frame.GetRootRenderPassData(),
                                     dest_pass, merge_pass);

  if (needs_surface_damage_rect_list_ && resolved_frame.WillDraw()) {
    AddSurfaceDamageToDamageList(
        /*default_damage_rect =*/gfx::Rect(), combined_transform,
        dest_root_target_clip_rect, dest_pass->transform_to_root_target,
        &resolved_frame);
  }

  if (frame_metadata.delegated_ink_metadata) {
    AggregatedRenderPassId render_pass_with_delegated_ink =
        merge_pass ? dest_pass->id
                   : resolved_frame.GetRootRenderPassData().remapped_id();
    // Copy delegated ink metadata from the compositor frame metadata. This
    // prevents the delegated ink trail from flickering if a compositor frame
    // is not generated due to a delayed main frame.
    TransformAndStoreDelegatedInkMetadata(
        dest_pass->transform_to_root_target * combined_transform,
        frame_metadata.delegated_ink_metadata.get(),
        render_pass_with_delegated_ink);
  }

  // TODO(fsamuel): Move this to a separate helper function.
  auto& resolved_passes = resolved_frame.GetResolvedPasses();
  size_t num_render_passes = resolved_passes.size();
  size_t passes_to_copy =
      merge_pass ? num_render_passes - 1 : num_render_passes;
  for (size_t j = 0; j < passes_to_copy; ++j) {
    ResolvedPassData& resolved_pass = resolved_passes[j];
    const CompositorRenderPass& source = resolved_pass.render_pass();

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    auto copy_pass = std::make_unique<AggregatedRenderPass>(sqs_size, dq_size);

    gfx::Rect output_rect = source.output_rect;
    if (max_render_target_size_ > 0) {
      output_rect.set_width(
          std::min(output_rect.width(), max_render_target_size_));
      output_rect.set_height(
          std::min(output_rect.height(), max_render_target_size_));
    }
    copy_pass->SetAll(
        resolved_pass.remapped_id(), output_rect, output_rect,
        source.transform_to_root_target, source.filters,
        source.backdrop_filters, source.backdrop_filter_bounds,
        root_content_color_usage_, source.has_transparent_background,
        source.cache_render_pass, resolved_pass.aggregation().has_damage,
        source.generate_mipmap);

    copy_pass->is_from_surface_root_pass = resolved_pass.is_root();

    UpdatePersistentPassDataMergeState(resolved_pass, copy_pass.get(),
                                       /*is_merged_pass=*/false);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    // Contributing passes aggregated in to the pass list need to take the
    // transform of the surface quad into account to update their transform to
    // the root surface.
    copy_pass->transform_to_root_target.PostConcat(combined_transform);
    copy_pass->transform_to_root_target.PostConcat(
        dest_pass->transform_to_root_target);

    CopyQuadsToPass(resolved_frame, resolved_pass, copy_pass.get(),
                    resolved_frame.device_scale_factor(), gfx::Transform(), {},
                    dest_root_target_clip_rect, MaskFilterInfoExt());

    SetRenderPassDamageRect(copy_pass.get(), resolved_pass);

    dest_pass_list_->push_back(std::move(copy_pass));
  }

  if (surface->IsVideoCaptureOnFromClient()) {
    CHECK(!merge_pass);
    dest_pass_list_->back()->video_capture_enabled = true;
  }

  auto& resolved_root_pass = resolved_frame.GetRootRenderPassData();

  // This hack allows for quads that require overlay to appear in a render pass
  // for a copy request as well as be merged into the dest pass (eventually the
  // root) to be promoted to overlay. This allows e.g. protected content to be
  // visible to the user, even if something is capturing the tab (the protected
  // content will still not appear in the capture). Note this does not handle
  // the case when the root pass is captured with protected content, which needs
  // to be handled during overlay processing.
  //
  // It works by preventing merging when there is a copy request (as usual), so
  // we have an intermediate render pass (and backing) that can service the copy
  // request. Then, we detect here if the render pass has quads that require
  // overlay and could've otherwise merged. If so, we force a merge, resulting
  // in a copy of the render pass quads in the intermediate pass and a copy in
  // the dest pass. Since we are not copying the copy request itself to the dest
  // pass, the quads that require overlay can still be promoted to overlay.
  const bool allow_forced_merge_pass = base::FeatureList::IsEnabled(
      features::kAllowForceMergeRenderPassWithRequireOverlayQuads);
  const bool force_merge_pass =
      allow_forced_merge_pass && !merge_pass && pass_is_mergeable &&
      base::ranges::any_of(dest_pass_list_->back()->quad_list,
                           &OverlayCandidate::RequiresOverlay);

  if (merge_pass || force_merge_pass) {
    // Compute a clip rect in |dest_pass| coordinate space to ensure merged
    // surface cannot draw outside where a non-merged surface would draw. An
    // enclosing rect in |surface_quad| target render pass coordinate space is
    // computed, then transformed into |dest_pass| coordinate space and finally
    // that is intersected with existing |added_clip_rect|.
    std::optional<gfx::Rect> surface_quad_clip = CalculateClipRect(
        added_clip_rect, ComputeDrawableRectForQuad(surface_quad),
        target_transform);

    // UpdatePersistentPassDataMergeState() has been called earlier.
    CopyQuadsToPass(resolved_frame, resolved_root_pass, dest_pass,
                    resolved_frame.device_scale_factor(), combined_transform,
                    surface_quad_clip, dest_root_target_clip_rect,
                    mask_filter_info);
  } else {
    auto* shared_quad_state = CopyAndScaleSharedQuadState(
        surface_quad_sqs, embedder_client_namespace_id,
        scaled_quad_to_target_transform, target_transform,
        gfx::ScaleToEnclosingRect(surface_quad_sqs->quad_layer_rect,
                                  inverse_extra_content_scale_x,
                                  inverse_extra_content_scale_y),
        gfx::ScaleToEnclosingRect(surface_quad_sqs->visible_quad_layer_rect,
                                  inverse_extra_content_scale_x,
                                  inverse_extra_content_scale_y),
        added_clip_rect, mask_filter_info, dest_pass);

    // At this point, we need to calculate three values in order to construct
    // the CompositorRenderPassDrawQuad:

    // |quad_rect| - A rectangle representing the RenderPass's output area in
    //   content space. This is equal to the root render pass (|last_pass|)
    //   output rect.
    gfx::Rect quad_rect = resolved_root_pass.render_pass().output_rect;

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
        surface_quad->visible_rect, inverse_extra_content_scale_x,
        inverse_extra_content_scale_y));

    // |tex_coord_rect| - A rectangle representing the bounds of the texture
    //   in the RenderPass's |quad_rect|. Not in content space, instead as an
    //   offset within |quad_rect|.
    gfx::RectF tex_coord_rect = gfx::RectF(gfx::SizeF(quad_rect.size()));

    // We can't produce content outside of |quad_rect|, so clip the visible
    // rect if necessary.
    quad_visible_rect.Intersect(quad_rect);
    auto remapped_pass_id = resolved_root_pass.remapped_id();
    if (quad_visible_rect.IsEmpty()) {
      std::erase_if(*dest_pass_list_,
                    [&remapped_pass_id](
                        const std::unique_ptr<AggregatedRenderPass>& pass) {
                      return pass->id == remapped_pass_id;
                    });
    } else {
      auto* quad =
          dest_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      quad->SetNew(shared_quad_state, quad_rect, quad_visible_rect,
                   remapped_pass_id, kInvalidResourceId, gfx::RectF(),
                   gfx::Size(), gfx::Vector2dF(1.0f, 1.0f), gfx::PointF(),
                   tex_coord_rect,
                   /*force_anti_aliasing_off=*/false,
                   /* backdrop_filter_quality*/ 1.0f);
    }
  }

  referenced_surfaces_.erase(surface_id);
}

void SurfaceAggregator::EmitDefaultBackgroundColorQuad(
    const SurfaceDrawQuad* surface_quad,
    uint32_t embedder_client_namespace_id,
    const gfx::Transform& target_transform,
    const std::optional<gfx::Rect> clip_rect,
    AggregatedRenderPass* dest_pass,
    const MaskFilterInfoExt& mask_filter_info) {
  TRACE_EVENT1("viz", "SurfaceAggregator::EmitDefaultBackgroundColorQuad",
               "surface_range", surface_quad->surface_range.ToString());

  // No matching surface was found so create a SolidColorDrawQuad with the
  // SurfaceDrawQuad default background color.
  SkColor4f background_color = surface_quad->default_background_color;
  auto* shared_quad_state = CopySharedQuadState(
      surface_quad->shared_quad_state, embedder_client_namespace_id,
      target_transform, clip_rect, mask_filter_info, dest_pass);

  auto* solid_color_quad =
      dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_color_quad->SetNew(shared_quad_state, surface_quad->rect,
                           surface_quad->visible_rect, background_color, false);
}

void SurfaceAggregator::EmitGutterQuadsIfNecessary(
    const gfx::Rect& primary_rect,
    const gfx::Rect& fallback_rect,
    const SharedQuadState* primary_shared_quad_state,
    uint32_t embedder_client_namespace_id,
    const gfx::Transform& target_transform,
    const std::optional<gfx::Rect> clip_rect,
    SkColor4f background_color,
    AggregatedRenderPass* dest_pass,
    const MaskFilterInfoExt& mask_filter_info) {
  bool has_transparent_background = background_color == SkColors::kTransparent;

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
        primary_shared_quad_state, embedder_client_namespace_id,
        primary_shared_quad_state->quad_to_target_transform, target_transform,
        right_gutter_rect, right_gutter_rect, clip_rect, mask_filter_info,
        dest_pass);

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
        primary_shared_quad_state, embedder_client_namespace_id,
        primary_shared_quad_state->quad_to_target_transform, target_transform,
        bottom_gutter_rect, bottom_gutter_rect, clip_rect, mask_filter_info,
        dest_pass);

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
  // space is unsuitable as a blending color space and the root render pass
  // requires blending.
  bool needs_color_conversion_pass =
      !display_color_spaces_
           .GetOutputColorSpace(root_render_pass->content_color_usage,
                                root_render_pass->has_transparent_background)
           .IsSuitableForBlending();
  needs_color_conversion_pass &= root_render_pass->ShouldDrawWithBlending();

  // If we added or removed the color conversion pass, we need to add full
  // damage to the current-root renderpass (and also the new-root renderpass,
  // if the current-root renderpass becomes and intermediate renderpass).
  if (needs_color_conversion_pass != last_frame_had_color_conversion_pass_)
    root_render_pass->damage_rect = output_rect;

  last_frame_had_color_conversion_pass_ = needs_color_conversion_pass;
  if (!needs_color_conversion_pass)
    return;
  CHECK(root_render_pass->transform_to_root_target == gfx::Transform());

  if (!color_conversion_render_pass_id_) {
    color_conversion_render_pass_id_ =
        render_pass_id_generator_.GenerateNextId();
  }

  AddRenderPassHelper(color_conversion_render_pass_id_, output_rect,
                      root_render_pass->damage_rect, root_content_color_usage_,
                      root_render_pass->has_transparent_background,
                      /*pass_is_color_conversion_pass=*/true,
                      /*quad_state_to_target_transform=*/gfx::Transform(),
                      /*quad_state_contents_opaque=*/false, SkBlendMode::kSrc,
                      root_render_pass->id);
}

void SurfaceAggregator::AddRootReadbackPass() {
  if (extra_pass_for_readback_option_ == ExtraPassForReadbackOption::kNone) {
    return;
  }

  auto* root_render_pass = dest_pass_list_->back().get();
  gfx::Rect output_rect = root_render_pass->output_rect;
  CHECK(root_render_pass->transform_to_root_target == gfx::Transform());
  bool needs_readback_pass = false;
  // Check if there are any render passes that draw into the root pass with
  // a backdrop filter.
  base::flat_set<AggregatedRenderPassId> pass_ids_drawing_to_root;
  for (auto* quad : root_render_pass->quad_list) {
    if (auto* render_pass_quad =
            quad->DynamicCast<AggregatedRenderPassDrawQuad>()) {
      pass_ids_drawing_to_root.insert(render_pass_quad->render_pass_id);
    }
  }
  if (!pass_ids_drawing_to_root.empty()) {
    for (auto& render_pass : *dest_pass_list_) {
      if (!pass_ids_drawing_to_root.contains(render_pass->id))
        continue;
      if (!render_pass->backdrop_filters.IsEmpty()) {
        needs_readback_pass = true;
        break;
      }
    }
  }

  if (extra_pass_for_readback_option_ ==
      ExtraPassForReadbackOption::kAlwaysAddPass) {
    needs_readback_pass = true;
  }

  if (needs_readback_pass != last_frame_had_readback_pass_)
    root_render_pass->damage_rect = output_rect;

  last_frame_had_readback_pass_ = needs_readback_pass;
  if (!last_frame_had_readback_pass_)
    return;

  if (!readback_render_pass_id_) {
    readback_render_pass_id_ = render_pass_id_generator_.GenerateNextId();
  }

  // Ensure the root-that's-non-root pass is cleared to fully transparent first.
  bool has_transparent_background =
      root_render_pass->has_transparent_background;
  root_render_pass->has_transparent_background = true;
  AddRenderPassHelper(readback_render_pass_id_, output_rect,
                      root_render_pass->damage_rect, root_content_color_usage_,
                      has_transparent_background,
                      /*pass_is_color_conversion_pass=*/false,
                      /*quad_state_to_target_transform=*/gfx::Transform(),
                      /*quad_state_contents_opaque=*/false,
                      SkBlendMode::kSrcOver, root_render_pass->id);
}

void SurfaceAggregator::AddDisplayTransformPass() {
  if (dest_pass_list_->empty())
    return;

  auto* root_render_pass = dest_pass_list_->back().get();
  DCHECK(root_render_pass->transform_to_root_target == root_surface_transform_);

  if (!display_transform_render_pass_id_) {
    display_transform_render_pass_id_ =
        render_pass_id_generator_.GenerateNextId();
  }

  bool are_contents_opaque = true;
  for (const auto* sqs : root_render_pass->shared_quad_state_list) {
    if (!sqs->are_contents_opaque) {
      are_contents_opaque = false;
      break;
    }
  }

  AddRenderPassHelper(
      display_transform_render_pass_id_,
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, root_render_pass->output_rect),
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, root_render_pass->damage_rect),
      root_render_pass->content_color_usage,
      root_render_pass->has_transparent_background,
      /*pass_is_color_conversion_pass=*/false, root_surface_transform_,
      are_contents_opaque, SkBlendMode::kSrcOver, root_render_pass->id);
}

void SurfaceAggregator::AddRenderPassHelper(
    AggregatedRenderPassId render_pass_id,
    const gfx::Rect& render_pass_output_rect,
    const gfx::Rect& render_pass_damage_rect,
    gfx::ContentColorUsage pass_color_usage,
    bool pass_has_transparent_background,
    bool pass_is_color_conversion_pass,
    const gfx::Transform& quad_state_to_target_transform,
    bool quad_state_contents_opaque,
    SkBlendMode quad_state_blend_mode,
    AggregatedRenderPassId quad_pass_id) {
  gfx::Rect current_output_rect = dest_pass_list_->back()->output_rect;

  auto render_pass = std::make_unique<AggregatedRenderPass>(1, 1);
  render_pass->SetAll(render_pass_id, render_pass_output_rect,
                      render_pass_damage_rect, gfx::Transform(),
                      /*filters=*/cc::FilterOperations(),
                      /*backdrop_filters=*/cc::FilterOperations(),
                      /*backdrop_filter_bounds=*/gfx::RRectF(),
                      pass_color_usage, pass_has_transparent_background,
                      /*cache_render_pass=*/false,
                      /*has_damage_from_contributing_content=*/false,
                      /*generate_mipmap=*/false);
  render_pass->is_color_conversion_pass = pass_is_color_conversion_pass;

  auto* shared_quad_state = render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(
      quad_state_to_target_transform,
      /*layer_rect=*/current_output_rect,
      /*visible_layer_rect=*/current_output_rect, gfx::MaskFilterInfo(),
      /*clip=*/std::nullopt, quad_state_contents_opaque, /*opacity_f=*/1.f,
      quad_state_blend_mode, /*sorting_context=*/0, /*layer_id*/ 0u,
      /*fast_rounded_corner=*/false);

  auto* quad =
      render_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, current_output_rect, current_output_rect,
               quad_pass_id, kInvalidResourceId, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1.0f, 1.0f), gfx::PointF(),
               gfx::RectF(current_output_rect),
               /*force_anti_aliasing_off=*/false,
               /*backdrop_filter_quality*/ 1.0f);
  dest_pass_list_->push_back(std::move(render_pass));
}

void SurfaceAggregator::CopyQuadsToPass(
    ResolvedFrameData& resolved_frame,
    ResolvedPassData& resolved_pass,
    AggregatedRenderPass* dest_pass,
    float parent_device_scale_factor,
    const gfx::Transform& target_transform,
    const std::optional<gfx::Rect> clip_rect,
    const std::optional<gfx::Rect> dest_root_target_clip_rect,
    const MaskFilterInfoExt& parent_mask_filter_info_ext) {
  const CompositorRenderPass& source_pass = resolved_pass.render_pass();
  const QuadList& source_quad_list = source_pass.quad_list;
  const SharedQuadState* last_copied_source_shared_quad_state = nullptr;

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

  const gfx::Transform pass_to_dest_root_target_transform =
      dest_pass->transform_to_root_target * target_transform;

  size_t overlay_damage_index = 0;
  const DrawQuad* quad_with_overlay_damage_index = nullptr;
  // Only process the damage rect at the root render pass, once per surface.
  if (needs_surface_damage_rect_list_ &&
      resolved_pass.aggregation().will_draw && resolved_pass.is_root()) {
    // TODO(crbug.com/40224514): If there is one specific quad for this pass's
    // damage we should move the allocation of the damage index below to be
    // consistent with quad ordering.
    quad_with_overlay_damage_index = FindQuadWithOverlayDamage(
        source_pass, dest_pass, pass_to_dest_root_target_transform,
        &overlay_damage_index);
  }

  // Add render pass |output_rect| to |dest_root_target_clip_rect|.
  auto new_dest_root_target_clip_rect = CalculateClipRect(
      dest_root_target_clip_rect, resolved_pass.render_pass().output_rect,
      pass_to_dest_root_target_transform);

  UpdateNeedsRedraw(resolved_pass, dest_pass, new_dest_root_target_clip_rect);

  size_t quad_index = 0;
  auto& resolved_draw_quads = resolved_pass.draw_quads();

  uint32_t client_namespace_id = resolved_frame.GetClientNamespaceId();

  for (auto* quad : source_quad_list) {
    const ResolvedQuadData& quad_data = resolved_draw_quads[quad_index++];

    // Both cannot be set at once (rounded corners are exception to this). If
    // this happens then a surface is being merged when it should not.
    DCHECK(!quad->shared_quad_state->mask_filter_info.HasGradientMask() ||
           !parent_mask_filter_info_ext.mask_filter_info.HasGradientMask());

    MaskFilterInfoExt new_mask_filter_info_ext = parent_mask_filter_info_ext;
    if (!quad->shared_quad_state->mask_filter_info.IsEmpty()) {
      new_mask_filter_info_ext = MaskFilterInfoExt(
          quad->shared_quad_state->mask_filter_info,
          quad->shared_quad_state->is_fast_rounded_corner, target_transform);
    }

    if (quad->material == DrawQuad::Material::kSharedElement) {
      // SharedElement quads should've been resolved before aggregation.
      continue;
    } else if (const auto* surface_quad =
                   quad->DynamicCast<SurfaceDrawQuad>()) {
      // HandleSurfaceQuad may add other shared quad state, so reset the
      // current data.
      last_copied_source_shared_quad_state = nullptr;

      if (!surface_quad->surface_range.end().is_valid())
        continue;

      HandleSurfaceQuad(source_pass, surface_quad, client_namespace_id,
                        parent_device_scale_factor, target_transform, clip_rect,
                        new_dest_root_target_clip_rect, dest_pass,
                        new_mask_filter_info_ext);
    } else {
      // Here we output the optional quad's |per_quad_damage| to the
      // |surface_damage_rect_list_|. Any non per quad damage associated with
      // this |source_pass| will have been added to the
      // |surface_damage_rect_list_| before this phase.
      bool needs_sqs =
          quad->shared_quad_state != last_copied_source_shared_quad_state;
      bool has_per_quad_damage =
          source_pass.has_per_quad_damage &&
          GetOptionalDamageRectFromQuad(quad).has_value() &&
          resolved_pass.aggregation().will_draw;

      if (needs_sqs || has_per_quad_damage) {
        SharedQuadState* dest_shared_quad_state = CopySharedQuadState(
            quad->shared_quad_state, client_namespace_id, target_transform,
            clip_rect, new_mask_filter_info_ext, dest_pass);

        if (has_per_quad_damage) {
          auto damage_rect_in_target_space =
              GetOptionalDamageRectFromQuad(quad);
          dest_shared_quad_state->overlay_damage_index =
              surface_damage_rect_list_->size();
          AddSurfaceDamageToDamageList(damage_rect_in_target_space.value(),
                                       target_transform,
                                       new_dest_root_target_clip_rect,
                                       dest_pass->transform_to_root_target,
                                       /*resolved_frame=*/nullptr);
        } else if (quad == quad_with_overlay_damage_index) {
          dest_shared_quad_state->overlay_damage_index = overlay_damage_index;
        }

        last_copied_source_shared_quad_state = quad->shared_quad_state;
      }

      DrawQuad* dest_quad = nullptr;
      if (const auto* pass_quad =
              quad->DynamicCast<CompositorRenderPassDrawQuad>()) {
        CompositorRenderPassId original_pass_id = pass_quad->render_pass_id;
        AggregatedRenderPassId remapped_pass_id =
            resolved_frame.GetRenderPassDataById(original_pass_id)
                .remapped_id();

        dest_quad = dest_pass->CopyFromAndAppendRenderPassDrawQuad(
            pass_quad, remapped_pass_id);

        if (needs_surface_damage_rect_list_ &&
            resolved_pass.aggregation().will_draw) {
          AddRenderPassFilterDamageToDamageList(
              resolved_frame, pass_quad, target_transform,
              new_dest_root_target_clip_rect,
              dest_pass->transform_to_root_target);
        }
      } else if (const auto* texture_quad =
                     quad->DynamicCast<TextureDrawQuad>()) {
        if (texture_quad->secure_output_only &&
            (!output_is_secure_ ||
             resolved_pass.aggregation().in_copy_request_pass)) {
          // If TextureDrawQuad requires secure output and the output is not
          // secure then replace it with solid black.
          auto* solid_color_quad =
              dest_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
          solid_color_quad->SetNew(dest_pass->shared_quad_state_list.back(),
                                   quad->rect, quad->visible_rect,
                                   SkColors::kBlack, false);
        } else {
          dest_quad = dest_pass->CopyFromAndAppendDrawQuad(quad);
        }
      } else {
        dest_quad = dest_pass->CopyFromAndAppendDrawQuad(quad);
      }
      if (dest_quad) {
        dest_quad->resources = quad_data.remapped_resources;
      }
    }
  }
}

void SurfaceAggregator::CopyPasses(ResolvedFrameData& resolved_frame) {
  Surface* surface = resolved_frame.surface();

  // The root surface is allowed to have copy output requests, so grab them
  // off its render passes. This map contains a set of CopyOutputRequests
  // keyed by each RenderPass id.
  Surface::CopyRequestsMap copy_requests;
  if (take_copy_requests_)
    surface->TakeCopyOutputRequests(&copy_requests);

  if (!resolved_frame.is_valid()) {
    return;
  }

  ++stats_->copied_surface_count;

  const gfx::Transform surface_transform =
      IsRootSurface(surface) ? root_surface_transform_ : gfx::Transform();

  auto& root_resolved_pass = resolved_frame.GetRootRenderPassData();
  gfx::Rect root_output_rect =
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          surface_transform, root_resolved_pass.render_pass().output_rect);

  const auto& frame_metadata = resolved_frame.GetMetadata();
  if (frame_metadata.delegated_ink_metadata) {
    // Copy delegated ink metadata from the compositor frame metadata. This
    // prevents the delegated ink trail from flickering if a compositor frame
    // is not generated due to a delayed main frame.
    TransformAndStoreDelegatedInkMetadata(
        root_resolved_pass.render_pass().transform_to_root_target *
            surface_transform,
        frame_metadata.delegated_ink_metadata.get(),
        resolved_frame.GetRootRenderPassData().remapped_id());
  }

  bool apply_surface_transform_to_root_pass = true;
  for (auto& resolved_pass : resolved_frame.GetResolvedPasses()) {
    const auto& source = resolved_pass.render_pass();

    size_t sqs_size = source.shared_quad_state_list.size();
    size_t dq_size = source.quad_list.size();
    auto copy_pass = std::make_unique<AggregatedRenderPass>(sqs_size, dq_size);

    MoveMatchingRequests(source.id, &copy_requests, &copy_pass->copy_requests);

    // We add an additional render pass for the transform if the root render
    // pass has any copy requests.
    apply_surface_transform_to_root_pass =
        resolved_pass.is_root() &&
        (copy_pass->copy_requests.empty() || surface_transform.IsIdentity());

    gfx::Rect output_rect = source.output_rect;
    gfx::Transform transform_to_root_target = source.transform_to_root_target;
    if (apply_surface_transform_to_root_pass) {
      // If we don't need an additional render pass to apply the surface
      // transform, adjust the root pass's rects to account for it.
      output_rect = root_output_rect;
    } else {
      // For the non-root render passes, the transform to root target needs to
      // be adjusted to include the root surface transform. This is also true if
      // we will be adding another render pass for the surface transform, in
      // which this will no longer be the root.
      transform_to_root_target =
          surface_transform * source.transform_to_root_target;
    }

    copy_pass->SetAll(
        resolved_pass.remapped_id(), output_rect, output_rect,
        transform_to_root_target, source.filters, source.backdrop_filters,
        source.backdrop_filter_bounds, root_content_color_usage_,
        source.has_transparent_background, source.cache_render_pass,
        resolved_pass.aggregation().has_damage, source.generate_mipmap);

    UpdatePersistentPassDataMergeState(resolved_pass, copy_pass.get(),
                                       /*is_merged_pass=*/false);

    if (needs_surface_damage_rect_list_ && resolved_pass.is_root()) {
      AddSurfaceDamageToDamageList(
          /*default_damage_rect=*/gfx::Rect(),
          /*parent_target_transform=*/surface_transform,
          /*dest_root_target_clip_rect=*/{},
          copy_pass->transform_to_root_target, &resolved_frame);
    }

    CopyQuadsToPass(resolved_frame, resolved_pass, copy_pass.get(),
                    resolved_frame.device_scale_factor(),
                    apply_surface_transform_to_root_pass ? surface_transform
                                                         : gfx::Transform(),
                    {}, /*dest_root_target_clip_rect*/ root_output_rect,
                    MaskFilterInfoExt());

    SetRenderPassDamageRect(copy_pass.get(), resolved_pass);

    dest_pass_list_->push_back(std::move(copy_pass));
  }

  dest_pass_list_->back()->video_capture_enabled =
      surface->IsVideoCaptureOnFromClient();

  if (!apply_surface_transform_to_root_pass)
    AddDisplayTransformPass();
}

void SurfaceAggregator::SetRenderPassDamageRect(
    AggregatedRenderPass* copy_pass,
    ResolvedPassData& resolved_pass) {
  // If the render pass has copy requests, or should be cached, or has
  // moving-pixel filters, or in a moving-pixel surface, we should damage the
  // whole output rect so that we always drawn the full content. Otherwise, we
  // might have incompleted copy request, or cached patially drawn render
  // pass.

  if (!RenderPassNeedsFullDamage(resolved_pass)) {
    gfx::Transform inverse_transform;
    if (copy_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
      gfx::Rect damage_rect_in_render_pass_space =
          cc::MathUtil::ProjectEnclosingClippedRect(inverse_transform,
                                                    root_damage_rect_);
      copy_pass->damage_rect.Intersect(damage_rect_in_render_pass_space);

      if (metrics_subsampler_.ShouldSample(0.001)) {
        gfx::Rect root_clip_in_render_pass_space =
            cc::MathUtil::ProjectEnclosingClippedRect(
                inverse_transform,
                resolved_pass.previous_persistent_data().parent_clip_rect);

        // The 'root_clip_in_render_pass_space' will now be a subrect of the
        // 'output rect'.
        root_clip_in_render_pass_space.Intersect(copy_pass->output_rect);

        bool is_output_rect =
            root_clip_in_render_pass_space == copy_pass->output_rect;

        UMA_HISTOGRAM_ENUMERATION(
            "Compositing.SurfaceAggregator.RenderPassDamageType",
            is_output_rect ? RenderPassDamage::kOutputRect
                           : RenderPassDamage::kRootClipped);

        const auto render_pass_overdamage =
            copy_pass->output_rect.size().Area64() -
            root_clip_in_render_pass_space.size().Area64();

        UMA_HISTOGRAM_COUNTS_10M(
            "Compositing.SurfaceAggregator.ExcessPixelsClipped",
            render_pass_overdamage);
      }
    }

    // For unembeded render passes, their damages were not added to the
    // root render pass. Add back the original damage from cc so it can be
    // skipped later when there is no internal damage.
    static const bool can_skip_render_pass = base::FeatureList::IsEnabled(
        features::kAllowUndamagedNonrootRenderPassToSkip);
    if (resolved_pass.IsUnembedded() && can_skip_render_pass) {
      copy_pass->damage_rect.Union(resolved_pass.aggregation().added_damage);
    }
  } else if (metrics_subsampler_.ShouldSample(0.001)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Compositing.SurfaceAggregator.RenderPassDamageType",
        RenderPassDamage::kForceFullOutputRect);
  }
}

void SurfaceAggregator::ProcessAddedAndRemovedSurfaces() {
  // Delete resolved frame data that wasn't used this aggregation. This releases
  // resources associated with those resolved frames.
  std::erase_if(resolved_frames_, [](auto& entry) {
    return !entry.second.WasUsedInAggregation();
  });
}

gfx::Rect SurfaceAggregator::PrewalkRenderPass(
    ResolvedFrameData& resolved_frame,
    ResolvedPassData& resolved_pass,
    const gfx::Rect& damage_from_parent,
    const gfx::Transform& target_to_root_transform,
    const ResolvedPassData* parent_pass,
    PrewalkResult& result) {
  const CompositorRenderPass& render_pass = resolved_pass.render_pass();

  if (render_pass.backdrop_filters.HasFilterThatMovesPixels()) {
    has_pixel_moving_backdrop_filter_ = true;
  }

  if (parent_pass && parent_pass->aggregation().will_draw)
    resolved_pass.aggregation().will_draw = true;

  // Populate state for about cached render passes and pixel moving filters.
  // These attributes apply transitively to all child render passes embedded by
  // the CompositorRenderPass with the attribute.
  if (render_pass.cache_render_pass ||
      (parent_pass && parent_pass->aggregation().in_cached_render_pass)) {
    resolved_pass.aggregation().in_cached_render_pass = true;
  }

  if (render_pass.filters.HasFilterThatMovesPixels() ||
      (parent_pass && parent_pass->aggregation().in_pixel_moving_filter_pass)) {
    resolved_pass.aggregation().in_pixel_moving_filter_pass = true;
    stats_->has_pixel_moving_filter = true;
  }

  const FrameDamageType damage_type = resolved_frame.GetFrameDamageType();
  if (damage_type == FrameDamageType::kFull) {
    resolved_pass.aggregation().has_damage = true;
  } else if (damage_type == FrameDamageType::kFrame &&
             render_pass.has_damage_from_contributing_content) {
    resolved_pass.aggregation().has_damage = true;
  }

  // The damage on the root render pass of the surface comes from damage
  // accumulated from all quads in the surface, and needs to be expanded by any
  // pixel-moving backdrop filter in the render pass if intersecting. Transform
  // this damage into the local space of the render pass for this purpose.
  // TODO(kylechar): If this render pass isn't reachable from the surfaces root
  // render pass then surface damage can't be transformed into this render pass
  // coordinate space. We should use the actual damage for the render pass,
  // which isn't included in the CompositorFrame right now.
  gfx::Rect surface_root_rp_damage = resolved_frame.GetSurfaceDamage();
  if (!surface_root_rp_damage.IsEmpty()) {
    gfx::Transform root_to_target_transform;
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
  for (const DrawQuad* quad : base::Reversed(render_pass.quad_list)) {
    gfx::Rect quad_damage_rect;
    gfx::Rect quad_target_space_damage_rect;
    if (quad->material == DrawQuad::Material::kSurfaceContent) {
      const auto* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
      ResolvedFrameData* child_resolved_frame =
          GetResolvedFrame(surface_quad->surface_range);

      // If the primary surface is not available then we assume the damage is
      // the full size of the SurfaceDrawQuad because we might need to introduce
      // gutter.
      if (!child_resolved_frame || child_resolved_frame->surface_id() !=
                                       surface_quad->surface_range.end()) {
        quad_damage_rect = quad->rect;
      }

      if (child_resolved_frame) {
        float x_scale = SK_Scalar1;
        float y_scale = SK_Scalar1;
        if (surface_quad->stretch_content_to_fill_bounds) {
          const gfx::Size& child_size = child_resolved_frame->size_in_pixels();
          if (!child_size.IsEmpty()) {
            x_scale = static_cast<float>(surface_quad->rect.width()) /
                      child_size.width();
            y_scale = static_cast<float>(surface_quad->rect.height()) /
                      child_size.height();
          }
        } else {
          // If not stretching to fit bounds then scale to adjust to device
          // scale factor differences between child and parent surface. This
          // scale factor is later applied to quads in the aggregated frame.
          x_scale = y_scale = resolved_frame.device_scale_factor() /
                              child_resolved_frame->device_scale_factor();
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
            gfx::Transform inverse =
                quad->shared_quad_state->quad_to_target_transform
                    .GetCheckedInverse();
            inverse.PostScale(SK_Scalar1 / x_scale, SK_Scalar1 / y_scale);
            accumulated_damage_in_child_space =
                cc::MathUtil::ProjectEnclosingClippedRect(
                    inverse, accumulated_damage_in_child_space);
          }
        }
        gfx::Rect child_rect =
            PrewalkSurface(*child_resolved_frame, &resolved_pass,
                           accumulated_damage_in_child_space, result);
        child_rect = gfx::ScaleToEnclosingRect(child_rect, x_scale, y_scale);
        quad_damage_rect.Union(child_rect);
      }

      // Only check for root render pass on the root surface.
      if (parent_pass == nullptr && resolved_pass.is_root() &&
          !result.page_fullscreen_mode) {
        gfx::Rect surface_quad_on_target_space = ClippedQuadRectangle(quad);
        // Often time the surface_quad_on_target_space is not exactly the same
        // as the output_rect after the math operations, although they are meant
        // to be the same. Set the delta tolerance to 8 pixels.
        if (surface_quad_on_target_space.ApproximatelyEqual(
                render_pass.output_rect, /*tolerance=*/8)) {
          result.page_fullscreen_mode = true;
        }
      }

#if BUILDFLAG(IS_WIN)
      // Force the root passes of surfaces referenced by the root pass of the
      // root surface to be embedded instead of merged. This supports the
      // feature |kDelegatedCompositingLimitToUi|.
      if (prevent_merging_surfaces_to_root_pass_ && child_resolved_frame &&
          resolved_pass.is_root() && IsRootSurface(resolved_frame.surface())) {
        child_resolved_frame->GetRootRenderPassData()
            .aggregation()
            .prevent_merge = true;
      }
#endif
    } else if (auto* render_pass_quad =
                   quad->DynamicCast<CompositorRenderPassDrawQuad>()) {
      CompositorRenderPassId child_pass_id = render_pass_quad->render_pass_id;

      ResolvedPassData& child_resolved_pass =
          resolved_frame.GetRenderPassDataById(child_pass_id);
      const CompositorRenderPass& child_render_pass =
          child_resolved_pass.render_pass();

      gfx::Rect rect_in_target_space = cc::MathUtil::MapEnclosingClippedRect(
          quad->shared_quad_state->quad_to_target_transform, quad->rect);

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
      // TODO(crbug.com/40244221): Work out how to correctly compute damage when
      // offset backdrop filters may be involved.

      // For the pixel-moving foreground filters, all effects can be expanded
      // outside the RenderPassDrawQuad rect based on filter pixel movement.
      // Therefore, we have to check if the expanded rpdq->rect intersects the
      // damage under it. Then we extend the damage rect to include the expanded
      // rpdq->rect.

      // Expand the damage to cover entire |output_rect| if the |render_pass|
      // has pixel-moving foreground filter.
      if (child_render_pass.filters.HasFilterThatMovesPixels()) {
        gfx::Rect expanded_rect_in_target_space =
            GetExpandedRectWithPixelMovingForegroundFilter(
                *render_pass_quad, child_render_pass.filters);

        if (expanded_rect_in_target_space.Intersects(damage_rect) ||
            expanded_rect_in_target_space.Intersects(damage_from_parent) ||
            expanded_rect_in_target_space.Intersects(surface_root_rp_damage)) {
          damage_rect.Union(expanded_rect_in_target_space);
        }
      }

      resolved_pass.aggregation().embedded_passes.insert(&child_resolved_pass);

      const gfx::Transform child_to_root_transform =
          target_to_root_transform *
          quad->shared_quad_state->quad_to_target_transform;
      quad_damage_rect =
          PrewalkRenderPass(resolved_frame, child_resolved_pass, gfx::Rect(),
                            child_to_root_transform, &resolved_pass, result);

    } else {
      // If this the next frame in sequence from last aggregation then per quad
      // damage_rects are valid so add them here. If not, either this is the
      // same frame as last aggregation and there is no damage OR there is
      // already full damage for the surface.
      if (damage_type == FrameDamageType::kFrame) {
        if (auto& per_quad_damage_rect = GetOptionalDamageRectFromQuad(quad)) {
          // The DrawQuad `per_quad_damage_rect` is already in the render pass
          // coordinate space instead of quad rect coordinate space.
          quad_target_space_damage_rect = per_quad_damage_rect.value();
        }
      }
    }

    // Clip the quad damage to the quad visible before converting back to
    // render pass coordinate space. Expanded damage outside the quad rect for
    // filters are added to |damage_rect| directly so this only clips damage
    // from drawing the quad itself.
    quad_damage_rect.Intersect(quad->visible_rect);

    if (!quad_damage_rect.IsEmpty()) {
      // Convert the quad damage rect into its target space and clip it if
      // needed. Ignore tiny errors to avoid artificially inflating the
      // damage due to floating point math.
      constexpr float kEpsilon = 0.001f;
      quad_target_space_damage_rect =
          cc::MathUtil::MapEnclosingClippedRectIgnoringError(
              quad->shared_quad_state->quad_to_target_transform,
              quad_damage_rect, kEpsilon);
    }

    if (!quad_target_space_damage_rect.IsEmpty()) {
      if (quad->shared_quad_state->clip_rect) {
        quad_target_space_damage_rect.Intersect(
            *quad->shared_quad_state->clip_rect);
      }
      damage_rect.Union(quad_target_space_damage_rect);
    }
  }

  if (!damage_rect.IsEmpty()) {
    // There is extra damage for this render pass. This is damage that the
    // client that submitted this render pass didn't know about and isn't
    // included in the surface damage or `has_damage_from_contributing_content`.
    resolved_pass.aggregation().has_damage = true;

    if (render_pass.filters.HasFilterThatMovesPixels()) {
      // Expand the damage to cover entire |output_rect| if the |render_pass|
      // has pixel-moving foreground filter.
      damage_rect.Union(render_pass.output_rect);
    }

    // The added damage from quads in the render pass is transformed back into
    // the render pass coordinate space without clipping, so it can extend
    // beyond the edge of the current render pass. Coordinates outside the
    // output_rect are invalid in this render passes coordinate space but they
    // may be valid coordinates in the embedder coordinate space, causing
    // unnecessary damage expansion.
    damage_rect.Intersect(render_pass.output_rect);

    resolved_pass.aggregation().added_damage.Union(damage_rect);
  }

  return damage_rect;
}

bool SurfaceAggregator::CheckFrameSinksChanged(const SurfaceId& surface_id) {
  contained_surfaces_.insert(surface_id);
  LocalSurfaceId& local_surface_id =
      contained_frame_sinks_[surface_id.frame_sink_id()];
  bool frame_sinks_changed =
      (!previous_contained_frame_sinks_.contains(surface_id.frame_sink_id()));
  local_surface_id = std::max(surface_id.local_surface_id(), local_surface_id);
  return frame_sinks_changed;
}

gfx::Rect SurfaceAggregator::PrewalkSurface(ResolvedFrameData& resolved_frame,
                                            ResolvedPassData* parent_pass,
                                            const gfx::Rect& damage_from_parent,
                                            PrewalkResult& result) {
  Surface* surface = resolved_frame.surface();
  DCHECK(surface->HasActiveFrame());

  if (referenced_surfaces_.count(surface->surface_id()))
    return gfx::Rect();

  result.frame_sinks_changed |=
      CheckFrameSinksChanged(resolved_frame.surface_id());

  if (!resolved_frame.is_valid())
    return gfx::Rect();

  DebugLogSurface(surface, resolved_frame.WillDraw());
  ++stats_->prewalked_surface_count;

  auto& root_resolved_pass = resolved_frame.GetRootRenderPassData();
  if (parent_pass) {
    parent_pass->aggregation().embedded_passes.insert(&root_resolved_pass);
  }

  gfx::Rect damage_rect = resolved_frame.GetSurfaceDamage();

  // Avoid infinite recursion by adding current surface to
  // |referenced_surfaces_|.
  referenced_surfaces_.insert(surface->surface_id());

  for (auto& resolved_pass : resolved_frame.GetResolvedPasses()) {
    // Prewalk any render passes that aren't reachable from the root pass. The
    // damage produced isn't correct since there is no transform between damage
    // in the root render passes coordinate space and the unembedded render
    // pass, but other attributes related to the embedding hierarchy are still
    // important to propagate.
    if (resolved_pass.IsUnembedded()) {
      stats_->has_unembedded_pass = true;
      resolved_pass.aggregation().added_damage =
          PrewalkRenderPass(resolved_frame, resolved_pass,
                            /*damage_from_parent=*/gfx::Rect(),
                            /*target_to_root_transform=*/gfx::Transform(),
                            /*parent_pass=*/nullptr, result);
    }
  }

  damage_rect.Union(PrewalkRenderPass(resolved_frame, root_resolved_pass,
                                      damage_from_parent, gfx::Transform(),
                                      parent_pass, result));

  if (!damage_rect.IsEmpty()) {
    auto damage_rect_surface_space = damage_rect;
    if (IsRootSurface(surface)) {
      // The damage reported to the surface is in pre-display transform space
      // since it is used by clients which are not aware of the display
      // transform.
      damage_rect = cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          root_surface_transform_, damage_rect);
      gfx::Transform inverse = root_surface_transform_.GetCheckedInverse();
      damage_rect_surface_space =
          cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(inverse,
                                                                  damage_rect);
    }

    // The following call can cause one or more copy requests to be added to the
    // Surface. Therefore, no code before this point should have assumed
    // anything about the presence or absence of copy requests after this point.
    surface->NotifyAggregatedDamage(damage_rect_surface_space,
                                    expected_display_time_);
  }

  // If any CopyOutputRequests were made at FrameSink level, make sure we grab
  // them too.
  surface->TakeCopyOutputRequestsFromClient();

  if (root_resolved_pass.aggregation().will_draw)
    surface->OnWillBeDrawn();

  const auto& frame_metadata = resolved_frame.GetMetadata();
  for (const SurfaceRange& surface_range : frame_metadata.referenced_surfaces) {
    damage_ranges_[surface_range.end().frame_sink_id()].push_back(
        surface_range);
    if (surface_range.HasDifferentFrameSinkIds()) {
      damage_ranges_[surface_range.start()->frame_sink_id()].push_back(
          surface_range);
    }
  }

  for (const SurfaceId& surface_id : surface->active_referenced_surfaces()) {
    // Referenced surfaces that haven't been prewalked yet are not embedded so
    // don't contribute any pixels to the display. They will only be drawn if
    // necessary to fulfill CopyOutputRequests.
    if (!contained_surfaces_.count(surface_id)) {
      result.undrawn_surfaces.insert(surface_id);
      ResolvedFrameData* undrawn_surface = GetResolvedFrame(surface_id);
      if (undrawn_surface) {
        PrewalkSurface(*undrawn_surface, /*parent_pass=*/nullptr, gfx::Rect(),
                       result);
      }
    }
  }

  for (auto& resolved_pass : resolved_frame.GetResolvedPasses()) {
    auto& render_pass = resolved_pass.render_pass();

    // Checking for copy requests need to be done after the prewalk because
    // copy requests can get added after damage is computed.
    if (!render_pass.copy_requests.empty()) {
      has_copy_requests_ = true;
      MarkAndPropagateCopyRequestPasses(resolved_pass);
    }
  }

  referenced_surfaces_.erase(surface->surface_id());
  result.content_color_usage =
      std::max(result.content_color_usage, frame_metadata.content_color_usage);

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
    ResolvedFrameData* resolved_frame = GetResolvedFrame(surface_id);
    if (!resolved_frame)
      continue;

    Surface* surface = resolved_frame->surface();
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
      CopyPasses(*resolved_frame);
      referenced_surfaces_.erase(surface_id);
    }
  }
}

void SurfaceAggregator::MarkAndPropagateCopyRequestPasses(
    ResolvedPassData& resolved_pass) {
  if (resolved_pass.aggregation().in_copy_request_pass)
    return;

  resolved_pass.aggregation().in_copy_request_pass = true;
  for (ResolvedPassData* child_pass :
       resolved_pass.aggregation().embedded_passes) {
    MarkAndPropagateCopyRequestPasses(*child_pass);
  }
}

AggregatedFrame SurfaceAggregator::Aggregate(
    const SurfaceId& surface_id,
    base::TimeTicks expected_display_time,
    gfx::OverlayTransform display_transform,
    const gfx::Rect& target_damage,
    int64_t display_trace_id) {
  DCHECK(!expected_display_time.is_null());
  DCHECK(contained_surfaces_.empty());

  DCHECK(!is_inside_aggregate_);
  is_inside_aggregate_ = true;

  root_surface_id_ = surface_id;

  // Start recording new stats for this aggregation.
  stats_.emplace();

  base::ElapsedTimer prewalk_timer;
  ResolvedFrameData* resolved_frame = GetResolvedFrame(surface_id);

  if (!resolved_frame || !resolved_frame->is_valid()) {
    ResetAfterAggregate();
    return {};
  }

  display_trace_id_ = display_trace_id;
  expected_display_time_ = expected_display_time;

  const CompositorFrameMetadata& frame_metadata = resolved_frame->GetMetadata();
  flow_ids_for_resolved_frames_.insert(frame_metadata.begin_frame_ack.trace_id);

  TRACE_EVENT_BEGIN(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(display_trace_id_),
      [this](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_SURFACE_AGGREGATION);
        data->set_display_trace_id(display_trace_id_);
      });

  // We need to terminate the above trace event separately so that the callees
  // of `SurfaceAggregator::Aggregate` can appropriately populate
  // `flow_ids_for_resolved_frames_`, which we need so that we can
  // terminate the flows for those frames at this trace event.
  absl::Cleanup surface_aggregation_trace_event_scoped_exit = [this] {
    TRACE_EVENT_END(
        "viz,benchmark,graphics.pipeline", [this](perfetto::EventContext ctx) {
          auto* chrome_graphics_pipeline =
              ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                  ->set_chrome_graphics_pipeline();
          // Two separate loops are necessary due to Perfetto's ProtoZero
          // semantics: if we start adding values to a repeated field, we should
          // add all values that need to be added, before moving on to updating
          // a different field.
          for (int64_t id : flow_ids_for_resolved_frames_) {
            chrome_graphics_pipeline->add_aggregated_frames_ids(id);
          }
          for (int64_t id : flow_ids_for_resolved_frames_) {
            ctx.event()->add_terminating_flow_ids(id);
          }
        });
    // Clear this separately from `ResetAfterAggregate` since this
    // `absl::Cleanup` is run after `ResetAfterAggregate`.
    flow_ids_for_resolved_frames_.clear();
  };

  CheckFrameSinksChanged(resolved_frame->surface_id());

  AggregatedFrame frame;
  dest_pass_list_ = &frame.render_pass_list;
  surface_damage_rect_list_ = &frame.surface_damage_rect_list_;

  auto& root_render_pass =
      resolved_frame->GetRootRenderPassData().render_pass();

  // The root render pass on the root surface can not have backdrop filters.
  DCHECK(!root_render_pass.backdrop_filters.HasFilterThatMovesPixels());

  const gfx::Size viewport_bounds = resolved_frame->size_in_pixels();
  root_surface_transform_ = gfx::OverlayTransformToTransform(
      display_transform, gfx::SizeF(viewport_bounds));

  // Reset state that couldn't be reset in ResetAfterAggregate().
  damage_ranges_.clear();

  DCHECK(referenced_surfaces_.empty());

  // The root surface root render pass is the start of the embedding tree.
  resolved_frame->GetRootRenderPassData().aggregation().will_draw = true;

  PrewalkResult prewalk_result;
  gfx::Rect prewalk_damage_rect =
      PrewalkSurface(*resolved_frame,
                     /*parent_pass=*/nullptr,
                     /*damage_from_parent=*/gfx::Rect(), prewalk_result);
  stats_->prewalk_time = prewalk_timer.Elapsed();

  root_damage_rect_ = prewalk_damage_rect;
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
        root_surface_transform_, gfx::Rect(viewport_bounds));
    root_content_color_usage_ = prewalk_result.content_color_usage;
  }

  if (prewalk_result.frame_sinks_changed)
    manager_->AggregatedFrameSinksChanged();

  frame.has_copy_requests = has_copy_requests_ && take_copy_requests_;
  frame.content_color_usage = prewalk_result.content_color_usage;
  frame.page_fullscreen_mode = prewalk_result.page_fullscreen_mode;

  base::ElapsedTimer copy_timer;
  CopyUndrawnSurfaces(&prewalk_result);
  referenced_surfaces_.insert(surface_id);
  CopyPasses(*resolved_frame);
  referenced_surfaces_.erase(surface_id);
  DCHECK(referenced_surfaces_.empty());
  stats_->copy_time = copy_timer.Elapsed();

  RecordStatHistograms();

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
      !RenderPassNeedsFullDamage(resolved_frame->GetRootRenderPassData())) {
    last_pass->damage_rect.Intersect(prewalk_damage_rect);
  }

  if (!base::FeatureList::IsEnabled(features::kColorConversionInRenderer)) {
    AddColorConversionPass();
  }
  AddRootReadbackPass();

  ProcessAddedAndRemovedSurfaces();
  contained_surfaces_.swap(previous_contained_surfaces_);
  contained_frame_sinks_.swap(previous_contained_frame_sinks_);

  ResetAfterAggregate();

  for (auto& contained_surface_id : previous_contained_surfaces_) {
    auto* surface = manager_->GetSurfaceForId(contained_surface_id);
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
    // If the aggregated frame is getting a metadata that matches the one it
    // received last frame, increment the counter. Once the limit of frames
    // with the same metadata `kMaxFramesWithIdenticalInkMetadata` is
    // reached, the metadata is no longer attached. This prevents the
    // delegated ink trail from persisting on the screen if no new
    // compositor frames are received by Viz. The purpose of this hysteresis
    // is to prevent flickering in the case where the compositor frame is
    // delayed due to a late main frame in the renderer process.
    if (previous_ink_metadata_time_ == delegated_ink_metadata_->timestamp()) {
      identical_ink_metadata_count_++;
    } else {
      identical_ink_metadata_count_ = 0;
    }
    if (identical_ink_metadata_count_ < kMaxFramesWithIdenticalInkMetadata) {
      previous_ink_metadata_time_ = delegated_ink_metadata_->timestamp();
      frame.delegated_ink_metadata = std::move(delegated_ink_metadata_);
      last_frame_had_delegated_ink_ = true;
    } else {
      last_frame_had_delegated_ink_ = false;
    }
  } else {
    last_frame_had_delegated_ink_ = false;
  }

  if (frame_annotator_)
    frame_annotator_->AnnotateAggregatedFrame(&frame);

  return frame;
}

void SurfaceAggregator::RecordStatHistograms() {
  UMA_HISTOGRAM_COUNTS_100(
      "Compositing.SurfaceAggregator.PrewalkedSurfaceCount",
      stats_->prewalked_surface_count);
  UMA_HISTOGRAM_COUNTS_100("Compositing.SurfaceAggregator.CopiedSurfaceCount",
                           stats_->copied_surface_count);

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Compositing.SurfaceAggregator.PrewalkUs", stats_->prewalk_time,
      kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Compositing.SurfaceAggregator.CopyUs", stats_->copy_time,
      kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);

  UMA_HISTOGRAM_BOOLEAN("Compositing.SurfaceAggregator.HasCopyRequestsPerFrame",
                        has_copy_requests_);
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.SurfaceAggregator.HasPixelMovingFiltersPerFrame",
      stats_->has_pixel_moving_filter);
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.SurfaceAggregator.HasPixelMovingBackdropFiltersPerFrame",
      has_pixel_moving_backdrop_filter_);
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.SurfaceAggregator.HasUnembeddedRenderPassesPerFrame",
      stats_->has_unembedded_pass);

  stats_.reset();
}

void SurfaceAggregator::ResetAfterAggregate() {
  DCHECK(is_inside_aggregate_);

  is_inside_aggregate_ = false;
  dest_pass_list_ = nullptr;
  surface_damage_rect_list_ = nullptr;
  current_zero_damage_rect_is_not_recorded_ = false;
  expected_display_time_ = base::TimeTicks();
  display_trace_id_ = -1;
  has_pixel_moving_backdrop_filter_ = false;
  has_copy_requests_ = false;
  resolved_surface_ranges_.clear();
  contained_surfaces_.clear();
  contained_frame_sinks_.clear();

  // Reset resolved frame data from this aggregation.
  for (auto& [surface_id, resolved_frame] : resolved_frames_)
    resolved_frame.ResetAfterAggregation();
}

void SurfaceAggregator::SetFullDamageForSurface(const SurfaceId& surface_id) {
  auto iter = resolved_frames_.find(surface_id);
  if (iter != resolved_frames_.end())
    iter->second.SetFullDamageForNextAggregation();
}

void SurfaceAggregator::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  display_color_spaces_ = display_color_spaces;
}

void SurfaceAggregator::SetMaxRenderTargetSize(int max_size) {
  DCHECK_GE(max_size, 0);
  max_render_target_size_ = max_size;
}

bool SurfaceAggregator::CheckForDisplayDamage(const SurfaceId& surface_id) {
  auto it = damage_ranges_.find(surface_id.frame_sink_id());
  if (it == damage_ranges_.end()) {
    return false;
  }

  for (const SurfaceRange& surface_range : it->second) {
    if (surface_range.IsInRangeInclusive(surface_id)) {
      return true;
    }
  }

  return false;
}

bool SurfaceAggregator::ForceReleaseResourcesIfNeeded(
    const SurfaceId& surface_id) {
  auto iter = resolved_frames_.find(surface_id);
  if (iter != resolved_frames_.end()) {
    auto& resolved_frame = iter->second;
    DCHECK(resolved_frame.surface()->HasActiveFrame());
    if (resolved_frame.surface()->GetActiveFrame().resource_list.empty()) {
      // When a client submits a CompositorFrame without resources it's
      // typically done to force return of existing resources to the client.
      resolved_frame.ForceReleaseResource();
    }
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
    const gfx::DelegatedInkMetadata* metadata,
    const AggregatedRenderPassId render_pass_with_delegated_ink) {
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

  gfx::PointF point =
      parent_quad_to_root_target_transform.MapPoint(metadata->point());
  gfx::RectF area = parent_quad_to_root_target_transform.MapRect(
      metadata->presentation_area());
  delegated_ink_metadata_ = std::make_unique<gfx::DelegatedInkMetadata>(
      point, metadata->diameter(), metadata->color(), metadata->timestamp(),
      area, metadata->frame_time(), metadata->is_hovering(),
      render_pass_with_delegated_ink.GetUnsafeValue());

  TRACE_EVENT_INSTANT2(
      "viz", "SurfaceAggregator::TransformAndStoreDelegatedInkMetadata",
      TRACE_EVENT_SCOPE_THREAD, "original metadata", metadata->ToString(),
      "transformed metadata", delegated_ink_metadata_->ToString());
}

void SurfaceAggregator::DebugLogSurface(const Surface* surface,
                                        bool will_draw) {
  DBG_LOG("aggregator.surface.log", "D%d - %s, %s draws=%s",
          static_cast<int>(referenced_surfaces_.size()),
          surface->surface_id().ToString().c_str(),
          surface->size_in_pixels().ToString().c_str(),
          will_draw ? "true" : "false");
}

}  // namespace viz
