// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/aggregated_render_pass.h"

#include <unordered_map>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/traced_value.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/largest_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/traced_value.h"

namespace viz {

AggregatedRenderPass::AggregatedRenderPass() = default;
AggregatedRenderPass::~AggregatedRenderPass() = default;

AggregatedRenderPass::AggregatedRenderPass(size_t shared_quad_state_size,
                                           size_t draw_quad_size)
    : RenderPassInternal(shared_quad_state_size, draw_quad_size) {}

void AggregatedRenderPass::SetNew(
    AggregatedRenderPassId pass_id,
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

void AggregatedRenderPass::SetAll(
    AggregatedRenderPassId pass_id,
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect,
    const gfx::Transform& transform_to_root_target,
    const cc::FilterOperations& pass_filters,
    const cc::FilterOperations& pass_backdrop_filters,
    const std::optional<SkPath>& pass_backdrop_filter_bounds,
    gfx::ContentColorUsage color_usage,
    bool has_transparent_background,
    bool cache_render_pass,
    bool has_damage_from_contributing_content,
    bool generate_mipmap) {
  DCHECK(pass_id);

  id = pass_id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;
  this->filters = pass_filters;
  this->backdrop_filters = pass_backdrop_filters;
  this->backdrop_filter_bounds = pass_backdrop_filter_bounds;
  content_color_usage = color_usage;
  this->has_transparent_background = has_transparent_background;
  this->cache_render_pass = cache_render_pass;
  this->has_damage_from_contributing_content =
      has_damage_from_contributing_content;
  this->generate_mipmap = generate_mipmap;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

AggregatedRenderPassDrawQuad*
AggregatedRenderPass::CopyFromAndAppendRenderPassDrawQuad(
    const CompositorRenderPassDrawQuad* quad,
    AggregatedRenderPassId render_pass_id) {
  DCHECK(!shared_quad_state_list.empty());
  auto* copy_quad = CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  copy_quad->SetAll(
      shared_quad_state_list.back(), quad->rect, quad->visible_rect,
      quad->needs_blending, render_pass_id, quad->mask_resource_id(),
      quad->mask_uv_rect, quad->mask_texture_size, quad->filters_scale,
      quad->filters_origin, quad->tex_coord_rect, quad->force_anti_aliasing_off,
      quad->backdrop_filter_quality, quad->intersects_damage_under);
  return copy_quad;
}

AggregatedRenderPassDrawQuad*
AggregatedRenderPass::CopyFromAndAppendRenderPassDrawQuad(
    const AggregatedRenderPassDrawQuad* quad) {
  DCHECK(!shared_quad_state_list.empty());
  auto* copy_quad =
      CopyFromAndAppendTypedDrawQuad<AggregatedRenderPassDrawQuad>(quad);
  copy_quad->shared_quad_state = shared_quad_state_list.back();
  return copy_quad;
}

DrawQuad* AggregatedRenderPass::CopyFromAndAppendDrawQuad(
    const DrawQuad* quad) {
  DCHECK(!shared_quad_state_list.empty());
  switch (quad->material) {
    case DrawQuad::Material::kDebugBorder:
      CopyFromAndAppendTypedDrawQuad<DebugBorderDrawQuad>(quad);
      break;
    case DrawQuad::Material::kPictureContent:
      CopyFromAndAppendTypedDrawQuad<PictureDrawQuad>(quad);
      break;
    case DrawQuad::Material::kTextureContent:
      CopyFromAndAppendTypedDrawQuad<TextureDrawQuad>(quad);
      break;
    case DrawQuad::Material::kSolidColor:
      CopyFromAndAppendTypedDrawQuad<SolidColorDrawQuad>(quad);
      break;
    case DrawQuad::Material::kTiledContent:
      CopyFromAndAppendTypedDrawQuad<TileDrawQuad>(quad);
      break;
    case DrawQuad::Material::kSurfaceContent:
      CopyFromAndAppendTypedDrawQuad<SurfaceDrawQuad>(quad);
      break;
    case DrawQuad::Material::kVideoHole:
      CopyFromAndAppendTypedDrawQuad<VideoHoleDrawQuad>(quad);
      break;
    case DrawQuad::Material::kSharedElement:
      NOTREACHED()
          << "Shared Element quads should be resolved before aggregation";
    // RenderPass quads need to use specific CopyFrom function.
    case DrawQuad::Material::kAggregatedRenderPass:
    case DrawQuad::Material::kCompositorRenderPass:
    case DrawQuad::Material::kInvalid:
      NOTREACHED();
  }
  quad_list.back()->shared_quad_state = shared_quad_state_list.back();
  return quad_list.back();
}

std::unique_ptr<AggregatedRenderPass> AggregatedRenderPass::Copy(
    AggregatedRenderPassId new_id) const {
  auto copy_pass = std::make_unique<AggregatedRenderPass>(
      shared_quad_state_list.size(), quad_list.size());
  copy_pass->SetAll(new_id, output_rect, damage_rect, transform_to_root_target,
                    filters, backdrop_filters, backdrop_filter_bounds,
                    content_color_usage, has_transparent_background,
                    cache_render_pass, has_damage_from_contributing_content,
                    generate_mipmap);
  return copy_pass;
}

std::unique_ptr<AggregatedRenderPass> AggregatedRenderPass::DeepCopy() const {
  // Since we can't copy these, it's wrong to use DeepCopy in a situation where
  // you may have copy_requests present.
  DCHECK_EQ(copy_requests.size(), 0u);

  auto copy_pass = std::make_unique<AggregatedRenderPass>(
      shared_quad_state_list.size(), quad_list.size());
  copy_pass->SetAll(id, output_rect, damage_rect, transform_to_root_target,
                    filters, backdrop_filters, backdrop_filter_bounds,
                    content_color_usage, has_transparent_background,
                    cache_render_pass, has_damage_from_contributing_content,
                    generate_mipmap);

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

    if (const auto* pass_quad =
            quad->DynamicCast<AggregatedRenderPassDrawQuad>()) {
      copy_pass->CopyFromAndAppendRenderPassDrawQuad(pass_quad);
    } else {
      copy_pass->CopyFromAndAppendDrawQuad(quad);
    }
  }
  return copy_pass;
}

bool AggregatedRenderPass::ShouldDrawWithBlending() const {
  for (auto* quad : quad_list) {
    if (quad->ShouldDrawWithBlending()) {
      return true;
    }
  }
  return false;
}

bool AggregatedRenderPass::HasCapture() const {
  return !copy_requests.empty() || video_capture_enabled;
}

void AggregatedRenderPass::AsValueInto(
    base::trace_event::TracedValue* value) const {
  // TODO(zmo): Improve this mapping for AggregatedFrame.
  std::unordered_map<ResourceId, size_t> resource_id_to_index_map;
  RenderPassInternal::AsValueInto(value, resource_id_to_index_map);

  value->SetInteger("content_color_usage",
                    base::to_underlying(content_color_usage));
  value->SetBoolean("is_from_surface_root_pass", is_from_surface_root_pass);
#if BUILDFLAG(IS_WIN)
  value->SetBoolean("will_backing_be_read_by_viz", will_backing_be_read_by_viz);
  value->SetBoolean("needs_synchronous_dcomp_commit",
                    needs_synchronous_dcomp_commit);
#endif
  value->SetBoolean("video_capture_enabled", video_capture_enabled);

  // id.value() is a 64-bit uint even on 32-bit architectures, so
  // using reinterpret_cast for the intentional conversion to a TracedValue::Id.
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("viz.quads"), value, "AggregatedRenderPass",
      TracedValue::Id(reinterpret_cast<void*>(id.value())));
}

}  // namespace viz
