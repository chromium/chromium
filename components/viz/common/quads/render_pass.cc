// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/render_pass.h"

#include <stddef.h>

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/largest_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/traced_value.h"

namespace {
const size_t kDefaultNumSharedQuadStatesToReserve = 32;
const size_t kDefaultNumQuadsToReserve = 128;
}  // namespace

namespace viz {

QuadList::QuadList()
    : ListContainer<DrawQuad>(LargestDrawQuadAlignment(),
                              LargestDrawQuadSize(),
                              kDefaultNumSharedQuadStatesToReserve) {}

QuadList::QuadList(size_t default_size_to_reserve)
    : ListContainer<DrawQuad>(LargestDrawQuadAlignment(),
                              LargestDrawQuadSize(),
                              default_size_to_reserve) {}

void QuadList::ReplaceExistingQuadWithOpaqueTransparentSolidColor(Iterator at) {
  // In order to fill the backbuffer with transparent black, the replacement
  // solid color quad needs to set |needs_blending| to false, and
  // ShouldDrawWithBlending() returns false so it is drawn without blending.
  const gfx::Rect rect = at->rect;
  bool needs_blending = false;
  const SharedQuadState* shared_quad_state = at->shared_quad_state;

  auto* replacement = QuadList::ReplaceExistingElement<SolidColorDrawQuad>(at);
  replacement->SetAll(shared_quad_state, rect, rect /* visible_rect */,
                      needs_blending, SK_ColorTRANSPARENT, true);
}

std::unique_ptr<RenderPass> RenderPass::Create() {
  return base::WrapUnique(new RenderPass());
}

std::unique_ptr<RenderPass> RenderPass::Create(size_t num_layers) {
  return base::WrapUnique(new RenderPass(num_layers));
}

std::unique_ptr<RenderPass> RenderPass::Create(
    size_t shared_quad_state_list_size,
    size_t quad_list_size) {
  return base::WrapUnique(
      new RenderPass(shared_quad_state_list_size, quad_list_size));
}

RenderPass::RenderPass()
    : quad_list(kDefaultNumQuadsToReserve),
      shared_quad_state_list(alignof(SharedQuadState),
                             sizeof(SharedQuadState),
                             kDefaultNumSharedQuadStatesToReserve) {}

// Each layer usually produces one shared quad state, so the number of layers
// is a good hint for what to reserve here.
RenderPass::RenderPass(size_t num_layers)
    : has_transparent_background(true),
      cache_render_pass(false),
      has_damage_from_contributing_content(false),
      quad_list(kDefaultNumQuadsToReserve),
      shared_quad_state_list(alignof(SharedQuadState),
                             sizeof(SharedQuadState),
                             num_layers) {}

RenderPass::RenderPass(size_t shared_quad_state_list_size,
                       size_t quad_list_size)
    : has_transparent_background(true),
      cache_render_pass(false),
      has_damage_from_contributing_content(false),
      quad_list(quad_list_size),
      shared_quad_state_list(alignof(SharedQuadState),
                             sizeof(SharedQuadState),
                             shared_quad_state_list_size) {}

RenderPass::~RenderPass() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
                                     "RenderPass", reinterpret_cast<void*>(id));
}

std::unique_ptr<RenderPass> RenderPass::Copy(int new_id) const {
  std::unique_ptr<RenderPass> copy_pass(
      Create(shared_quad_state_list.size(), quad_list.size()));
  copy_pass->SetAll(new_id, output_rect, damage_rect, transform_to_root_target,
                    filters, backdrop_filters, backdrop_filter_bounds,
                    color_space, has_transparent_background, cache_render_pass,
                    has_damage_from_contributing_content, generate_mipmap);
  return copy_pass;
}

std::unique_ptr<RenderPass> RenderPass::DeepCopy() const {
  // Since we can't copy these, it's wrong to use DeepCopy in a situation where
  // you may have copy_requests present.
  DCHECK_EQ(copy_requests.size(), 0u);

  std::unique_ptr<RenderPass> copy_pass(
      Create(shared_quad_state_list.size(), quad_list.size()));
  copy_pass->SetAll(id, output_rect, damage_rect, transform_to_root_target,
                    filters, backdrop_filters, backdrop_filter_bounds,
                    color_space, has_transparent_background, cache_render_pass,
                    has_damage_from_contributing_content, generate_mipmap);

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

    if (quad->material == DrawQuad::Material::kRenderPass) {
      const RenderPassDrawQuad* pass_quad =
          RenderPassDrawQuad::MaterialCast(quad);
      copy_pass->CopyFromAndAppendRenderPassDrawQuad(pass_quad,
                                                     pass_quad->render_pass_id);
    } else {
      copy_pass->CopyFromAndAppendDrawQuad(quad);
    }
  }
  return copy_pass;
}

// static
void RenderPass::CopyAll(const std::vector<std::unique_ptr<RenderPass>>& in,
                         std::vector<std::unique_ptr<RenderPass>>* out) {
  for (const auto& source : in)
    out->push_back(source->DeepCopy());
}

void RenderPass::SetNew(uint64_t id,
                        const gfx::Rect& output_rect,
                        const gfx::Rect& damage_rect,
                        const gfx::Transform& transform_to_root_target) {
  DCHECK(id);
  DCHECK(damage_rect.IsEmpty() || output_rect.Contains(damage_rect))
      << "damage_rect: " << damage_rect.ToString()
      << " output_rect: " << output_rect.ToString();

  this->id = id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
}

void RenderPass::SetAll(
    uint64_t id,
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
    bool generate_mipmap) {
  DCHECK(id);

  this->id = id;
  this->output_rect = output_rect;
  this->damage_rect = damage_rect;
  this->transform_to_root_target = transform_to_root_target;
  this->filters = filters;
  this->backdrop_filters = backdrop_filters;
  this->backdrop_filter_bounds = backdrop_filter_bounds;
  this->color_space = color_space;
  this->has_transparent_background = has_transparent_background;
  this->cache_render_pass = cache_render_pass;
  this->has_damage_from_contributing_content =
      has_damage_from_contributing_content;
  this->generate_mipmap = generate_mipmap;

  DCHECK(quad_list.empty());
  DCHECK(shared_quad_state_list.empty());
  DCHECK(color_space.IsValid());
}

void RenderPass::AsValueInto(base::trace_event::TracedValue* value) const {
  cc::MathUtil::AddToTracedValue("output_rect", output_rect, value);
  cc::MathUtil::AddToTracedValue("damage_rect", damage_rect, value);

  value->SetBoolean("has_transparent_background", has_transparent_background);
  value->SetBoolean("cache_render_pass", cache_render_pass);
  value->SetBoolean("has_damage_from_contributing_content",
                    has_damage_from_contributing_content);
  value->SetBoolean("generate_mipmap", generate_mipmap);
  value->SetInteger("copy_requests",
                    base::saturated_cast<int>(copy_requests.size()));

  value->BeginArray("filters");
  filters.AsValueInto(value);
  value->EndArray();

  value->BeginArray("backdrop_filters");
  backdrop_filters.AsValueInto(value);
  value->EndArray();

  if (backdrop_filter_bounds.has_value()) {
    cc::MathUtil::AddToTracedValue("backdrop_filter_bounds",
                                   backdrop_filter_bounds.value(), value);
  }

  value->BeginArray("shared_quad_state_list");
  for (auto* shared_quad_state : shared_quad_state_list) {
    value->BeginDictionary();
    shared_quad_state->AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();

  value->BeginArray("quad_list");
  for (auto* quad : quad_list) {
    value->BeginDictionary();
    quad->AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();

  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("viz.quads"), value, "RenderPass",
      reinterpret_cast<void*>(id));
}

SharedQuadState* RenderPass::CreateAndAppendSharedQuadState() {
  return shared_quad_state_list.AllocateAndConstruct<SharedQuadState>();
}

RenderPassDrawQuad* RenderPass::CopyFromAndAppendRenderPassDrawQuad(
    const RenderPassDrawQuad* quad,
    RenderPassId render_pass_id) {
  DCHECK(!shared_quad_state_list.empty());
  auto* copy_quad = CopyFromAndAppendTypedDrawQuad<RenderPassDrawQuad>(quad);
  copy_quad->shared_quad_state = shared_quad_state_list.back();
  copy_quad->render_pass_id = render_pass_id;
  return copy_quad;
}

DrawQuad* RenderPass::CopyFromAndAppendDrawQuad(const DrawQuad* quad) {
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
    case DrawQuad::Material::kStreamVideoContent:
      CopyFromAndAppendTypedDrawQuad<StreamVideoDrawQuad>(quad);
      break;
    case DrawQuad::Material::kSurfaceContent:
      CopyFromAndAppendTypedDrawQuad<SurfaceDrawQuad>(quad);
      break;
    case DrawQuad::Material::kVideoHole:
      CopyFromAndAppendTypedDrawQuad<VideoHoleDrawQuad>(quad);
      break;
    case DrawQuad::Material::kYuvVideoContent:
      CopyFromAndAppendTypedDrawQuad<YUVVideoDrawQuad>(quad);
      break;
    // RenderPass quads need to use specific CopyFrom function.
    case DrawQuad::Material::kRenderPass:
    case DrawQuad::Material::kInvalid:
      // TODO(danakj): Why is this a check instead of dcheck, and validate from
      // IPC?
      CHECK(false);  // Invalid DrawQuad material.
      break;
  }
  quad_list.back()->shared_quad_state = shared_quad_state_list.back();
  return quad_list.back();
}

}  // namespace viz
