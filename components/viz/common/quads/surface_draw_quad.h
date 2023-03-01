// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SURFACE_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SURFACE_DRAW_QUAD_H_

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace viz {

class VIZ_COMMON_EXPORT SurfaceDrawQuad : public DrawQuad {
 public:
  static constexpr Material kMaterial = Material::kSurfaceContent;

  SurfaceDrawQuad();
  SurfaceDrawQuad(const SurfaceDrawQuad& other);
  ~SurfaceDrawQuad() override;

  SurfaceDrawQuad& operator=(const SurfaceDrawQuad& other);

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              const SurfaceRange& range,
              SkColor4f background_color,
              bool stretch_content);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const SurfaceRange& range,
              SkColor4f background_color,
              bool stretch_content,
              bool reflection,
              bool merge);

  SurfaceRange surface_range;
  SkColor4f default_background_color = SkColors::kWhite;
  bool stretch_content_to_fill_bounds = false;
  bool is_reflection = false;
  // If true, allows this surface to be merged into the embedding surface,
  // avoiding an intermediate texture.
  bool allow_merge = true;

  static const SurfaceDrawQuad* MaterialCast(const DrawQuad* quad);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SURFACE_DRAW_QUAD_H_
