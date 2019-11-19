// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/stream_video_draw_quad.h"

#include "base/logging.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"

namespace viz {

StreamVideoDrawQuad::StreamVideoDrawQuad() = default;
StreamVideoDrawQuad::~StreamVideoDrawQuad() = default;
StreamVideoDrawQuad::StreamVideoDrawQuad(const StreamVideoDrawQuad& quad) =
    default;

void StreamVideoDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 bool needs_blending,
                                 unsigned resource_id,
                                 gfx::Size resource_size_in_pixels,
                                 const gfx::PointF& uv_top_left,
                                 const gfx::PointF& uv_bottom_right) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kStreamVideoContent,
                   rect, visible_rect, needs_blending);
  resources.ids[kResourceIdIndex] = resource_id;
  overlay_resources.size_in_pixels[kResourceIdIndex] = resource_size_in_pixels;
  resources.count = 1;
  this->uv_top_left = uv_top_left;
  this->uv_bottom_right = uv_bottom_right;
}

void StreamVideoDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                                 const gfx::Rect& rect,
                                 const gfx::Rect& visible_rect,
                                 bool needs_blending,
                                 unsigned resource_id,
                                 gfx::Size resource_size_in_pixels,
                                 const gfx::PointF& uv_top_left,
                                 const gfx::PointF& uv_bottom_right) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kStreamVideoContent,
                   rect, visible_rect, needs_blending);
  resources.ids[kResourceIdIndex] = resource_id;
  overlay_resources.size_in_pixels[kResourceIdIndex] = resource_size_in_pixels;
  resources.count = 1;
  this->uv_top_left = uv_top_left;
  this->uv_bottom_right = uv_bottom_right;
}

const StreamVideoDrawQuad* StreamVideoDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::Material::kStreamVideoContent);
  return static_cast<const StreamVideoDrawQuad*>(quad);
}

void StreamVideoDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  value->SetInteger("resource_id", resources.ids[kResourceIdIndex]);
  cc::MathUtil::AddToTracedValue("uv_top_left", uv_top_left, value);
  cc::MathUtil::AddToTracedValue("uv_bottom_right", uv_bottom_right, value);
}

StreamVideoDrawQuad::OverlayResources::OverlayResources() {}

}  // namespace viz
