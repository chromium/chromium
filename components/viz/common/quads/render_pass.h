// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_H_
#define COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_H_

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/hash/hash.h"
#include "base/macros.h"
#include "cc/base/list_container.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/largest_draw_quad.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace viz {
class CopyOutputRequest;
class DrawQuad;
class RenderPassDrawQuad;
class SharedQuadState;

// A list of DrawQuad objects, sorted internally in front-to-back order. To
// add a new quad drawn behind another quad, it must be placed after the other
// quad.
class VIZ_COMMON_EXPORT QuadList : public cc::ListContainer<DrawQuad> {
 public:
  QuadList();
  explicit QuadList(size_t default_size_to_reserve);

  typedef QuadList::ReverseIterator BackToFrontIterator;
  typedef QuadList::ConstReverseIterator ConstBackToFrontIterator;

  inline BackToFrontIterator BackToFrontBegin() { return rbegin(); }
  inline BackToFrontIterator BackToFrontEnd() { return rend(); }
  inline ConstBackToFrontIterator BackToFrontBegin() const { return rbegin(); }
  inline ConstBackToFrontIterator BackToFrontEnd() const { return rend(); }

  // This function is used by overlay algorithm to fill the backbuffer with
  // transparent black.
  void ReplaceExistingQuadWithOpaqueTransparentSolidColor(Iterator at);
};

using SharedQuadStateList = cc::ListContainer<SharedQuadState>;

using RenderPassId = uint64_t;

class VIZ_COMMON_EXPORT RenderPass {
 public:
  ~RenderPass();

  static std::unique_ptr<RenderPass> Create();
  static std::unique_ptr<RenderPass> Create(size_t num_layers);
  static std::unique_ptr<RenderPass> Create(size_t shared_quad_state_list_size,
                                            size_t quad_list_size);

  // A shallow copy of the render pass, which does not include its quads or copy
  // requests.
  std::unique_ptr<RenderPass> Copy(int new_id) const;

  // A deep copy of the render pass that includes quads.
  std::unique_ptr<RenderPass> DeepCopy() const;

  // A deep copy of the render passes in the list including the quads.
  static void CopyAll(const std::vector<std::unique_ptr<RenderPass>>& in,
                      std::vector<std::unique_ptr<RenderPass>>* out);

  void SetNew(RenderPassId id,
              const gfx::Rect& output_rect,
              const gfx::Rect& damage_rect,
              const gfx::Transform& transform_to_root_target);

  void SetAll(RenderPassId id,
              const gfx::Rect& output_rect,
              const gfx::Rect& damage_rect,
              const gfx::Transform& transform_to_root_target,
              const cc::FilterOperations& filters,
              const cc::FilterOperations& backdrop_filters,
              const base::Optional<gfx::RRectF>& backdrop_filter_bounds,
              const gfx::ColorSpace& color_space,
              bool has_transparent_background,
              bool cache_render_pass,
              bool has_damage_from_contributing_content,
              bool generate_mipmap);

  void AsValueInto(base::trace_event::TracedValue* dict) const;

  SharedQuadState* CreateAndAppendSharedQuadState();

  template <typename DrawQuadType>
  DrawQuadType* CreateAndAppendDrawQuad() {
    return quad_list.AllocateAndConstruct<DrawQuadType>();
  }

  RenderPassDrawQuad* CopyFromAndAppendRenderPassDrawQuad(
      const RenderPassDrawQuad* quad,
      RenderPassId render_pass_id);
  DrawQuad* CopyFromAndAppendDrawQuad(const DrawQuad* quad);

  // Uniquely identifies the render pass in the compositor's current frame.
  RenderPassId id = 0;

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

  // The color space into which content will be rendered for this render pass.
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

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

 protected:
  explicit RenderPass(size_t num_layers);
  RenderPass();
  RenderPass(size_t shared_quad_state_list_size, size_t quad_list_size);

 private:
  template <typename DrawQuadType>
  DrawQuadType* CopyFromAndAppendTypedDrawQuad(const DrawQuad* quad) {
    return quad_list.AllocateAndCopyFrom(DrawQuadType::MaterialCast(quad));
  }

  DISALLOW_COPY_AND_ASSIGN(RenderPass);
};

using RenderPassList = std::vector<std::unique_ptr<RenderPass>>;

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_H_
