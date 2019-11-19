// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/tile_draw_quad.h"

#include "base/logging.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"

namespace viz {

TileDrawQuad::TileDrawQuad() = default;

TileDrawQuad::~TileDrawQuad() = default;

void TileDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                          const gfx::Rect& rect,
                          const gfx::Rect& visible_rect,
                          bool needs_blending,
                          unsigned resource_id,
                          const gfx::RectF& tex_coord_rect,
                          const gfx::Size& texture_size,
                          bool is_premultiplied,
                          bool nearest_neighbor,
                          bool force_anti_aliasing_off) {
  ContentDrawQuadBase::SetNew(
      shared_quad_state, DrawQuad::Material::kTiledContent, rect, visible_rect,
      needs_blending, tex_coord_rect, texture_size, is_premultiplied,
      nearest_neighbor, force_anti_aliasing_off);
  resources.ids[kResourceIdIndex] = resource_id;
  resources.count = 1;
}

void TileDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                          const gfx::Rect& rect,
                          const gfx::Rect& visible_rect,
                          bool needs_blending,
                          unsigned resource_id,
                          const gfx::RectF& tex_coord_rect,
                          const gfx::Size& texture_size,
                          bool is_premultiplied,
                          bool nearest_neighbor,
                          bool force_anti_aliasing_off) {
  ContentDrawQuadBase::SetAll(
      shared_quad_state, DrawQuad::Material::kTiledContent, rect, visible_rect,
      needs_blending, tex_coord_rect, texture_size, is_premultiplied,
      nearest_neighbor, force_anti_aliasing_off);
  resources.ids[kResourceIdIndex] = resource_id;
  resources.count = 1;
}

const TileDrawQuad* TileDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::Material::kTiledContent);
  return static_cast<const TileDrawQuad*>(quad);
}

void TileDrawQuad::ExtendValue(base::trace_event::TracedValue* value) const {
  ContentDrawQuadBase::ExtendValue(value);
  value->SetInteger("resource_id", resources.ids[kResourceIdIndex]);
}

}  // namespace viz
