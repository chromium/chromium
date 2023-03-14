// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_DEBUG_BORDER_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_DEBUG_BORDER_DRAW_QUAD_H_

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace viz {

class VIZ_COMMON_EXPORT DebugBorderDrawQuad : public DrawQuad {
 public:
  static constexpr Material kMaterial = Material::kDebugBorder;

  DebugBorderDrawQuad();

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              SkColor4f c,
              int w);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              SkColor4f c,
              int w);

  SkColor4f color = SkColors::kTransparent;
  int width = 0;

  static const DebugBorderDrawQuad* MaterialCast(const DrawQuad*);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_DEBUG_BORDER_DRAW_QUAD_H_
