// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/yuv_video_draw_quad.h"

#include "base/check.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "ui/gfx/hdr_metadata.h"

namespace viz {

YUVVideoDrawQuad::YUVVideoDrawQuad() = default;

YUVVideoDrawQuad::YUVVideoDrawQuad(const YUVVideoDrawQuad& other) = default;

YUVVideoDrawQuad::~YUVVideoDrawQuad() = default;

void YUVVideoDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                              const gfx::Rect& rect,
                              const gfx::Rect& visible_rect,
                              bool needs_blending,
                              const gfx::Size& video_frame_coded_size,
                              const gfx::Rect& video_frame_visible_rect,
                              // Returned from VideFrame::SampleSize.
                              const gfx::Size& video_frame_uv_sample_size,
                              ResourceId y_plane_resource_id,
                              ResourceId u_plane_resource_id,
                              ResourceId v_plane_resource_id,
                              ResourceId a_plane_resource_id,
                              const gfx::ColorSpace& color_space,
                              float offset,
                              float multiplier,
                              uint32_t bits,
                              gfx::ProtectedVideoType video_type,
                              absl::optional<gfx::HDRMetadata> metadata) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kYuvVideoContent,
                   rect, visible_rect, needs_blending);

  // Make sure the scale is either 1 or 2.
  DCHECK_LE(video_frame_uv_sample_size.width(), 2);
  DCHECK_LE(video_frame_uv_sample_size.height(), 2);

  coded_size = video_frame_coded_size;
  video_visible_rect = video_frame_visible_rect;
  u_scale = video_frame_uv_sample_size.width();
  v_scale = video_frame_uv_sample_size.height();
  resources.ids[kYPlaneResourceIdIndex] = y_plane_resource_id;
  resources.ids[kUPlaneResourceIdIndex] = u_plane_resource_id;
  resources.ids[kVPlaneResourceIdIndex] = v_plane_resource_id;
  resources.ids[kAPlaneResourceIdIndex] = a_plane_resource_id;
  resources.count = a_plane_resource_id ? 4 : 3;
  video_color_space = color_space;
  resource_offset = offset;
  resource_multiplier = multiplier;
  bits_per_channel = bits;
  protected_video_type = video_type;
  hdr_metadata = metadata;
}

void YUVVideoDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                              const gfx::Rect& rect,
                              const gfx::Rect& visible_rect,
                              bool needs_blending,
                              const gfx::Size& video_frame_coded_size,
                              const gfx::Rect& video_frame_visible_rect,
                              // Returned from VideFrame::SampleSize.
                              const gfx::Size& video_frame_uv_sample_size,
                              ResourceId y_plane_resource_id,
                              ResourceId u_plane_resource_id,
                              ResourceId v_plane_resource_id,
                              ResourceId a_plane_resource_id,
                              const gfx::ColorSpace& color_space,
                              float offset,
                              float multiplier,
                              uint32_t bits,
                              gfx::ProtectedVideoType video_type,
                              absl::optional<gfx::HDRMetadata> metadata) {
  SetNew(shared_quad_state, rect, visible_rect, needs_blending,
         video_frame_coded_size, video_frame_visible_rect,
         video_frame_uv_sample_size, y_plane_resource_id, u_plane_resource_id,
         v_plane_resource_id, a_plane_resource_id, color_space, offset,
         multiplier, bits, video_type, metadata);
}

const YUVVideoDrawQuad* YUVVideoDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::Material::kYuvVideoContent);
  return static_cast<const YUVVideoDrawQuad*>(quad);
}

void YUVVideoDrawQuad::ExtendValue(
    base::trace_event::TracedValue* value) const {
  cc::MathUtil::AddToTracedValue("ya_tex_coord_rect", ya_tex_coord_rect(),
                                 value);
  cc::MathUtil::AddToTracedValue("uv_tex_coord_rect", uv_tex_coord_rect(),
                                 value);
  cc::MathUtil::AddToTracedValue("ya_tex_size", ya_tex_size(), value);
  cc::MathUtil::AddToTracedValue("uv_tex_size", uv_tex_size(), value);
  value->SetInteger("y_plane_resource_id",
                    resources.ids[kYPlaneResourceIdIndex].GetUnsafeValue());
  value->SetInteger("u_plane_resource_id",
                    resources.ids[kUPlaneResourceIdIndex].GetUnsafeValue());
  value->SetInteger("v_plane_resource_id",
                    resources.ids[kVPlaneResourceIdIndex].GetUnsafeValue());
  value->SetInteger("a_plane_resource_id",
                    resources.ids[kAPlaneResourceIdIndex].GetUnsafeValue());
  value->SetInteger("protected_video_type",
                    static_cast<int>(protected_video_type));
}

}  // namespace viz
