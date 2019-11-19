// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/video_hole_draw_quad.h"

#include <stddef.h>
#include "base/trace_event/traced_value.h"
#include "base/values.h"

namespace viz {

VideoHoleDrawQuad::VideoHoleDrawQuad() = default;

VideoHoleDrawQuad::VideoHoleDrawQuad(const VideoHoleDrawQuad& other) = default;

VideoHoleDrawQuad::~VideoHoleDrawQuad() = default;

void VideoHoleDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                               const gfx::Rect& rect,
                               const gfx::Rect& visible_rect,
                               const base::UnguessableToken& overlay_plane_id) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kVideoHole, rect,
                   visible_rect,
                   /*needs_blending=*/false);
  this->overlay_plane_id = overlay_plane_id;
}

void VideoHoleDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                               const gfx::Rect& rect,
                               const gfx::Rect& visible_rect,
                               bool needs_blending,
                               const base::UnguessableToken& overlay_plane_id) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kVideoHole, rect,
                   visible_rect, needs_blending);
  this->overlay_plane_id = overlay_plane_id;
}

const VideoHoleDrawQuad* VideoHoleDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::Material::kVideoHole);
  return static_cast<const VideoHoleDrawQuad*>(quad);
}

void VideoHoleDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  value->SetString("overlay_plane_id", overlay_plane_id.ToString());
}

}  // namespace viz
