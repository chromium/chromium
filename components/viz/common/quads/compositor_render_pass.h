// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_RENDER_PASS_H_
#define COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_RENDER_PASS_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/hash/hash.h"
#include "base/macros.h"
#include "base/util/type_safety/id_type.h"
#include "cc/base/list_container.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/largest_draw_quad.h"
#include "components/viz/common/quads/quad_list.h"
#include "components/viz/common/quads/render_pass_internal.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace viz {
class AggregatedRenderPass;
class AggregatedRenderPassDrawQuad;
class DrawQuad;
class CompositorRenderPass;
class CompositorRenderPassDrawQuad;

using CompositorRenderPassId = util::IdTypeU64<CompositorRenderPass>;

// This class represents a render pass that is submitted from the UI or renderer
// compositor to viz. It is mojo-serializable and typically has a unique
// CompositorRenderPassId within its surface id.
class VIZ_COMMON_EXPORT CompositorRenderPass : public RenderPassInternal {
 public:
  ~CompositorRenderPass();

  static std::unique_ptr<CompositorRenderPass> Create();
  static std::unique_ptr<CompositorRenderPass> Create(size_t num_layers);
  static std::unique_ptr<CompositorRenderPass> Create(
      size_t shared_quad_state_list_size,
      size_t quad_list_size);

  void SetNew(CompositorRenderPassId id,
              const gfx::Rect& output_rect,
              const gfx::Rect& damage_rect,
              const gfx::Transform& transform_to_root_target);

  void SetAll(CompositorRenderPassId id,
              const gfx::Rect& output_rect,
              const gfx::Rect& damage_rect,
              const gfx::Transform& transform_to_root_target,
              const cc::FilterOperations& filters,
              const cc::FilterOperations& backdrop_filters,
              const base::Optional<gfx::RRectF>& backdrop_filter_bounds,
              bool has_transparent_background,
              bool cache_render_pass,
              bool has_damage_from_contributing_content,
              bool generate_mipmap);

  void AsValueInto(base::trace_event::TracedValue* dict) const;

  template <typename DrawQuadType>
  DrawQuadType* CreateAndAppendDrawQuad() {
    static_assert(
        !std::is_same<DrawQuadType, AggregatedRenderPassDrawQuad>::value,
        "cannot create CompositorRenderPassDrawQuad in AggregatedRenderPass");
    return quad_list.AllocateAndConstruct<DrawQuadType>();
  }

  // Uniquely identifies the render pass in the compositor's current frame.
  CompositorRenderPassId id;

  // For testing functions.
  // TODO(vmpstr): See if we can clean these up by moving the tests to use
  // AggregatedRenderPasses where appropriate.
  CompositorRenderPassDrawQuad* CopyFromAndAppendRenderPassDrawQuad(
      const CompositorRenderPassDrawQuad* quad,
      CompositorRenderPassId render_pass_id);
  DrawQuad* CopyFromAndAppendDrawQuad(const DrawQuad* quad);

  // A deep copy of the render pass that includes quads.
  std::unique_ptr<CompositorRenderPass> DeepCopy() const;

 protected:
  // This is essentially "using RenderPassInternal::RenderPassInternal", but
  // since that generates inline (complex) ctors, the chromium-style plug-in
  // refuses to compile it.
  CompositorRenderPass();
  explicit CompositorRenderPass(size_t num_layers);
  CompositorRenderPass(size_t shared_quad_state_list_size,
                       size_t quad_list_size);

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorRenderPass);
};

using CompositorRenderPassList =
    std::vector<std::unique_ptr<CompositorRenderPass>>;

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_RENDER_PASS_H_
