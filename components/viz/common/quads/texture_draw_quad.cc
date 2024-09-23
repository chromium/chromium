// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/common/quads/texture_draw_quad.h"

#include <stddef.h>

#include "base/check.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_f.h"

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
                             bool flipped,
                             bool nearest,
                             bool secure_output,
                             gfx::ProtectedVideoType video_type) {
  CHECK_NE(resource_id, kInvalidResourceId);
  this->needs_blending = needs_blending;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kTextureContent, rect,
                   visible_rect, needs_blending);
  resources.ids[kResourceIdIndex] = resource_id;
  resources.count = 1;
  premultiplied_alpha = premultiplied;
  uv_top_left = top_left;
  uv_bottom_right = bottom_right;
  background_color = background;
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
                             bool flipped,
                             bool nearest,
                             bool secure_output,
                             gfx::ProtectedVideoType video_type) {
  CHECK_NE(resource_id, kInvalidResourceId);
  DrawQuad::SetAll(shared_quad_state, DrawQuad::Material::kTextureContent, rect,
                   visible_rect, needs_blending);
  resources.ids[kResourceIdIndex] = resource_id;
  overlay_resources.size_in_pixels = resource_size_in_pixels;
  resources.count = 1;
  premultiplied_alpha = premultiplied;
  uv_top_left = top_left;
  uv_bottom_right = bottom_right;
  background_color = background;
  y_flipped = flipped;
  nearest_neighbor = nearest;
  secure_output_only = secure_output;
  protected_video_type = video_type;
}

const TextureDrawQuad* TextureDrawQuad::MaterialCast(const DrawQuad* quad) {
  CHECK_EQ(quad->material, DrawQuad::Material::kTextureContent);
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

  value->SetString(
      "rounded_display_masks_info",
      base::StringPrintf(
          "%d,%d,is_horizontally_positioned=%d",
          rounded_display_masks_info
              .radii[RoundedDisplayMasksInfo::kOriginRoundedDisplayMaskIndex],
          rounded_display_masks_info
              .radii[RoundedDisplayMasksInfo::kOtherRoundedDisplayMaskIndex],
          static_cast<int>(
              rounded_display_masks_info.is_horizontally_positioned)));

  value->SetBoolean("y_flipped", y_flipped);
  value->SetBoolean("nearest_neighbor", nearest_neighbor);
  value->SetBoolean("is_video_frame", is_video_frame);
  value->SetBoolean("is_stream_video", is_stream_video);
  value->SetInteger("protected_video_type",
                    static_cast<int>(protected_video_type));
}

TextureDrawQuad::OverlayResources::OverlayResources() = default;

TextureDrawQuad::RoundedDisplayMasksInfo::RoundedDisplayMasksInfo() = default;

// static
TextureDrawQuad::RoundedDisplayMasksInfo
TextureDrawQuad::RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
    int origin_rounded_display_mask_radius,
    int other_rounded_display_mask_radius,
    bool is_horizontally_positioned) {
  RoundedDisplayMasksInfo info;
  info.radii[kOriginRoundedDisplayMaskIndex] =
      origin_rounded_display_mask_radius;
  info.radii[kOtherRoundedDisplayMaskIndex] = other_rounded_display_mask_radius;
  info.is_horizontally_positioned = is_horizontally_positioned;

  return info;
}

// static
std::array<
    gfx::RectF,
    TextureDrawQuad::RoundedDisplayMasksInfo::kMaxRoundedDisplayMasksCount>
TextureDrawQuad::RoundedDisplayMasksInfo::GetRoundedDisplayMasksBounds(
    const DrawQuad* quad) {
  std::array<gfx::RectF, RoundedDisplayMasksInfo::kMaxRoundedDisplayMasksCount>
      mask_rects;

  const TextureDrawQuad* texture_quad = quad->DynamicCast<TextureDrawQuad>();
  if (!texture_quad) {
    return mask_rects;
  }

  TextureDrawQuad::RoundedDisplayMasksInfo mask_info =
      texture_quad->rounded_display_masks_info;

  if (mask_info.IsEmpty()) {
    return mask_rects;
  }

  const gfx::Transform& transform =
      quad->shared_quad_state->quad_to_target_transform;
  const gfx::RectF target_rect = transform.MapRect(gfx::RectF(quad->rect));

  const int16_t origin_mask_radius =
      mask_info.radii[TextureDrawQuad::RoundedDisplayMasksInfo::
                          kOriginRoundedDisplayMaskIndex];
  mask_rects[RoundedDisplayMasksInfo::kOriginRoundedDisplayMaskIndex] =
      gfx::RectF(target_rect.x(), target_rect.y(), origin_mask_radius,
                 origin_mask_radius);

  const int16_t other_mask_radius =
      mask_info.radii[TextureDrawQuad::RoundedDisplayMasksInfo::
                          kOtherRoundedDisplayMaskIndex];
  if (mask_info.is_horizontally_positioned) {
    mask_rects[RoundedDisplayMasksInfo::kOtherRoundedDisplayMaskIndex] =
        gfx::RectF(target_rect.x() + target_rect.width() - other_mask_radius,
                   target_rect.y(), other_mask_radius, other_mask_radius);
  } else {
    mask_rects[RoundedDisplayMasksInfo::kOtherRoundedDisplayMaskIndex] =
        gfx::RectF(target_rect.x(),
                   target_rect.y() + target_rect.height() - other_mask_radius,
                   other_mask_radius, other_mask_radius);
  }

  return mask_rects;
}

bool TextureDrawQuad::RoundedDisplayMasksInfo::IsEmpty() const {
  return radii[kOriginRoundedDisplayMaskIndex] == 0 &&
         radii[kOtherRoundedDisplayMaskIndex] == 0;
}

}  // namespace viz
