// Copyright 2021 The Chromium Authors
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
  if (auto* texture_quad = quad->DynamicCast<TextureDrawQuad>()) {
    return texture_quad->damage_rect;
  } else if (auto* yuv_video_quad = quad->DynamicCast<YUVVideoDrawQuad>()) {
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

ResolvedFrameData::ResolvedFrameData(DisplayResourceProvider* resource_provider,
                                     Surface* surface,
                                     uint64_t previous_frame_index)
    : resource_provider_(resource_provider),
      surface_id_(surface->surface_id()),
      surface_(surface),
      previous_frame_index_(previous_frame_index) {
  DCHECK(resource_provider_);
  DCHECK(surface_);

  RegisterWithResourceProvider();
}

ResolvedFrameData::~ResolvedFrameData() {
  // Release resources used by this ResolvedFrameData.
  resource_provider_->DestroyChild(child_resource_id_);
}

void ResolvedFrameData::SetFullDamageForNextAggregation() {
  previous_frame_index_ = kInvalidFrameIndex;
}

uint32_t ResolvedFrameData::GetClientNamespaceId() const {
  return static_cast<uint32_t>(child_resource_id_);
}

void ResolvedFrameData::ForceReleaseResource() {
  // Resources for future frames are stored under a new child id going forward.
  resource_provider_->DestroyChild(child_resource_id_);
  RegisterWithResourceProvider();
}

void ResolvedFrameData::UpdateForActiveFrame(
    AggregatedRenderPassId::Generator& render_pass_id_generator) {
  auto& compositor_frame = surface_->GetActiveOrInterpolatedFrame();
  auto& resource_list = compositor_frame.resource_list;
  auto& render_passes = compositor_frame.render_pass_list;
  size_t num_render_pass = render_passes.size();
  DCHECK(!render_passes.empty());

  resource_provider_->ReceiveFromChild(child_resource_id_, resource_list);

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

  auto& child_to_parent_map =
      resource_provider_->GetChildToParentMap(child_resource_id_);

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

    // Loop through the quads, remapping resource ids and storing them.
    auto& draw_quads = fixed.draw_quads;
    draw_quads.reserve(render_pass->quad_list.size());
    for (auto* quad : render_pass->quad_list) {
      if (render_pass->has_per_quad_damage) {
        auto optional_damage = GetOptionalDamageRectFromQuad(quad);
        if (optional_damage.has_value()) {
          fixed.prewalk_quads.push_back(quad);
        }
      }

      if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
        // Check CompositorRenderPassDrawQuad refers to a render pass
        // that exists and is drawn before the current render pass.
        auto quad_render_pass_id =
            CompositorRenderPassDrawQuad::MaterialCast(quad)->render_pass_id;
        auto iter = render_pass_id_map_.find(quad_render_pass_id);
        if (iter == render_pass_id_map_.end()) {
          DLOG(ERROR) << "CompositorRenderPassDrawQuad with invalid id";
          SetInvalid();
          return;
        }

        ++iter->second->fixed_.embed_count;
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
          return;
        }

        referenced_resources.push_back(resource_id);

        // Update `ResolvedQuadData::remapped_resources` to have the remapped
        // display resource_id.
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
      return;
    }
  }

  frame_index_ = surface_->GetActiveFrameIndex();
  DCHECK_NE(frame_index_, 0u);

  // Clear id mappings that weren't used in this frame.
  base::EraseIf(aggregated_id_map_, [this](auto& entry) {
    return render_pass_id_map_.find(entry.first) == render_pass_id_map_.end();
  });

  valid_ = true;

  // Declare the used resources to the provider. This will cause all resources
  // that were received but not used in the render passes to be unreferenced in
  // the surface, and returned to the child in the resource provider.
  resource_provider_->DeclareUsedResourcesFromChild(
      child_resource_id_, ResourceIdSet(std::move(referenced_resources)));
}

void ResolvedFrameData::SetInvalid() {
  frame_index_ = surface_->GetActiveFrameIndex();
  render_pass_id_map_.clear();
  resolved_passes_.clear();
  valid_ = false;
}

void ResolvedFrameData::MarkAsUsedInAggregation() {
  used_in_aggregation_ = true;
}

bool ResolvedFrameData::WasUsedInAggregation() const {
  return used_in_aggregation_;
}

void ResolvedFrameData::ResetAfterAggregation() {
  // Reset aggregation scoped data.
  for (auto& resolved_pass : resolved_passes_)
    resolved_pass.aggregation().Reset();

  previous_frame_index_ = frame_index_;
  used_in_aggregation_ = false;
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

FrameDamageType ResolvedFrameData::GetFrameDamageType() const {
  DCHECK(valid_);
  DCHECK(used_in_aggregation_);

  if (previous_frame_index_ == frame_index_) {
    // This is the same frame as the one used last aggregation.
    return FrameDamageType::kNone;
  } else if (previous_frame_index_ > kInvalidFrameIndex &&
             frame_index_ == previous_frame_index_ + 1) {
    // This is the next frame after the one used last aggregation.
    return FrameDamageType::kFrame;
  }

  return FrameDamageType::kFull;
}

gfx::Rect ResolvedFrameData::GetSurfaceDamage() const {
  switch (GetFrameDamageType()) {
    case FrameDamageType::kFull:
      return GetOutputRect();
    case FrameDamageType::kFrame:
      return resolved_passes_.back().render_pass().damage_rect;
    case FrameDamageType::kNone:
      return gfx::Rect();
  }
}

const gfx::Rect& ResolvedFrameData::GetOutputRect() const {
  DCHECK(valid_);
  return resolved_passes_.back().render_pass().output_rect;
}

void ResolvedFrameData::RegisterWithResourceProvider() {
  child_resource_id_ = resource_provider_->CreateChild(
      base::BindRepeating(&SurfaceClient::UnrefResources, surface_->client()),
      surface_id_);
}

}  // namespace viz
