// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/surface_draw_quad.h"

#include "base/logging.h"
#include "base/optional.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"

namespace viz {

SurfaceDrawQuad::SurfaceDrawQuad() = default;

SurfaceDrawQuad::SurfaceDrawQuad(const SurfaceDrawQuad& other) = default;

SurfaceDrawQuad::~SurfaceDrawQuad() = default;

SurfaceDrawQuad& SurfaceDrawQuad::operator=(const SurfaceDrawQuad& other) =
    default;

void SurfaceDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             const SurfaceRange& surface_range,
                             SkColor default_background_color,
                             bool stretch_content_to_fill_bounds,
                             bool ignores_input_event) {
  bool needs_blending = true;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kSurfaceContent, rect,
                   visible_rect, needs_blending);
  this->surface_range = surface_range;
  this->default_background_color = default_background_color;
  this->stretch_content_to_fill_bounds = stretch_content_to_fill_bounds;
  this->ignores_input_event = ignores_input_event;
}

void SurfaceDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             const SurfaceRange& surface_range,
                             SkColor default_background_color,
                             bool stretch_content_to_fill_bounds,
                             bool ignores_input_event,
                             bool is_reflection,
                             bool allow_merge) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kSurfaceContent, rect,
                   visible_rect, needs_blending);
  this->surface_range = surface_range;
  this->default_background_color = default_background_color;
  this->stretch_content_to_fill_bounds = stretch_content_to_fill_bounds;
  this->ignores_input_event = ignores_input_event;
  this->is_reflection = is_reflection;
  this->allow_merge = allow_merge;
}

const SurfaceDrawQuad* SurfaceDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK_EQ(quad->material, DrawQuad::Material::kSurfaceContent);
  return static_cast<const SurfaceDrawQuad*>(quad);
}

void SurfaceDrawQuad::ExtendValue(base::trace_event::TracedValue* value) const {
  value->SetString("surface_range", surface_range.ToString());
}

}  // namespace viz
