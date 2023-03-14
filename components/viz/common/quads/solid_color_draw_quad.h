// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SOLID_COLOR_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SOLID_COLOR_DRAW_QUAD_H_

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace viz {

class VIZ_COMMON_EXPORT SolidColorDrawQuad : public DrawQuad {
 public:
  static constexpr Material kMaterial = Material::kSolidColor;

  SolidColorDrawQuad();

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              SkColor4f c,
              bool anti_aliasing_off);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              SkColor4f c,
              bool anti_aliasing_off);

  SkColor4f color;
  bool force_anti_aliasing_off = false;

  static const SolidColorDrawQuad* MaterialCast(const DrawQuad*);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SOLID_COLOR_DRAW_QUAD_H_
