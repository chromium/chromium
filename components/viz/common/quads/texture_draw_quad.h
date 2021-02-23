// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_TEXTURE_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_TEXTURE_DRAW_QUAD_H_

#include <stddef.h>

#include <memory>

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/video_types.h"

namespace viz {

class VIZ_COMMON_EXPORT TextureDrawQuad : public DrawQuad {
 public:
  static const size_t kResourceIdIndex = 0;

  TextureDrawQuad();
  TextureDrawQuad(const TextureDrawQuad& other);

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              ResourceId resource_id,
              bool premultiplied_alpha,
              const gfx::PointF& uv_top_left,
              const gfx::PointF& uv_bottom_right,
              SkColor background_color,
              const float vertex_opacity[4],
              bool y_flipped,
              bool nearest_neighbor,
              bool secure_output_only,
              gfx::ProtectedVideoType protected_video_type);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              ResourceId resource_id,
              gfx::Size resource_size_in_pixels,
              bool premultiplied_alpha,
              const gfx::PointF& uv_top_left,
              const gfx::PointF& uv_bottom_right,
              SkColor background_color,
              const float vertex_opacity[4],
              bool y_flipped,
              bool nearest_neighbor,
              bool secure_output_only,
              gfx::ProtectedVideoType protected_video_type);

  gfx::PointF uv_top_left;
  gfx::PointF uv_bottom_right;
  SkColor background_color = SK_ColorTRANSPARENT;
  float vertex_opacity[4] = {0, 0, 0, 0};
  bool y_flipped : 1;
  bool nearest_neighbor : 1;
  bool premultiplied_alpha : 1;

  // True if the quad must only be GPU composited if shown on secure outputs.
  bool secure_output_only : 1;

  // True if this quad contains a video frame from VideoResourceUpdater instead
  // of canvas or webgl content.
  bool is_video_frame : 1;

  // kClear if the contents do not require any special protection. See enum of a
  // list of protected content types. Protected contents cannot be displayed via
  // regular display path. They need either a protected output or a protected
  // hardware overlay.
  gfx::ProtectedVideoType protected_video_type : 2;

  // Identifier passed through by the video decoder that allows us to validate
  // if a protected surface can still be displayed. Non-zero when valid.
  uint32_t hw_protected_validation_id = 0;

  struct OverlayResources {
    OverlayResources();

    gfx::Size size_in_pixels;
  };
  OverlayResources overlay_resources;

  ResourceId resource_id() const { return resources.ids[kResourceIdIndex]; }
  const gfx::Size& resource_size_in_pixels() const {
    return overlay_resources.size_in_pixels;
  }
  void set_resource_size_in_pixels(const gfx::Size& size_in_pixels) {
    overlay_resources.size_in_pixels = size_in_pixels;
  }

  static const TextureDrawQuad* MaterialCast(const DrawQuad*);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_TEXTURE_DRAW_QUAD_H_
