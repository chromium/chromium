// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/surface_draw_quad.h"

#include "base/check_op.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "ui/gfx/color_utils.h"

namespace viz {

SurfaceDrawQuad::SurfaceDrawQuad() = default;

SurfaceDrawQuad::SurfaceDrawQuad(const SurfaceDrawQuad& other) = default;

SurfaceDrawQuad::~SurfaceDrawQuad() = default;

SurfaceDrawQuad& SurfaceDrawQuad::operator=(const SurfaceDrawQuad& other) =
    default;

void SurfaceDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             const SurfaceRange& range,
                             SkColor4f background_color,
                             bool stretch_content) {
  bool needs_blending = true;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kSurfaceContent, rect,
                   visible_rect, needs_blending);
  surface_range = range;
  default_background_color = background_color;
  stretch_content_to_fill_bounds = stretch_content;
}

void SurfaceDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             const SurfaceRange& range,
                             SkColor4f background_color,
                             bool stretch_content,
                             bool reflection,
                             bool merge) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kSurfaceContent, rect,
                   visible_rect, needs_blending);
  surface_range = range;
  default_background_color = background_color;
  stretch_content_to_fill_bounds = stretch_content;
  is_reflection = reflection;
  allow_merge = merge;
}

const SurfaceDrawQuad* SurfaceDrawQuad::MaterialCast(const DrawQuad* quad) {
  CHECK_EQ(quad->material, DrawQuad::Material::kSurfaceContent);
  return static_cast<const SurfaceDrawQuad*>(quad);
}

void SurfaceDrawQuad::ExtendValue(base::trace_event::TracedValue* value) const {
  value->SetString("surface_range", surface_range.ToString());
  value->SetString(
      "default_background_color",
      color_utils::SkColor4fToRgbaString(default_background_color));
  value->SetBoolean("stretch_content_to_fill_bounds",
                    stretch_content_to_fill_bounds);
  value->SetBoolean("is_reflection", is_reflection);
  value->SetBoolean("allow_merge", allow_merge);
  if (override_child_filter_quality) {
    value->SetInteger("override_child_filter_quality",
                      static_cast<int>(*override_child_filter_quality));
  }
  if (override_child_dynamic_range_limit) {
    value->SetString("override_child_dynamic_range_limit",
                     override_child_dynamic_range_limit->ToString());
  }
}

}  // namespace viz
