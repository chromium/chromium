// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SHARED_ELEMENT_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SHARED_ELEMENT_DRAW_QUAD_H_

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

class VIZ_COMMON_EXPORT SharedElementDrawQuad : public DrawQuad {
 public:
  static constexpr Material kMaterial = Material::kSharedElement;

  SharedElementDrawQuad();
  SharedElementDrawQuad(const SharedElementDrawQuad& other);
  ~SharedElementDrawQuad() override;

  SharedElementDrawQuad& operator=(const SharedElementDrawQuad& other);

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              const ViewTransitionElementResourceId& id);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const ViewTransitionElementResourceId& id);

  ViewTransitionElementResourceId resource_id;

  static const SharedElementDrawQuad* MaterialCast(const DrawQuad* quad);

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SHARED_ELEMENT_DRAW_QUAD_H_
