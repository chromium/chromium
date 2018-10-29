// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/yuv_video_draw_quad.h"

#include "base/logging.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"

namespace viz {

YUVVideoDrawQuad::YUVVideoDrawQuad() = default;

YUVVideoDrawQuad::YUVVideoDrawQuad(const YUVVideoDrawQuad& other) = default;

YUVVideoDrawQuad::~YUVVideoDrawQuad() = default;

void YUVVideoDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                              const gfx::Rect& rect,
                              const gfx::Rect& visible_rect,
                              bool needs_blending,
                              const gfx::RectF& ya_tex_coord_rect,
                              const gfx::RectF& uv_tex_coord_rect,
                              const gfx::Size& ya_tex_size,
                              const gfx::Size& uv_tex_size,
                              unsigned y_plane_resource_id,
                              unsigned u_plane_resource_id,
                              unsigned v_plane_resource_id,
                              unsigned a_plane_resource_id,
                              const gfx::ColorSpace& video_color_space,
                              float offset,
                              float multiplier,
                              uint32_t bits_per_channel) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::YUV_VIDEO_CONTENT, rect,
                   visible_rect, needs_blending);
  this->ya_tex_coord_rect = ya_tex_coord_rect;
  this->uv_tex_coord_rect = uv_tex_coord_rect;
  this->ya_tex_size = ya_tex_size;
  this->uv_tex_size = uv_tex_size;
  resources.ids[kYPlaneResourceIdIndex] = y_plane_resource_id;
  resources.ids[kUPlaneResourceIdIndex] = u_plane_resource_id;
  resources.ids[kVPlaneResourceIdIndex] = v_plane_resource_id;
  resources.ids[kAPlaneResourceIdIndex] = a_plane_resource_id;
  resources.count = a_plane_resource_id ? 4 : 3;
  this->video_color_space = video_color_space;
  this->resource_offset = offset;
  this->resource_multiplier = multiplier;
  this->bits_per_channel = bits_per_channel;
}

void YUVVideoDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                              const gfx::Rect& rect,
                              const gfx::Rect& visible_rect,
                              bool needs_blending,
                              const gfx::RectF& ya_tex_coord_rect,
                              const gfx::RectF& uv_tex_coord_rect,
                              const gfx::Size& ya_tex_size,
                              const gfx::Size& uv_tex_size,
                              unsigned y_plane_resource_id,
                              unsigned u_plane_resource_id,
                              unsigned v_plane_resource_id,
                              unsigned a_plane_resource_id,
                              const gfx::ColorSpace& video_color_space,
                              float offset,
                              float multiplier,
                              uint32_t bits_per_channel,
                              bool require_overlay,
                              bool is_protected_video) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::YUV_VIDEO_CONTENT, rect,
                   visible_rect, needs_blending);
  this->ya_tex_coord_rect = ya_tex_coord_rect;
  this->uv_tex_coord_rect = uv_tex_coord_rect;
  this->ya_tex_size = ya_tex_size;
  this->uv_tex_size = uv_tex_size;
  resources.ids[kYPlaneResourceIdIndex] = y_plane_resource_id;
  resources.ids[kUPlaneResourceIdIndex] = u_plane_resource_id;
  resources.ids[kVPlaneResourceIdIndex] = v_plane_resource_id;
  resources.ids[kAPlaneResourceIdIndex] = a_plane_resource_id;
  resources.count = resources.ids[kAPlaneResourceIdIndex] ? 4 : 3;
  this->video_color_space = video_color_space;
  this->resource_offset = offset;
  this->resource_multiplier = multiplier;
  this->bits_per_channel = bits_per_channel;
  this->require_overlay = require_overlay;
  this->is_protected_video = is_protected_video;
}

const YUVVideoDrawQuad* YUVVideoDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::YUV_VIDEO_CONTENT);
  return static_cast<const YUVVideoDrawQuad*>(quad);
}

void YUVVideoDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  cc::MathUtil::AddToTracedValue("ya_tex_coord_rect", ya_tex_coord_rect, value);
  cc::MathUtil::AddToTracedValue("uv_tex_coord_rect", uv_tex_coord_rect, value);
  cc::MathUtil::AddToTracedValue("ya_tex_size", ya_tex_size, value);
  cc::MathUtil::AddToTracedValue("uv_tex_size", uv_tex_size, value);
  value->SetInteger("y_plane_resource_id",
                    resources.ids[kYPlaneResourceIdIndex]);
  value->SetInteger("u_plane_resource_id",
                    resources.ids[kUPlaneResourceIdIndex]);
  value->SetInteger("v_plane_resource_id",
                    resources.ids[kVPlaneResourceIdIndex]);
  value->SetInteger("a_plane_resource_id",
                    resources.ids[kAPlaneResourceIdIndex]);
  value->SetBoolean("require_overlay", require_overlay);
  value->SetBoolean("is_protected_video", is_protected_video);
}

}  // namespace viz
