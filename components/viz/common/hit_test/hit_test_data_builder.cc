// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/hit_test/hit_test_data_builder.h"

#include "components/viz/common/quads/surface_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
namespace {

const RenderPass* GetRenderPassInFrame(const CompositorFrame& frame,
                                       RenderPassId render_pass_id) {
  if (!render_pass_id)
    return frame.render_pass_list.back().get();

  for (const auto& render_pass : frame.render_pass_list) {
    if (render_pass->id == render_pass_id)
      return render_pass.get();
  }

  return nullptr;
}

void AddHitTestRegion(const FrameSinkId& frame_sink_id,
                      const gfx::Rect& visible_rect,
                      const gfx::Transform& hit_test_region_transform,
                      std::vector<HitTestRegion>* regions,
                      bool should_ask_for_child_region,
                      bool ignores_input_event) {
  regions->emplace_back();
  HitTestRegion* hit_test_region = &regions->back();
  hit_test_region->frame_sink_id = frame_sink_id;
  hit_test_region->flags = HitTestRegionFlags::kHitTestMouse |
                           HitTestRegionFlags::kHitTestTouch |
                           HitTestRegionFlags::kHitTestChildSurface;
  if (should_ask_for_child_region) {
    hit_test_region->flags |= HitTestRegionFlags::kHitTestAsk;
    hit_test_region->async_hit_test_reasons =
        AsyncHitTestReasons::kUseDrawQuadData;
  }
  if (ignores_input_event)
    hit_test_region->flags |= HitTestRegionFlags::kHitTestIgnore;
  hit_test_region->rect = visible_rect;
  hit_test_region->transform = hit_test_region_transform;
}

std::vector<gfx::Rect> ExtractAlphaRects(
    const cc::FilterOperations& filters,
    const gfx::Rect& surface_quad_rect,
    const gfx::Transform& quad_to_target_transform,
    const gfx::Transform& target_to_quad_transform,
    float device_scale_factor) {
  std::vector<gfx::Rect> filter_regions;
  if (filters.IsEmpty())
    return filter_regions;

  size_t i = 0;
  for (i = 0; i < filters.size(); ++i) {
    const cc::FilterOperation& op = filters.at(i);
    if (op.type() == cc::FilterOperation::ALPHA_THRESHOLD) {
      if (op.shape().empty())
        return filter_regions;

      for (const gfx::Rect& rect : op.shape()) {
        gfx::RectF rect_in_pixels(
            gfx::ScaleRect(gfx::RectF(rect), device_scale_factor));
        gfx::RectF surface_quad_rect_in_pass(surface_quad_rect);
        quad_to_target_transform.TransformRect(&surface_quad_rect_in_pass);
        if (surface_quad_rect_in_pass.Intersects(rect_in_pixels)) {
          gfx::RectF visible_rect(surface_quad_rect_in_pass);
          visible_rect.Intersect(rect_in_pixels);
          target_to_quad_transform.TransformRect(&visible_rect);
          filter_regions.push_back(gfx::ToNearestRect(visible_rect));
        }
      }
      return filter_regions;
    }
  }
  return filter_regions;
}

void AddHitTestDataFromRenderPass(
    const CompositorFrame& frame,
    RenderPassId render_pass_id,
    std::vector<HitTestRegion>* regions,
    bool should_ask_for_child_region,
    base::flat_map<RenderPassId, std::pair<uint32_t, uint32_t>>*
        render_pass_hit_test_region_list) {
  if (render_pass_hit_test_region_list->count(render_pass_id)) {
    const auto& list_range =
        render_pass_hit_test_region_list->find(render_pass_id)->second;
    const uint32_t start = list_range.first;
    const uint32_t end = list_range.second;
    if (start >= end || regions->size() < end)
      return;
    regions->insert(regions->end(), regions->begin() + start,
                    regions->begin() + end);
    return;
  }

  const RenderPass* render_pass = GetRenderPassInFrame(frame, render_pass_id);
  if (!render_pass)
    return;

  // Skip the render_pass if the transform is not invertible (i.e. it will not
  // be able to receive events).
  gfx::Transform transform_to_root_target =
      render_pass->transform_to_root_target;
  transform_to_root_target.FlattenTo2d();
  gfx::Transform transform_from_root_target;
  if (!transform_to_root_target.GetInverse(&transform_from_root_target)) {
    return;
  }

  const uint32_t render_pass_hit_test_region_list_start = regions->size();
  for (const DrawQuad* quad : render_pass->quad_list) {
    if (quad->material == DrawQuad::Material::kSurfaceContent) {
      const SurfaceDrawQuad* surface_quad = SurfaceDrawQuad::MaterialCast(quad);

      // Skip the quad if the transform is not invertible (i.e. it will not
      // be able to receive events).
      gfx::Transform quad_to_target_transform =
          quad->shared_quad_state->quad_to_target_transform;
      quad_to_target_transform.FlattenTo2d();
      gfx::Transform target_to_quad_transform;
      if (!quad_to_target_transform.GetInverse(&target_to_quad_transform)) {
        continue;
      }
      gfx::Transform hit_test_region_transform =
          target_to_quad_transform * transform_from_root_target;

      // If |surface_range|.begin() and end() have different frame sinks, use
      // end(). If there is no surface submitted for this FrameSinkId at
      // aggregation time, an async hit test query will be sent to the client.
      const auto& filters = render_pass->filters;
      std::vector<gfx::Rect> filter_regions = ExtractAlphaRects(
          filters, surface_quad->rect, quad_to_target_transform,
          target_to_quad_transform, frame.device_scale_factor());
      if (filter_regions.empty()) {
        AddHitTestRegion(surface_quad->surface_range.end().frame_sink_id(),
                         surface_quad->rect, hit_test_region_transform, regions,
                         should_ask_for_child_region,
                         surface_quad->ignores_input_event);
      } else {
        for (const auto& filter_region : filter_regions) {
          AddHitTestRegion(surface_quad->surface_range.end().frame_sink_id(),
                           filter_region, hit_test_region_transform, regions,
                           should_ask_for_child_region,
                           surface_quad->ignores_input_event);
        }
      }
    } else if (quad->material == DrawQuad::Material::kRenderPass) {
      const RenderPassDrawQuad* render_quad =
          RenderPassDrawQuad::MaterialCast(quad);
      AddHitTestDataFromRenderPass(frame, render_quad->render_pass_id, regions,
                                   should_ask_for_child_region,
                                   render_pass_hit_test_region_list);
    }
  }
  const uint32_t render_pass_hit_test_region_list_end = regions->size();
  render_pass_hit_test_region_list->emplace(
      render_pass_id, std::make_pair(render_pass_hit_test_region_list_start,
                                     render_pass_hit_test_region_list_end));
}

}  // namespace

// static
base::Optional<HitTestRegionList> HitTestDataBuilder::CreateHitTestData(
    const CompositorFrame& compositor_frame,
    bool root_accepts_events,
    bool should_ask_for_child_region) {
  base::Optional<HitTestRegionList> hit_test_region_list(base::in_place);
  hit_test_region_list->flags =
      (root_accepts_events ? HitTestRegionFlags::kHitTestMine
                           : HitTestRegionFlags::kHitTestIgnore) |
      HitTestRegionFlags::kHitTestMouse | HitTestRegionFlags::kHitTestTouch;
  hit_test_region_list->bounds.set_size(compositor_frame.size_in_pixels());
  base::flat_map<RenderPassId, std::pair<uint32_t, uint32_t>>
      render_pass_hit_test_region_list_cache;
  AddHitTestDataFromRenderPass(
      compositor_frame, compositor_frame.render_pass_list.back()->id,
      &hit_test_region_list->regions, should_ask_for_child_region,
      &render_pass_hit_test_region_list_cache);
  return hit_test_region_list;
}

}  // namespace viz
