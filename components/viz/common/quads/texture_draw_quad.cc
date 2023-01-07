// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/texture_draw_quad.h"

#include <stddef.h>

#include "base/check.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {

TextureDrawQuad::TextureDrawQuad()
    : y_flipped(false),
      nearest_neighbor(false),
      premultiplied_alpha(false),
      secure_output_only(false),
      is_video_frame(false),
      is_stream_video(false),
      protected_video_type(gfx::ProtectedVideoType::kClear) {
  static_assert(static_cast<int>(gfx::ProtectedVideoType::kMaxValue) < 4,
                "protected_video_type needs more bits in order to represent "
                "all the enum values");
}

TextureDrawQuad::TextureDrawQuad(const TextureDrawQuad& other) = default;

TextureDrawQuad::~TextureDrawQuad() = default;

void TextureDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             ResourceId resource_id,
                             bool premultiplied,
                             const gfx::PointF& top_left,
                             const gfx::PointF& bottom_right,
                             SkColor4f background,
                             const float opacity[4],
                             bool flipped,
                             bool nearest,
                             bool secure_output,
                             gfx::ProtectedVideoType video_type) {
  needs_blending = needs_blending || opacity[0] != 1.0f || opacity[1] != 1.0f ||
                   opacity[2] != 1.0f || opacity[3] != 1.0f;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kTextureContent, rect,
                   visible_rect, needs_blending);
  resources.ids[kResourceIdIndex] = resource_id;
  resources.count = 1;
  premultiplied_alpha = premultiplied;
  uv_top_left = top_left;
  uv_bottom_right = bottom_right;
  background_color = background;
  vertex_opacity[0] = opacity[0];
  vertex_opacity[1] = opacity[1];
  vertex_opacity[2] = opacity[2];
  vertex_opacity[3] = opacity[3];
  y_flipped = flipped;
  nearest_neighbor = nearest;
  secure_output_only = secure_output;
  protected_video_type = video_type;
}

void TextureDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             ResourceId resource_id,
                             gfx::Size resource_size_in_pixels,
                             bool premultiplied,
                             const gfx::PointF& top_left,
                             const gfx::PointF& bottom_right,
                             SkColor4f background,
                             const float opacity[4],
                             bool flipped,
                             bool nearest,
                             bool secure_output,
                             gfx::ProtectedVideoType video_type) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kTextureContent, rect,
                   visible_rect, needs_blending);
  resources.ids[kResourceIdIndex] = resource_id;
  overlay_resources.size_in_pixels = resource_size_in_pixels;
  resources.count = 1;
  premultiplied_alpha = premultiplied;
  uv_top_left = top_left;
  uv_bottom_right = bottom_right;
  background_color = background;
  vertex_opacity[0] = opacity[0];
  vertex_opacity[1] = opacity[1];
  vertex_opacity[2] = opacity[2];
  vertex_opacity[3] = opacity[3];
  y_flipped = flipped;
  nearest_neighbor = nearest;
  secure_output_only = secure_output;
  protected_video_type = video_type;
}

const TextureDrawQuad* TextureDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::Material::kTextureContent);
  return static_cast<const TextureDrawQuad*>(quad);
}

void TextureDrawQuad::ExtendValue(base::trace_event::TracedValue* value) const {
  value->SetInteger("resource_id",
                    resources.ids[kResourceIdIndex].GetUnsafeValue());
  value->SetBoolean("premultiplied_alpha", premultiplied_alpha);

  cc::MathUtil::AddToTracedValue("uv_top_left", uv_top_left, value);
  cc::MathUtil::AddToTracedValue("uv_bottom_right", uv_bottom_right, value);

  value->SetString("background_color",
                   color_utils::SkColor4fToRgbaString(background_color));

  value->BeginArray("vertex_opacity");
  for (size_t i = 0; i < 4; ++i)
    value->AppendDouble(vertex_opacity[i]);
  value->EndArray();

  value->SetBoolean("y_flipped", y_flipped);
  value->SetBoolean("nearest_neighbor", nearest_neighbor);
  value->SetBoolean("is_video_frame", is_video_frame);
  value->SetBoolean("is_stream_video", is_stream_video);
  value->SetInteger("protected_video_type",
                    static_cast<int>(protected_video_type));
}

TextureDrawQuad::OverlayResources::OverlayResources() = default;

}  // namespace viz
