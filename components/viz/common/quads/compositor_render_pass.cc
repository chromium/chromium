// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_render_pass.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/largest_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/traced_value.h"

namespace viz {

std::unique_ptr<CompositorRenderPass> CompositorRenderPass::Create() {
  return base::WrapUnique(new CompositorRenderPass());
}

std::unique_ptr<CompositorRenderPass> CompositorRenderPass::Create(
    size_t num_layers) {
  return base::WrapUnique(new CompositorRenderPass(num_layers));
}

std::unique_ptr<CompositorRenderPass> CompositorRenderPass::Create(
    size_t shared_quad_state_list_size,
    size_t quad_list_size) {
  return base::WrapUnique(
      new CompositorRenderPass(shared_quad_state_list_size, quad_list_size));
}

CompositorRenderPass::CompositorRenderPass() = default;
CompositorRenderPass::CompositorRenderPass(size_t num_layers)
    : RenderPassInternal(num_layers) {}
CompositorRenderPass::CompositorRenderPass(size_t shared_quad_state_list_size,
                                           size_t quad_list_size)
    : RenderPassInternal(shared_quad_state_list_size, quad_list_size) {}

CompositorRenderPass::~CompositorRenderPass() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("viz.quads"), "CompositorRenderPass",
      reinterpret_cast<void*>(static_cast<uint64_t>(id)));
}

void CompositorRenderPass::SetNew(
    CompositorRenderPassId pass_id,
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect,
    const gfx::Transform& transform_to_root_target) {
  DCHECK(pass_id);
  DCHECK(damage_rect.IsEmpty() || output_rect.Contains(damage_rect))
      << "damage_rect: " << damage_rect.ToString()
      << " output_rect: " << output_rect.ToString();

  id = pass_id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

void CompositorRenderPass::SetAll(
    CompositorRenderPassId pass_id,
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect,
    const gfx::Transform& transform_to_root_target,
    const cc::FilterOperations& filters,
    const cc::FilterOperations& backdrop_filters,
    const std::optional<gfx::RRectF>& backdrop_filter_bounds,
    SubtreeCaptureId capture_id,
    gfx::Size subtree_capture_size,
    ViewTransitionElementResourceId resource_id,
    bool has_transparent_background,
    bool cache_render_pass,
    bool has_damage_from_contributing_content,
    bool generate_mipmap,
    bool per_quad_damage) {
  DCHECK(pass_id);

  id = pass_id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;
  this->filters = filters;
  this->backdrop_filters = backdrop_filters;
  this->backdrop_filter_bounds = backdrop_filter_bounds;
  this->subtree_capture_id = capture_id;
  this->subtree_size = subtree_capture_size;
  this->view_transition_element_resource_id = resource_id;
  this->has_transparent_background = has_transparent_background;
  this->cache_render_pass = cache_render_pass;
  this->has_damage_from_contributing_content =
      has_damage_from_contributing_content;
  this->generate_mipmap = generate_mipmap;
  has_per_quad_damage = per_quad_damage;
  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

void CompositorRenderPass::AsValueInto(
    base::trace_event::TracedValue* value) const {
  RenderPassInternal::AsValueInto(value);

  value->SetString("subtree_capture_id", subtree_capture_id.ToString());
  cc::MathUtil::AddToTracedValue("subtree_size", subtree_size, value);

  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("viz.quads"), value, "CompositorRenderPass",
      reinterpret_cast<void*>(static_cast<uint64_t>(id)));
}

CompositorRenderPassDrawQuad*
CompositorRenderPass::CopyFromAndAppendRenderPassDrawQuad(
    const CompositorRenderPassDrawQuad* quad,
    CompositorRenderPassId render_pass_id) {
  DCHECK(!shared_quad_state_list.empty());
  auto* copy_quad = quad_list.AllocateAndCopyFrom(quad);
  copy_quad->shared_quad_state = shared_quad_state_list.back();
  copy_quad->render_pass_id = render_pass_id;
  return copy_quad;
}

DrawQuad* CompositorRenderPass::CopyFromAndAppendDrawQuad(
    const DrawQuad* quad) {
  DCHECK(!shared_quad_state_list.empty());
  switch (quad->material) {
    case DrawQuad::Material::kDebugBorder:
      quad_list.AllocateAndCopyFrom(DebugBorderDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kPictureContent:
      quad_list.AllocateAndCopyFrom(PictureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kTextureContent:
      quad_list.AllocateAndCopyFrom(TextureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kSolidColor:
      quad_list.AllocateAndCopyFrom(SolidColorDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kTiledContent:
      quad_list.AllocateAndCopyFrom(TileDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kSurfaceContent:
      quad_list.AllocateAndCopyFrom(SurfaceDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kVideoHole:
      quad_list.AllocateAndCopyFrom(VideoHoleDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kSharedElement:
      quad_list.AllocateAndCopyFrom(SharedElementDrawQuad::MaterialCast(quad));
      break;
    // RenderPass quads need to use specific CopyFrom function.
    case DrawQuad::Material::kAggregatedRenderPass:
    case DrawQuad::Material::kCompositorRenderPass:
    case DrawQuad::Material::kInvalid:
      // TODO(danakj): Why is this a check instead of dcheck, and validate from
      // IPC?
      CHECK(false);  // Invalid DrawQuad material.
      break;
  }
  quad_list.back()->shared_quad_state = shared_quad_state_list.back();
  return quad_list.back();
}

std::unique_ptr<CompositorRenderPass> CompositorRenderPass::DeepCopy() const {
  // Since we can't copy these, it's wrong to use DeepCopy in a situation where
  // you may have copy_requests present.
  DCHECK_EQ(copy_requests.size(), 0u);

  auto copy_pass = CompositorRenderPass::Create(shared_quad_state_list.size(),
                                                quad_list.size());
  copy_pass->SetAll(id, output_rect, damage_rect, transform_to_root_target,
                    filters, backdrop_filters, backdrop_filter_bounds,
                    subtree_capture_id, subtree_size,
                    view_transition_element_resource_id,
                    has_transparent_background, cache_render_pass,
                    has_damage_from_contributing_content, generate_mipmap,
                    has_per_quad_damage);

  if (shared_quad_state_list.empty()) {
    DCHECK(quad_list.empty());
    return copy_pass;
  }

  SharedQuadStateList::ConstIterator sqs_iter = shared_quad_state_list.begin();
  SharedQuadState* copy_shared_quad_state =
      copy_pass->CreateAndAppendSharedQuadState();
  *copy_shared_quad_state = **sqs_iter;
  for (auto* quad : quad_list) {
    while (quad->shared_quad_state != *sqs_iter) {
      ++sqs_iter;
      DCHECK(sqs_iter != shared_quad_state_list.end());
      copy_shared_quad_state = copy_pass->CreateAndAppendSharedQuadState();
      *copy_shared_quad_state = **sqs_iter;
    }
    DCHECK(quad->shared_quad_state == *sqs_iter);

    if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
      const auto* pass_quad = CompositorRenderPassDrawQuad::MaterialCast(quad);
      copy_pass->CopyFromAndAppendRenderPassDrawQuad(pass_quad,
                                                     pass_quad->render_pass_id);
    } else {
      copy_pass->CopyFromAndAppendDrawQuad(quad);
    }
  }
  return copy_pass;
}

}  // namespace viz
