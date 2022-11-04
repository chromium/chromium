// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_TEXTURE_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_TEXTURE_DRAW_QUAD_H_

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"

namespace viz {

// The priority for a quads to require being promoted to overlay.
enum class OverlayPriority { kLow, kRegular, kRequired };

class VIZ_COMMON_EXPORT TextureDrawQuad : public DrawQuad {
 public:
  static const size_t kResourceIdIndex = 0;

  TextureDrawQuad();
  TextureDrawQuad(const TextureDrawQuad& other);

  ~TextureDrawQuad() override;

  void SetNew(const SharedQuadState* shared_quad_state,
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
              gfx::ProtectedVideoType video_type);

  void SetAll(const SharedQuadState* shared_quad_state,
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
              gfx::ProtectedVideoType video_type);

  gfx::PointF uv_top_left;
  gfx::PointF uv_bottom_right;
  SkColor4f background_color = SkColors::kTransparent;
  float vertex_opacity[4] = {0, 0, 0, 0};
  bool y_flipped : 1;
  bool nearest_neighbor : 1;
  bool premultiplied_alpha : 1;

  // True if the quad must only be GPU composited if shown on secure outputs.
  bool secure_output_only : 1;

  // True if this quad contains a video frame from VideoResourceUpdater instead
  // of canvas or webgl content.
  bool is_video_frame : 1;

  // True if this quad is a stream video texture. This mostly affects overlay
  // creation (e.g. color space, protection type).
  bool is_stream_video : 1;

  gfx::HDRMode hdr_mode = gfx::HDRMode::kDefault;
  absl::optional<gfx::HDRMetadata> hdr_metadata;

  // kClear if the contents do not require any special protection. See enum of a
  // list of protected content types. Protected contents cannot be displayed via
  // regular display path. They need either a protected output or a protected
  // hardware overlay.
  gfx::ProtectedVideoType protected_video_type : 2;
  // The overlay promotion hint.
  OverlayPriority overlay_priority_hint = OverlayPriority::kRegular;

  // This optional damage is in target render pass coordinate space.
  absl::optional<gfx::Rect> damage_rect;

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
