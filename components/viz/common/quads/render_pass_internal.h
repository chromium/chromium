// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_INTERNAL_H_
#define COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_INTERNAL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/optional.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/quad_list.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"

namespace viz {
class SharedQuadState;
class CopyOutputRequest;

using SharedQuadStateList = cc::ListContainer<SharedQuadState>;

// This class represents common data that is shared between the compositor and
// aggregated render passes.
class VIZ_COMMON_EXPORT RenderPassInternal {
 public:
  SharedQuadState* CreateAndAppendSharedQuadState();

  // These are in the space of the render pass' physical pixels.
  gfx::Rect output_rect;
  gfx::Rect damage_rect;

  // Transforms from the origin of the |output_rect| to the origin of the root
  // render pass' |output_rect|.
  gfx::Transform transform_to_root_target;

  // Post-processing filters, applied to the pixels in the render pass' texture.
  cc::FilterOperations filters;

  // Post-processing filters, applied to the pixels showing through the
  // backdrop of the render pass, from behind it.
  cc::FilterOperations backdrop_filters;

  // Clipping bounds for backdrop filter.
  base::Optional<gfx::RRectF> backdrop_filter_bounds;

  // If false, the pixels in the render pass' texture are all opaque.
  bool has_transparent_background = true;

  // If true we might reuse the texture if there is no damage.
  bool cache_render_pass = false;
  // Indicates whether there is accumulated damage from contributing render
  // surface or layer or surface quad. Not including property changes on itself.
  bool has_damage_from_contributing_content = false;

  // Generate mipmap for trilinear filtering, applied to render pass' texture.
  bool generate_mipmap = false;

  // If non-empty, the renderer should produce a copy of the render pass'
  // contents as a bitmap, and give a copy of the bitmap to each callback in
  // this list.
  std::vector<std::unique_ptr<CopyOutputRequest>> copy_requests;

  QuadList quad_list;
  SharedQuadStateList shared_quad_state_list;

  template <typename RenderPassType>
  static void CopyAllForTest(
      const std::vector<std::unique_ptr<RenderPassType>>& in,
      std::vector<std::unique_ptr<RenderPassType>>* out) {
    for (const auto& source : in)
      out->push_back(source->DeepCopy());
  }

 protected:
  RenderPassInternal();
  explicit RenderPassInternal(size_t num_layers);
  RenderPassInternal(size_t shared_quad_state_list_size, size_t quad_list_size);

  ~RenderPassInternal();

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderPassInternal);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_INTERNAL_H_
