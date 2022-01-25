// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/resolved_frame_data.h"

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/service/surfaces/surface.h"

namespace viz {

const absl::optional<gfx::Rect>& GetOptionalDamageRectFromQuad(
    const DrawQuad* quad) {
  if (quad->material == DrawQuad::Material::kTextureContent) {
    auto* texture_quad = TextureDrawQuad::MaterialCast(quad);
    return texture_quad->damage_rect;
  } else if (quad->material == DrawQuad::Material::kYuvVideoContent) {
    auto* yuv_video_quad = YUVVideoDrawQuad::MaterialCast(quad);
    return yuv_video_quad->damage_rect;
  } else {
    static absl::optional<gfx::Rect> no_damage;
    return no_damage;
  }
}

ResolvedQuadData::ResolvedQuadData(const DrawQuad& quad)
    : remapped_resources(quad.resources) {}

FixedPassData::FixedPassData() = default;
FixedPassData::FixedPassData(FixedPassData&& other) = default;
FixedPassData& FixedPassData::operator=(FixedPassData&& other) = default;
FixedPassData::~FixedPassData() = default;

AggregationPassData::AggregationPassData() = default;
AggregationPassData::AggregationPassData(AggregationPassData&& other) = default;
AggregationPassData& AggregationPassData::operator=(
    AggregationPassData&& other) = default;
AggregationPassData::~AggregationPassData() = default;

void AggregationPassData::Reset() {
  *this = AggregationPassData();
}

ResolvedPassData::ResolvedPassData(FixedPassData fixed_data)
    : fixed_(std::move(fixed_data)) {}
ResolvedPassData::~ResolvedPassData() = default;
ResolvedPassData::ResolvedPassData(ResolvedPassData&& other) = default;
ResolvedPassData& ResolvedPassData::operator=(ResolvedPassData&& other) =
    default;

ResolvedFrameData::ResolvedFrameData(const SurfaceId& surface_id,
                                     Surface* surface)
    : surface_id_(surface_id), surface_(surface) {}

ResolvedFrameData::~ResolvedFrameData() = default;

ResourceIdSet ResolvedFrameData::UpdateForActiveFrame(
    const std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>&
        child_to_parent_map,
    AggregatedRenderPassId::Generator& render_pass_id_generator) {
  auto& compositor_frame = surface_->GetActiveOrInterpolatedFrame();
  auto& resource_list = compositor_frame.resource_list;
  auto& render_passes = compositor_frame.render_pass_list;
  size_t num_render_pass = render_passes.size();
  DCHECK(!render_passes.empty());

  // Figure out which resources are actually used in the render pass.
  // Note that we first gather them in a vector, since ResourceIdSet (which we
  // actually need) is a flat_set, which means bulk insertion we do at the end
  // is more efficient.
  std::vector<ResourceId> referenced_resources;
  referenced_resources.reserve(resource_list.size());

  // Will be repopulated based on active frame.
  render_pass_id_map_.clear();
  resolved_passes_.clear();
  render_pass_id_map_.reserve(num_render_pass);
  resolved_passes_.reserve(num_render_pass);

  root_damage_rect_ = render_passes.back()->damage_rect;

  // Reset and compute new render pass / quad data for this frame. This stores
  // remapped display resource ids.
  for (size_t i = 0; i < num_render_pass; ++i) {
    auto& render_pass = render_passes[i];

    FixedPassData fixed;

    fixed.render_pass = render_pass.get();

    AggregatedRenderPassId& remapped_id = aggregated_id_map_[render_pass->id];
    if (remapped_id.is_null()) {
      remapped_id = render_pass_id_generator.GenerateNextId();
    }
    fixed.remapped_id = remapped_id;
    fixed.is_root = i == num_render_pass - 1;

    bool add_quad_damage_to_root_damage_rect =
        fixed.is_root && render_pass->has_per_quad_damage;

    // Loop through the quads, remapping resource ids and storing them.
    auto& draw_quads = fixed.draw_quads;
    draw_quads.reserve(render_pass->quad_list.size());
    for (auto* quad : render_pass->quad_list) {
      if (add_quad_damage_to_root_damage_rect) {
        auto optional_damage = GetOptionalDamageRectFromQuad(quad);
        if (optional_damage.has_value()) {
          root_damage_rect_.Union(optional_damage.value());
        }
      }

      if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
        // Check CompositorRenderPassDrawQuad refers to a render pass
        // that exists and is drawn before the current render pass.
        auto quad_render_pass_id =
            CompositorRenderPassDrawQuad::MaterialCast(quad)->render_pass_id;
        if (!base::Contains(render_pass_id_map_, quad_render_pass_id)) {
          DLOG(ERROR) << "CompositorRenderPassDrawQuad with invalid id";
          SetInvalid();
          return {};
        }

        fixed.prewalk_quads.push_back(quad);
      } else if (quad->material == DrawQuad::Material::kSurfaceContent) {
        fixed.prewalk_quads.push_back(quad);
      }

      draw_quads.emplace_back(*quad);
      for (ResourceId& resource_id : draw_quads.back().remapped_resources) {
        // If we're using a resource which was not declared in the
        // |resource_list| then this is an invalid frame, we can abort.
        auto iter = child_to_parent_map.find(resource_id);
        if (iter == child_to_parent_map.end()) {
          DLOG(ERROR) << "Invalid resource for " << surface_id();
          SetInvalid();
          return {};
        }

        referenced_resources.push_back(resource_id);
        resource_id = iter->second;
      }
    }

    resolved_passes_.emplace_back(std::move(fixed));

    // Build render pass id map and check for duplicate ids at the same time.
    if (!render_pass_id_map_
             .insert(std::make_pair(render_pass->id, &resolved_passes_.back()))
             .second) {
      DLOG(ERROR) << "Duplicate render pass ids";
      SetInvalid();
      return {};
    }
  }

  frame_index_ = surface_->GetActiveFrameIndex();
  DCHECK_NE(frame_index_, 0u);

  // Clear id mappings that weren't used in this frame.
  base::EraseIf(aggregated_id_map_, [this](auto& entry) {
    return render_pass_id_map_.find(entry.first) == render_pass_id_map_.end();
  });

  valid_ = true;
  return ResourceIdSet(std::move(referenced_resources));
}

void ResolvedFrameData::SetInvalid() {
  frame_index_ = surface_->GetActiveFrameIndex();
  render_pass_id_map_.clear();
  resolved_passes_.clear();
  valid_ = false;
}

bool ResolvedFrameData::MarkAsUsed() {
  // Returns true the first time this is called after reset.
  return !std::exchange(used_, true);
}

bool ResolvedFrameData::CheckIfUsedAndReset() {
  // Reset aggregation scoped data.
  for (auto& resolved_pass : resolved_passes_)
    resolved_pass.aggregation().Reset();

  return std::exchange(used_, false);
}

bool ResolvedFrameData::WillDraw() const {
  return GetRootRenderPassData().aggregation().will_draw;
}

ResolvedPassData& ResolvedFrameData::GetRenderPassDataById(
    CompositorRenderPassId render_pass_id) {
  return const_cast<ResolvedPassData&>(
      const_cast<const ResolvedFrameData*>(this)->GetRenderPassDataById(
          render_pass_id));
}

const ResolvedPassData& ResolvedFrameData::GetRenderPassDataById(
    CompositorRenderPassId render_pass_id) const {
  DCHECK(valid_);

  auto iter = render_pass_id_map_.find(render_pass_id);
  DCHECK(iter != render_pass_id_map_.end());
  return *iter->second;
}

ResolvedPassData& ResolvedFrameData::GetRootRenderPassData() {
  DCHECK(valid_);
  return resolved_passes_.back();
}

const ResolvedPassData& ResolvedFrameData::GetRootRenderPassData() const {
  DCHECK(valid_);
  return resolved_passes_.back();
}

const gfx::Rect& ResolvedFrameData::GetDamageRect(
    bool include_per_quad_damage) const {
  DCHECK(valid_);

  if (include_per_quad_damage)
    return root_damage_rect_;

  return resolved_passes_.back().render_pass().damage_rect;
}

const gfx::Rect& ResolvedFrameData::GetOutputRect() const {
  DCHECK(valid_);
  return resolved_passes_.back().render_pass().output_rect;
}

}  // namespace viz
