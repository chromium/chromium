// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/resolved_frame_data.h"

#include <set>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/surfaces/surface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {
namespace {

gfx::Rect EnclosingOffsetRect(const gfx::Rect& rect, gfx::Vector2dF offset) {
  if (rect.IsEmpty()) {
    return gfx::Rect();
  }
  gfx::RectF offset_rect(rect);
  offset_rect.Offset(offset);
  return gfx::ToEnclosingRect(offset_rect);
}

}  // namespace

const std::optional<gfx::Rect>& GetOptionalDamageRectFromQuad(
    const DrawQuad* quad) {
  if (auto* texture_quad = quad->DynamicCast<TextureDrawQuad>()) {
    return texture_quad->damage_rect;
  } else {
    static std::optional<gfx::Rect> no_damage;
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

PersistentPassData::PersistentPassData() = default;
PersistentPassData::PersistentPassData(PersistentPassData&& other) = default;
PersistentPassData& PersistentPassData::operator=(PersistentPassData& other) =
    default;
PersistentPassData& PersistentPassData::operator=(
    const PersistentPassData& other) = default;
PersistentPassData& PersistentPassData::operator=(PersistentPassData&& other) =
    default;
PersistentPassData::~PersistentPassData() = default;

ResolvedPassData::ResolvedPassData(FixedPassData fixed_data)
    : fixed_(std::move(fixed_data)) {}
ResolvedPassData::~ResolvedPassData() = default;
ResolvedPassData::ResolvedPassData(ResolvedPassData&& other) = default;
ResolvedPassData& ResolvedPassData::operator=(ResolvedPassData&& other) =
    default;

const CompositorRenderPass& ResolvedPassData::render_pass() const {
  CHECK(fixed_.render_pass);
  return *fixed_.render_pass;
}

void ResolvedPassData::CopyAndResetPersistentPassData() {
  previous_persistent_data_ = current_persistent_data_;
  current_persistent_data_ = PersistentPassData();
}

void ResolvedPassData::SetCompositorRenderPass(CompositorRenderPass* pass) {
  CHECK(pass);
  CHECK_EQ(pass->id, fixed_.render_pass_id);
  fixed_.render_pass = pass;
}

void ResolvedPassData::ResetCompositorRenderPass() {
  fixed_.render_pass = nullptr;
}

ResolvedFrameData::ResolvedFrameData(DisplayResourceProvider* resource_provider,
                                     Surface* surface,
                                     uint64_t previous_frame_index,
                                     AggregatedRenderPassId prev_root_pass_id)
    : resource_provider_(resource_provider),
      surface_id_(surface->surface_id()),
      surface_(surface),
      previous_frame_index_(previous_frame_index),
      prev_root_pass_id_(prev_root_pass_id) {
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

gfx::Size ResolvedFrameData::size_in_pixels() const {
  return surface_->size_in_pixels();
}

float ResolvedFrameData::device_scale_factor() const {
  return surface_->device_scale_factor();
}

uint32_t ResolvedFrameData::GetClientNamespaceId() const {
  return static_cast<uint32_t>(child_resource_id_);
}

void ResolvedFrameData::ForceReleaseResource() {
  // Resources for future frames are stored under a new child id going forward.
  resource_provider_->DestroyChild(child_resource_id_);
  RegisterWithResourceProvider();
}

void ResolvedFrameData::UpdateForAggregation(
    AggregatedRenderPassId::Generator& render_pass_id_generator) {
  CHECK(!used_in_aggregation_);
  used_in_aggregation_ = true;

  if (previous_frame_index() != surface_->GetActiveFrameIndex()) {
    // There is a new active CompositorFrame.
    UpdateActiveFrame(render_pass_id_generator);
  } else if (is_valid()) {
    // The same active CompositorFrame as last aggregation.
    ReuseActiveFrame();
  }
}

void ResolvedFrameData::UpdateActiveFrame(
    AggregatedRenderPassId::Generator& render_pass_id_generator) {
  // If there are modified render passes they need to be rebuilt based on
  // current active CompositorFrame.
  offset_tag_render_passes_.clear();

  auto& compositor_frame = surface_->GetActiveFrame();
  auto& resource_list = compositor_frame.resource_list;

  // Ref the resources in the surface, and let the provider know we've received
  // new resources from the compositor frame.
  if (surface_->client()) {
    surface_->client()->RefResources(resource_list);
  }

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
  std::vector<ResolvedPassData> previous_resolved_passes;
  resolved_passes_.swap(previous_resolved_passes);
  render_pass_id_map_.reserve(num_render_pass);
  resolved_passes_.reserve(num_render_pass);

  auto& child_to_parent_map =
      resource_provider_->GetChildToParentMap(child_resource_id_);

  // Reset and compute new render pass / quad data for this frame. This stores
  // remapped display resource ids.
  for (size_t i = 0; i < num_render_pass; ++i) {
    auto& render_pass = render_passes[i];
    const bool is_root = i == num_render_pass - 1;

    FixedPassData fixed;

    fixed.render_pass = render_pass.get();

    AggregatedRenderPassId& remapped_id = aggregated_id_map_[render_pass->id];
    if (remapped_id.is_null()) {
      if (is_root && !prev_root_pass_id_.is_null()) {
        remapped_id = prev_root_pass_id_;
      } else {
        remapped_id = render_pass_id_generator.GenerateNextId();
      }
    }
    fixed.remapped_id = remapped_id;
    fixed.is_root = is_root;
    fixed.render_pass_id = render_pass->id;

    // Loop through the quads, remapping resource ids and storing them.
    auto& draw_quads = fixed.draw_quads;
    draw_quads.reserve(render_pass->quad_list.size());
    for (auto* quad : render_pass->quad_list) {
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

  // Get persistent_data from the previous frame to the current frame.
  MovePersistentPassDataFromPreviousFrame(previous_resolved_passes);
  previous_resolved_passes.clear();

  // Clear id mappings that weren't used in this frame.
  base::EraseIf(aggregated_id_map_, [this](auto& entry) {
    return render_pass_id_map_.find(entry.first) == render_pass_id_map_.end();
  });

  valid_ = true;

  ComputeOffsetTagContainingRects();

  // Declare the used resources to the provider. This will cause all resources
  // that were received but not used in the render passes to be unreferenced in
  // the surface, and returned to the child in the resource provider.
  resource_provider_->DeclareUsedResourcesFromChild(
      child_resource_id_, ResourceIdSet(std::move(referenced_resources)));
}

void ResolvedFrameData::UpdateOffsetTags(OffsetTagLookupFn lookup_value_fn) {
  auto& offset_tags_to_find =
      surface_->GetActiveFrameMetadata().offset_tag_definitions;

  if (offset_tags_to_find.empty() && !has_non_zero_offset_tag_value_) {
    // Early return if there were no offset tags last and this aggregation. This
    // is the common case so avoid doing any work on this path.
    return;
  }

  // Find the offset value for all defined tags first.
  has_non_zero_offset_tag_value_ = false;
  for (auto& tag_def : offset_tags_to_find) {
    auto offset = tag_def.constraints.Clamp(lookup_value_fn(tag_def));
    auto& tag_data = offset_tag_data_[tag_def.tag];
    tag_data.current_offset = offset;
    tag_data.defined_in_frame = true;
    if (!offset.IsZero()) {
      has_non_zero_offset_tag_value_ = true;
    }
  }

  bool offset_tag_values_changed_from_last_frame =
      std::ranges::any_of(offset_tag_data_, [](auto& entry) {
        return entry.second.current_offset != entry.second.last_offset;
      });

  if (offset_tag_values_changed_from_last_frame) {
    offset_tag_render_passes_.clear();
  } else if (!offset_tag_render_passes_.empty()) {
    // If offset tag values haven't changed and the copied render passes weren't
    // cleared elsewhere they can be reused.
    CHECK_EQ(offset_tag_render_passes_.size(), resolved_passes_.size());
    for (size_t i = 0; i < offset_tag_render_passes_.size(); ++i) {
      resolved_passes_[i].SetCompositorRenderPass(
          offset_tag_render_passes_[i].get());
    }
    // Skip running RecomputeOffsetTagDamage() since nothing changed since
    // last frame and there is no added damage.
    return;
  }

  if (has_non_zero_offset_tag_value_) {
    RebuildRenderPassesForOffsetTags();
  }
  RecomputeOffsetTagDamage();
}

void ResolvedFrameData::ComputeOffsetTagContainingRects() {
  auto& offset_tags_to_find = GetMetadata().offset_tag_definitions;
  if (offset_tags_to_find.empty()) {
    return;
  }

  // `offset_tag_data_` hasn't been populated for this frame yet so build a set
  // to check if a definition exists when a tag is seen on a layer.
  base::flat_set<OffsetTag> tag_set(
      base::ToVector(offset_tags_to_find,
                     [](const OffsetTagDefinition& def) { return def.tag; }));

  for (auto& resolved_pass : resolved_passes_) {
    const CompositorRenderPass& render_pass = resolved_pass.render_pass();

    for (auto* sqs : render_pass.shared_quad_state_list) {
      if (sqs->offset_tag && tag_set.contains(sqs->offset_tag)) {
        gfx::Transform combined_transform =
            render_pass.transform_to_root_target *
            sqs->quad_to_target_transform;
        gfx::Rect containing_rect_in_root =
            cc::MathUtil::MapEnclosingClippedRect(combined_transform,
                                                  sqs->visible_quad_layer_rect);
        offset_tag_data_[sqs->offset_tag].current_containing_rect.Union(
            containing_rect_in_root);
      }
    }
  }

  for (auto& [tag, data] : offset_tag_data_) {
    data.current_containing_rect.Intersect(GetOutputRect());
  }
}

void ResolvedFrameData::RebuildRenderPassesForOffsetTags() {
  CHECK(offset_tag_render_passes_.empty());
  CHECK(has_non_zero_offset_tag_value_);

  // Create copies of the render passes and modify tagged quad positions by
  // adjusting `quad_to_target_transform` transform.
  // TODO(kylechar): This only needs to make a copy of render passes that have
  // tagged quads.
  offset_tag_render_passes_.reserve(resolved_passes_.size());
  for (auto& resolved_pass : resolved_passes_) {
    CHECK(resolved_pass.fixed_.render_pass);

    // DeepCopy() can't copy CopyOutputRequests. Remove them from `source_pass`
    // before copying and then add them back afterwards. The requests are
    // copied to the AggregatedRenderPass by Surface::TakeCopyOutputRequests()
    // which will look in the original render pass.
    auto source_pass = resolved_pass.fixed_.render_pass;
    auto copy_requests = std::move(source_pass->copy_requests);
    auto modified_pass = source_pass->DeepCopy();
    source_pass->copy_requests = std::move(copy_requests);

    for (auto* sqs : modified_pass->shared_quad_state_list) {
      if (sqs->offset_tag && offset_tag_data_.contains(sqs->offset_tag)) {
        auto& tag_data = offset_tag_data_[sqs->offset_tag];
        if (!tag_data.current_offset.IsZero()) {
          sqs->quad_to_target_transform.PostTranslate(tag_data.current_offset);

          if (!sqs->mask_filter_info.IsEmpty()) {
            // Slim compositor enforces that mask filter info isn't added on
            // a fixed parent layer that has a child layer with offset tag, so
            // we can assume the mask filter info should also be translated.
            // See crbug.com/361804880 for details.
            sqs->mask_filter_info.ApplyTransform(
                gfx::Transform::MakeTranslation(tag_data.current_offset));
          }
        }
      }
    }

    // Replace the CompositorRenderPass pointer so that modified frame is used
    // during aggregation.
    resolved_pass.fixed_.render_pass = modified_pass.get();
    offset_tag_render_passes_.push_back(std::move(modified_pass));
  }
}

void ResolvedFrameData::RecomputeOffsetTagDamage() {
  CHECK(offset_tag_added_damage_.IsEmpty());

  // Get the surface damage before this function modifies it.
  const gfx::Rect surface_damage_rect = GetSurfaceDamage();
  const gfx::Rect output_rect = GetOutputRect();

  if (surface_damage_rect == output_rect) {
    // If the frame already has full damage then there is no point computing
    // damage to add.
    return;
  }

  for (auto& [tag, data] : offset_tag_data_) {
    if (data.current_offset != data.last_offset) {
      // If the offset value for a tag changed then both the old and new
      // locations of tagged quads are damaged since content moved.
      offset_tag_added_damage_.Union(EnclosingOffsetRect(
          data.current_containing_rect, data.current_offset));
      offset_tag_added_damage_.Union(
          EnclosingOffsetRect(data.last_containing_rect, data.last_offset));
    } else if (!data.current_offset.IsZero()) {
      // If the offset didn't change and current offset is non-zero then adjust
      // client provided damage to take into account quads that were offset.
      // This assumes that any damage which intersects the tagged quads comes
      // from the tagged quads. This isn't necessarily true but there isn't
      // enough information here to know what layer/quads introduced the damage
      // so this is pessimistic.
      offset_tag_added_damage_.Union(
          EnclosingOffsetRect(gfx::IntersectRects(data.current_containing_rect,
                                                  surface_damage_rect),
                              data.current_offset));

      if (!data.last_containing_rect.IsEmpty() &&
          !data.current_containing_rect.Contains(data.last_containing_rect)) {
        // This case aims to detect when a layer had a tag removed or a tagged
        // layer was deleted. The client will add damage for the removed layer
        // at it's default location but since `last_offset` is non-zero the
        // content was drawn elsewhere. Viz needs to add damage where the
        // removed layer was drawn. There is no simple way to track when tagged
        // layers are removed, so this uses an imperfect proxy of containing
        // rect shrinking, and if that happens it adds damage for all tagged
        // layers last frame.
        //
        // It's possible the containing rect shrinks without removing a tagged
        // layer, eg. size or position of the tagged layers change. This case
        // will result in adding extra damage that isn't needed which has no
        // impact on correctness.
        //
        // It's also possible a tagged layer was removed but the containing rect
        // doesn't shrink, eg. either another tagged layer was added or other
        // tagged layers had size/position change. The current containing rect
        // still contains all last frames tagged layers and client adds damage
        // for the removed layer so the code above to shift client damage will
        // handle it.
        offset_tag_added_damage_.Union(
            EnclosingOffsetRect(data.last_containing_rect, data.last_offset));
      }
    }
  }
  // Clip added damage if it extends beyond output rect.
  offset_tag_added_damage_.Intersect(output_rect);
}

void ResolvedFrameData::SetInvalid() {
  frame_index_ = surface_->GetActiveFrameIndex();
  render_pass_id_map_.clear();
  resolved_passes_.clear();
  valid_ = false;
}

bool ResolvedFrameData::WasUsedInAggregation() const {
  return used_in_aggregation_;
}

void ResolvedFrameData::ResetAfterAggregation() {
  // Reset aggregation scoped data.
  for (auto& resolved_pass : resolved_passes_) {
    resolved_pass.aggregation().Reset();
    resolved_pass.CopyAndResetPersistentPassData();
    resolved_pass.ResetCompositorRenderPass();
  }

  previous_frame_index_ = frame_index_;
  used_in_aggregation_ = false;

  // Delete entries for tags that weren't defined in this frame.
  base::EraseIf(offset_tag_data_,
                [](auto& entry) { return !entry.second.defined_in_frame; });

  // Reset remaining offset tag data for next aggregation.
  for (auto& [tag, data] : offset_tag_data_) {
    data.last_containing_rect = data.current_containing_rect;
    data.current_containing_rect = gfx::Rect();
    data.last_offset = data.current_offset;
    data.current_offset = gfx::Vector2dF();
    data.defined_in_frame = false;
  }
  offset_tag_added_damage_ = gfx::Rect();
  // Don't reset `has_non_zero_offset_tag_value_` until next aggregation when
  // UpdateOffsetTags() is called.
}

const CompositorFrameMetadata& ResolvedFrameData::GetMetadata() const {
  // TODO(crbug.com/354664676): Add back CHECK(valid_) once this is only called
  // for valid frames.
  return surface_->GetActiveFrameMetadata();
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
      return gfx::UnionRects(resolved_passes_.back().render_pass().damage_rect,
                             offset_tag_added_damage_);
    case FrameDamageType::kNone:
      return offset_tag_added_damage_;
  }
}

const gfx::Rect& ResolvedFrameData::GetOutputRect() const {
  DCHECK(valid_);
  return resolved_passes_.back().render_pass().output_rect;
}

void ResolvedFrameData::ReuseActiveFrame() {
  const CompositorRenderPassList& render_pass_list =
      surface_->GetActiveFrame().render_pass_list;

  // `render_pass_list` and `resolved_passes_` should have the same size and
  // order.
  CHECK_EQ(render_pass_list.size(), resolved_passes_.size());
  for (size_t i = 0; i < resolved_passes_.size(); ++i) {
    ResolvedPassData& resolved_pass = resolved_passes_[i];
    const auto& render_pass = render_pass_list[i];
    CHECK_EQ(resolved_pass.render_pass_id(), render_pass->id);
    resolved_pass.SetCompositorRenderPass(render_pass.get());
  }

  // OffsetTag containing rects are the same as last frame.
  for (auto& [tag, containing] : offset_tag_data_) {
    containing.current_containing_rect = containing.last_containing_rect;
  }
}

void ResolvedFrameData::RegisterWithResourceProvider() {
  child_resource_id_ = resource_provider_->CreateChild(
      base::BindRepeating(&SurfaceClient::UnrefResources, surface_->client()),
      surface_id_);
}

void ResolvedFrameData::MovePersistentPassDataFromPreviousFrame(
    const std::vector<ResolvedPassData>& previous_resolved_passes) {
  for (const auto& previous_resolved_pass : previous_resolved_passes) {
    auto render_pass_id = previous_resolved_pass.render_pass_id();
    // iter to |current_persistent_data_|
    auto iter = render_pass_id_map_.find(render_pass_id);

    if (iter != render_pass_id_map_.end()) {
      iter->second->previous_persistent_data() =
          previous_resolved_pass.previous_persistent_data();
    }
  }
}

}  // namespace viz
