// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/occlusion_culler.h"

#include <algorithm>

#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
namespace {

// SkRegion uses INT_MAX as a sentinel. Reduce gfx::Rect values when they are
// equal to INT_MAX to prevent conversion to an empty region.
gfx::Rect SafeConvertRectForRegion(const gfx::Rect& r) {
  gfx::Rect safe_rect(r);
  if (safe_rect.x() == INT_MAX) {
    safe_rect.set_x(INT_MAX - 1);
  }
  if (safe_rect.y() == INT_MAX) {
    safe_rect.set_y(INT_MAX - 1);
  }
  if (safe_rect.width() == INT_MAX) {
    safe_rect.set_width(INT_MAX - 1);
  }
  if (safe_rect.height() == INT_MAX) {
    safe_rect.set_height(INT_MAX - 1);
  }
  return safe_rect;
}

// Decides whether or not a DrawQuad should be split into a more complex visible
// region in order to avoid overdraw.
bool CanSplitQuad(const DrawQuad::Material quad_material,
                  const std::vector<gfx::Rect>& visible_region_rects,
                  const gfx::Size& visible_region_bounding_size,
                  int minimum_fragments_reduced,
                  const float device_scale_factor) {
  static constexpr DrawQuad::Material kNonSplittableMaterials[] = {
      // Exclude debug quads from quad splitting.
      DrawQuad::Material::kDebugBorder,
      // Exclude possible overlay candidates from quad splitting
      // See `OverlayCandidate::FromDrawQuad()`.
      DrawQuad::Material::kTextureContent,
      DrawQuad::Material::kVideoHole,
  };

  if (base::Contains(kNonSplittableMaterials, quad_material)) {
    return false;
  }

  base::CheckedNumeric<int> area = 0;
  for (const auto& r : visible_region_rects) {
    area += r.size().GetCheckedArea();
    // In calculations below, assume false if this addition overflows.
    if (!area.IsValid()) {
      return false;
    }
  }

  base::CheckedNumeric<int> visible_region_bounding_area =
      visible_region_bounding_size.GetCheckedArea();
  if (!visible_region_bounding_area.IsValid()) {
    // In calculations below, assume true if this overflows.
    return true;
  }

  area = visible_region_bounding_area - area;
  if (!area.IsValid()) {
    // In calculations below, assume false if this subtraction underflows.
    return false;
  }

  const int int_area = area.ValueOrDie();
  return int_area * device_scale_factor * device_scale_factor >
         minimum_fragments_reduced;
}

// Returns the bounds for the largest rect that can be inscribed in a rounded
// rect.
gfx::RectF GetOccludingRectForRRectF(const gfx::RRectF& bounds) {
  if (bounds.IsEmpty()) {
    return gfx::RectF();
  }

  if (bounds.GetType() == gfx::RRectF::Type::kRect) {
    return bounds.rect();
  }

  gfx::RectF occluding_rect = bounds.rect();

  // Compute the radius for each corner
  const auto top_left = bounds.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft);
  const auto top_right =
      bounds.GetCornerRadii(gfx::RRectF::Corner::kUpperRight);
  const auto lower_right =
      bounds.GetCornerRadii(gfx::RRectF::Corner::kLowerRight);
  const auto lower_left =
      bounds.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft);

  // Get a bounding rect that does not intersect with the rounding clip.
  // When a rect has rounded corner with radius r, then the largest rect that
  // can be inscribed inside it has an inset of |((2 - sqrt(2)) / 2) * radius|.
  // Should you wish to convince yourself that sin(pi/4) is the max value check:
  // https://math.stackexchange.com/questions/240192/find-the-area-of-largest-rectangle-that-can-be-inscribed-in-an-ellipse
  constexpr float kInsetCoeficient = 0.3f;
  occluding_rect.Inset(gfx::InsetsF::TLBR(
      std::max(top_left.y(), top_right.y()) * kInsetCoeficient,
      std::max(top_left.x(), lower_left.x()) * kInsetCoeficient,
      std::max(lower_right.y(), lower_left.y()) * kInsetCoeficient,
      std::max(top_right.x(), lower_right.x()) * kInsetCoeficient));
  return occluding_rect;
}

// Attempts to consolidate rectangles that were only split because of the
// nature of base::Region and transforms the region into a list of visible
// rectangles. Returns true upon successful reduction of the region to under
// `complexity_limit`, false otherwise.
bool ReduceComplexity(const cc::Region& region,
                      size_t complexity_limit,
                      std::vector<gfx::Rect>& reduced_region) {
  reduced_region.clear();

  for (gfx::Rect r : region) {
    auto it = base::ranges::find_if(reduced_region, [&r](const gfx::Rect& a) {
      return a.SharesEdgeWith(r);
    });

    if (it != reduced_region.end()) {
      it->Union(r);
      continue;
    }

    reduced_region.push_back(r);

    if (reduced_region.size() >= complexity_limit) {
      return false;
    }
  }
  return true;
}

}  // namespace

OcclusionCuller::OcclusionCuller(
    OverlayProcessorInterface* overlay_processor,
    const RendererSettings::OcclusionCullerSettings& settings)
    : overlay_processor_(overlay_processor), settings_(settings) {}

OcclusionCuller::~OcclusionCuller() = default;

void OcclusionCuller::RemoveOverdrawQuads(AggregatedFrame* frame,
                                          float device_scale_factor) {
  if (frame->render_pass_list.empty()) {
    return;
  }

  base::flat_map<AggregatedRenderPassId, gfx::Rect> backdrop_filter_rects;
  for (const auto& pass : frame->render_pass_list) {
    if (!pass->backdrop_filters.IsEmpty() &&
        pass->backdrop_filters.HasFilterThatMovesPixels()) {
      backdrop_filter_rects[pass->id] = cc::MathUtil::MapEnclosingClippedRect(
          pass->transform_to_root_target, pass->output_rect);
    }
  }

  for (const auto& pass : frame->render_pass_list) {
    const SharedQuadState* last_sqs = nullptr;
    cc::Region occlusion_in_target_space;
    cc::Region backdrop_filters_in_target_space;
    bool current_sqs_intersects_occlusion = false;

    // TODO(yiyix): Add filter effects to draw occlusion calculation
    if (!pass->filters.IsEmpty() || !pass->backdrop_filters.IsEmpty()) {
      continue;
    }

    // When there is only one quad in the render pass, occlusion is not
    // possible.
    if (pass->quad_list.size() == 1) {
      continue;
    }

    auto quad_list_end = pass->quad_list.end();
    cc::Region occlusion_in_quad_content_space;
    gfx::Rect render_pass_quads_in_content_space;

    for (auto quad = pass->quad_list.begin(); quad != quad_list_end;) {
      // Sanity check: we should not have a Compositor
      // CompositorRenderPassDrawQuad here.
      DCHECK_NE(quad->material, DrawQuad::Material::kCompositorRenderPass);

      // Skip quad if it is a AggregatedRenderPassDrawQuad because it is a
      // special type of DrawQuad where the visible_rect of shared quad state is
      // not entirely covered by draw quads in it.
      if (auto* rpdq = quad->DynamicCast<AggregatedRenderPassDrawQuad>()) {
        // A RenderPass with backdrop filters may apply to a quad underlying
        // RenderPassQuad. These regions should be tracked so that correctly
        // handle splitting and occlusion of the underlying quad.
        auto it = backdrop_filter_rects.find(rpdq->render_pass_id);
        if (it != backdrop_filter_rects.end()) {
          backdrop_filters_in_target_space.Union(it->second);
        }

        ++quad;
        continue;
      }

      // Also skip quad if the DrawQuad is inside a 3d object.
      if (quad->shared_quad_state->sorting_context_id != 0) {
        ++quad;
        continue;
      }

      if (!last_sqs) {
        last_sqs = quad->shared_quad_state;
      }

      gfx::Transform transform =
          quad->shared_quad_state->quad_to_target_transform;

      // TODO(yiyix): Find a rect interior to each transformed quad.
      if (last_sqs != quad->shared_quad_state) {
        if (last_sqs->opacity == 1 && last_sqs->are_contents_opaque &&
            (last_sqs->blend_mode == SkBlendMode::kSrcOver ||
             last_sqs->blend_mode == SkBlendMode::kSrc) &&
            last_sqs->quad_to_target_transform
                .NonDegeneratePreserves2dAxisAlignment()) {
          gfx::Rect sqs_rect_in_target =
              cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                  last_sqs->quad_to_target_transform,
                  last_sqs->visible_quad_layer_rect);

          // If a rounded corner is being applied then the visible rect for the
          // sqs is actually even smaller. Reduce the rect size to get a
          // rounded corner adjusted occluding region.
          if (last_sqs->mask_filter_info.HasRoundedCorners()) {
            sqs_rect_in_target.Intersect(
                gfx::ToEnclosedRect(GetOccludingRectForRRectF(
                    last_sqs->mask_filter_info.rounded_corner_bounds())));
          }

          if (last_sqs->clip_rect) {
            sqs_rect_in_target.Intersect(*last_sqs->clip_rect);
          }

          // If region complexity is above our threshold, remove the smallest
          // rects from occlusion region.
          occlusion_in_target_space.Union(sqs_rect_in_target);
          while (occlusion_in_target_space.GetRegionComplexity() >
                 settings_.maximum_occluder_complexity) {
            gfx::Rect smallest_rect = *occlusion_in_target_space.begin();
            for (auto occluding_rect : occlusion_in_target_space) {
              if (occluding_rect.size().GetCheckedArea().ValueOrDefault(
                      INT_MAX) <
                  smallest_rect.size().GetCheckedArea().ValueOrDefault(
                      INT_MAX)) {
                smallest_rect = occluding_rect;
              }
            }
            occlusion_in_target_space.Subtract(smallest_rect);
          }
        }

        // If the visible_rect of the current shared quad state does not
        // intersect with the occlusion rect, we can skip draw occlusion checks
        // for quads in the current SharedQuadState.
        last_sqs = quad->shared_quad_state;
        occlusion_in_quad_content_space.Clear();
        render_pass_quads_in_content_space = gfx::Rect();

        const auto current_sqs_in_target_space =
            cc::MathUtil::MapEnclosingClippedRect(
                transform, last_sqs->visible_quad_layer_rect);
        current_sqs_intersects_occlusion =
            occlusion_in_target_space.Intersects(current_sqs_in_target_space);

        // Compute the occlusion region in the quad content space for scale and
        // translation transforms. Note that 0 scale transform will fail the
        // positive scale check.
        if (current_sqs_intersects_occlusion &&
            transform.IsPositiveScaleOrTranslation()) {
          // Scale transform can be inverted by multiplying 1/scale (given
          // scale > 0) and translation transform can be inverted by applying
          // the reversed directional translation. Therefore, |transform| is
          // always invertible.
          gfx::Transform reverse_transform = transform.GetCheckedInverse();
          DCHECK_LE(occlusion_in_target_space.GetRegionComplexity(),
                    settings_.maximum_occluder_complexity);

          // Since transform can only be a scale or a translation matrix, it is
          // safe to use function MapEnclosedRectWith2dAxisAlignedTransform to
          // define occluded region in the quad content space with inverted
          // transform.
          for (gfx::Rect rect_in_target_space : occlusion_in_target_space) {
            if (current_sqs_in_target_space.Intersects(rect_in_target_space)) {
              auto rect_in_content =
                  cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                      reverse_transform, rect_in_target_space);
              occlusion_in_quad_content_space.Union(
                  SafeConvertRectForRegion(rect_in_content));
            }
          }

          // A render pass quad may apply some filter or transform to an
          // underlying quad. Do not split quads when they intersect with a
          // render pass quad.
          if (current_sqs_in_target_space.Intersects(
                  backdrop_filters_in_target_space.bounds())) {
            for (auto rect_in_target_space : backdrop_filters_in_target_space) {
              auto rect_in_content =
                  cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                      reverse_transform, rect_in_target_space);
              render_pass_quads_in_content_space.Union(rect_in_content);
            }
          }
        }
      }

      if (!current_sqs_intersects_occlusion) {
        ++quad;
        continue;
      }

      if (occlusion_in_quad_content_space.Contains(quad->visible_rect)) {
        // Case 1: for simple transforms (scale or translation), define the
        // occlusion region in the quad content space. If |quad| is not
        // shown on the screen, then set its rect and visible_rect to be empty.
        quad->visible_rect.set_size(gfx::Size());
      } else if (occlusion_in_quad_content_space.Intersects(
                     quad->visible_rect)) {
        // Case 2: for simple transforms, if the quad is partially shown on
        // screen and the region formed by (occlusion region - visible_rect) is
        // a rect, then update visible_rect to the resulting rect.
        cc::Region visible_region = quad->visible_rect;
        visible_region.Subtract(occlusion_in_quad_content_space);
        quad->visible_rect = visible_region.bounds();

        std::vector<gfx::Rect> reduced_visible_region;

        // Split quad into multiple draw quads when area can be reduce by
        // more than X fragments.
        const bool should_split_quads =
            !overlay_processor_->DisableSplittingQuads() &&
            !visible_region.Intersects(render_pass_quads_in_content_space) &&
            ReduceComplexity(visible_region, settings_.quad_split_limit,
                             reduced_visible_region) &&
            CanSplitQuad(quad->material, reduced_visible_region,
                         visible_region.bounds().size(),
                         settings_.minimum_fragments_reduced,
                         device_scale_factor);
        if (should_split_quads) {
          auto new_quad = pass->quad_list.InsertCopyBeforeDrawQuad(
              quad, reduced_visible_region.size() - 1);

          for (const auto& visible_rect : reduced_visible_region) {
            new_quad->visible_rect = visible_rect;
            ++new_quad;
          }

          quad = new_quad;
          continue;
        }
      } else if (occlusion_in_quad_content_space.IsEmpty() &&
                 occlusion_in_target_space.Contains(
                     cc::MathUtil::MapEnclosingClippedRect(
                         transform, quad->visible_rect))) {
        // Case 3: for non simple transforms, define the occlusion region in
        // target space. If |quad| is not shown on the screen, then set its
        // rect and visible_rect to be empty.
        quad->visible_rect.set_size(gfx::Size());
      }
      ++quad;
    }
  }
}

}  // namespace viz
