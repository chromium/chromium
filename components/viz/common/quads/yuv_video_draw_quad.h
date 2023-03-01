// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_YUV_VIDEO_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_YUV_VIDEO_DRAW_QUAD_H_

#include "base/bits.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"

namespace viz {

class VIZ_COMMON_EXPORT YUVVideoDrawQuad : public DrawQuad {
 public:
  static const size_t kYPlaneResourceIdIndex = 0;
  static const size_t kUPlaneResourceIdIndex = 1;
  static const size_t kVPlaneResourceIdIndex = 2;
  static const size_t kAPlaneResourceIdIndex = 3;

  enum : uint32_t { kMinBitsPerChannel = 8, kMaxBitsPerChannel = 24 };

  static constexpr Material kMaterial = Material::kYuvVideoContent;

  ~YUVVideoDrawQuad() override;

  YUVVideoDrawQuad();
  YUVVideoDrawQuad(const YUVVideoDrawQuad& other);

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const gfx::Size& video_frame_coded_size,
              // |*_rect| contains non-normalized coordinates.
              // TODO(reveman): Make the use of normalized vs non-normalized
              // coordinates consistent across all quad types: crbug.com/487370
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
              absl::optional<gfx::HDRMetadata> metadata);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const gfx::Size& video_frame_coded_size,
              // |*_rect| contains non-normalized coordinates.
              // TODO(reveman): Make the use of normalized vs non-normalized
              // coordinates consistent across all quad types: crbug.com/487370
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
              absl::optional<gfx::HDRMetadata> metadata);

  // The video frame's coded size: the full dimensions of the video frame data
  // (see gfx::Media::VideoFrame::coded_size). The YA and UV texture sizes are
  // derived from this value.
  gfx::Size coded_size;
  gfx::Rect video_visible_rect;

  float resource_offset = 0.0f;
  float resource_multiplier = 1.0f;
  uint32_t bits_per_channel = 8;
  // TODO(hubbe): Move to ResourceProvider::ScopedSamplerGL.
  gfx::ColorSpace video_color_space;
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
  absl::optional<gfx::HDRMetadata> hdr_metadata;

  // This optional damage is in target render pass coordinate space.
  absl::optional<gfx::Rect> damage_rect;

  // The UV texture size scale relative to coded_size. Is either 1 or 2.
  uint8_t u_scale : 2;
  uint8_t v_scale : 2;

  static const YUVVideoDrawQuad* MaterialCast(const DrawQuad*);

  ResourceId y_plane_resource_id() const {
    return resources.ids[kYPlaneResourceIdIndex];
  }
  ResourceId u_plane_resource_id() const {
    return resources.ids[kUPlaneResourceIdIndex];
  }
  ResourceId v_plane_resource_id() const {
    return resources.ids[kVPlaneResourceIdIndex];
  }
  ResourceId a_plane_resource_id() const {
    return resources.ids[kAPlaneResourceIdIndex];
  }

  gfx::Size ya_tex_size() const { return coded_size; }

  gfx::Size uv_tex_size() const {
    // TODO: This code is duplicated with VideoFrame::Rows and Columns. Check if
    // AlignUp is ever needed in YUV textures.
    return gfx::Size(
        base::bits::AlignUp(coded_size.width(), static_cast<int>(u_scale)) /
            static_cast<int>(u_scale),
        base::bits::AlignUp(coded_size.height(), static_cast<int>(v_scale)) /
            static_cast<int>(v_scale));
  }

  gfx::RectF ya_tex_coord_rect() const {
    return gfx::RectF(video_visible_rect);
  }

  gfx::RectF uv_tex_coord_rect() const {
    gfx::RectF rect = ya_tex_coord_rect();
    rect.Scale(1.f / u_scale, 1.f / v_scale);
    return rect;
  }

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_YUV_VIDEO_DRAW_QUAD_H_
