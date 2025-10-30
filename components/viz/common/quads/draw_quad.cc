// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/draw_quad.h"

#include <stddef.h>

#include "base/check.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/traced_value.h"
#include "ui/gfx/geometry/quad_f.h"

namespace viz {

DrawQuad::DrawQuad()
    : material(Material::kInvalid),
      needs_blending(false),
      shared_quad_state(nullptr) {}

DrawQuad::DrawQuad(const DrawQuad& other) = default;

void DrawQuad::SetAll(const SharedQuadState* quad_state,
                      Material m,
                      const gfx::Rect& r,
                      const gfx::Rect& visible_r,
                      bool blending) {
  DCHECK(rect.Contains(visible_rect))
      << "rect: " << rect.ToString()
      << " visible_rect: " << visible_rect.ToString();

  material = m;
  rect = r;
  visible_rect = visible_r;
  needs_blending = blending;
  shared_quad_state = quad_state;

  DCHECK(shared_quad_state);
  DCHECK(material != Material::kInvalid);
}

DrawQuad::~DrawQuad() = default;

void DrawQuad::AsValueInto(
    base::trace_event::TracedValue* value,
    const std::unordered_map<const SharedQuadState*, size_t>&
        sqs_pointer_to_index_map,
    const std::unordered_map<ResourceId, size_t>& resource_id_to_index_map)
    const {
  value->SetInteger("material", static_cast<int>(material));

  value->BeginDictionary("shared_quad_state");
  auto it = sqs_pointer_to_index_map.find(shared_quad_state);
  DCHECK(it != sqs_pointer_to_index_map.end());
  value->SetInteger("index", it->second);
  value->EndDictionary();

  cc::MathUtil::AddToTracedValue("content_space_rect", rect, value);

  bool rect_is_clipped;
  gfx::QuadF rect_as_target_space_quad =
      cc::MathUtil::MapQuad(shared_quad_state->quad_to_target_transform,
                            gfx::QuadF(gfx::RectF(rect)), &rect_is_clipped);
  cc::MathUtil::AddToTracedValue("rect_as_target_space_quad",
                                 rect_as_target_space_quad, value);

  value->SetBoolean("rect_is_clipped", rect_is_clipped);

  cc::MathUtil::AddToTracedValue("content_space_visible_rect", visible_rect,
                                 value);

  bool visible_rect_is_clipped;
  gfx::QuadF visible_rect_as_target_space_quad = cc::MathUtil::MapQuad(
      shared_quad_state->quad_to_target_transform,
      gfx::QuadF(gfx::RectF(visible_rect)), &visible_rect_is_clipped);

  cc::MathUtil::AddToTracedValue("visible_rect_as_target_space_quad",
                                 visible_rect_as_target_space_quad, value);

  value->SetBoolean("visible_rect_is_clipped", visible_rect_is_clipped);

  value->SetBoolean("needs_blending", needs_blending);
  value->SetBoolean("should_draw_with_blending", ShouldDrawWithBlending());
  ExtendValue(value, resource_id_to_index_map);
}

int DrawQuad::ResourceIdIndex(
    const std::unordered_map<ResourceId, size_t>& resource_id_to_index_map,
    ResourceId id) const {
  if (!resource_id_to_index_map.size()) {
    // Not all code paths set up |resource_id_to_index_map|. In such cases,
    // just log the original resource id.
    return static_cast<int>(id.GetUnsafeValue());
  }
  auto it = resource_id_to_index_map.find(id);
  if (it == resource_id_to_index_map.end()) {
    return -1;
  }
  return static_cast<int>(it->second);
}

}  // namespace viz
