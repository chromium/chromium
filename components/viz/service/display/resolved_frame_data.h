// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind_internal.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/surfaces/frame_index_constants.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class Surface;

// Returns |damage_rect| field from the DrawQuad if it exists otherwise returns
// an empty optional.
const std::optional<gfx::Rect>& GetOptionalDamageRectFromQuad(
    const DrawQuad* quad);

// Data associated with a DrawQuad in a resolved frame.
struct VIZ_SERVICE_EXPORT ResolvedQuadData {
  explicit ResolvedQuadData(const DrawQuad& quad);

  // Remapped display ResourceIds.
  DrawQuad::Resources remapped_resources;
};

// Render pass data that is fixed for the lifetime of ResolvedPassData.
struct VIZ_SERVICE_EXPORT FixedPassData {
  FixedPassData();
  FixedPassData(FixedPassData&& other);
  FixedPassData& operator=(FixedPassData&& other);
  ~FixedPassData();

  // Only valid during aggregation: set at the beginning a new round of
  // aggregation and reset to null at the end of each aggregation.
  //
  // This shouldn't be dangling anymore because CompositorFrames are never
  // destroyed during aggregation so the pointer will remain valid for the
  // duration of aggregation (until it's set to null).
  raw_ptr<CompositorRenderPass> render_pass = nullptr;

  // How many times this render pass is embedded by another render pass in the
  // same frame.
  int embed_count = 0;

  AggregatedRenderPassId remapped_id;
  CompositorRenderPassId render_pass_id;
  bool is_root = false;
  std::vector<ResolvedQuadData> draw_quads;
};

class ResolvedPassData;

// Render pass data that must be recomputed each aggregation. Unlike
// FixedPassData this changes each aggregation depending on what other
// Surfaces/CompositorFrames are part of the draw tree.
struct VIZ_SERVICE_EXPORT AggregationPassData {
  AggregationPassData();
  AggregationPassData(AggregationPassData&& other);
  AggregationPassData& operator=(AggregationPassData&& other);
  ~AggregationPassData();

  // Resets to default constructed state.
  void Reset();

  // Embedded render passes that contribute pixels to this render pass.
  base::flat_set<raw_ptr<ResolvedPassData, CtnExperimental>> embedded_passes;

  // True if the render pass is drawn to fulfil part of a copy request. This
  // property is transitive from parent pass to embedded passes.
  bool in_copy_request_pass = false;

  // True if the render pass is be impacted by a pixel moving foreground filter.
  // This property is transitive from parent pass to embedded passes.
  bool in_pixel_moving_filter_pass = false;

  // True if the render pass will be stored as part of a cached render pass.
  // This property is transitive from parent pass to embedded passes.
  bool in_cached_render_pass = false;

  // True if there is accumulated damage in the render pass or from contributing
  // render passes or surfaces. This bit indicates whether the render pass needs
  // to be redrawn since its content has changed from the previous frame or if
  // the cached content from the previous frame can be reused.
  //
  // Note: This is different than checking render pass damage_rect.IsEmpty(). cc
  // resets any non-root render pass damage_rects and aggregates non-root damage
  // into the root render pass damage_rect. cc already plumbs a separate bool
  // `has_damage_from_contributing_content` with each CompositorRenderPass to
  // say if the render pass has damage. Ideally cc would just plumb the correct
  // damage_rect and no bool. `has_damage` also takes into account if there is
  // added damage from embedded content or filters that the client submitting
  // the CompositorFrame didn't know about.
  bool has_damage = false;

  // Indicates that the render pass is embedded from the root surface root
  // render pass and will contribute pixels to framebuffer. Render passes this
  // is false for may still be drawn but they won't contribute pixels to
  // framebuffer.
  bool will_draw = false;

  // The damage added from its descandant surfaces during aggregation. This is
  // not part of the original render_pass->damage_rect from CC.
  gfx::Rect added_damage;

  // |true| to if this pass should embedded and not merged. This is to support
  // |kDelegatedCompositingLimitToUi| on Windows to keep the web contents
  // surface in a swap chain instead of delegating its quads.
  // TODO(crbug.com/324460866): Used for partially delegated compositing.
  bool prevent_merge = false;
};

// Render pass data that must be recomputed each aggregation and needs to be
// persisted to next aggregation.
struct PersistentPassData {
  PersistentPassData();
  PersistentPassData(PersistentPassData&& other);
  PersistentPassData& operator=(PersistentPassData& other);
  PersistentPassData& operator=(const PersistentPassData& other);
  PersistentPassData& operator=(PersistentPassData&& other);
  ~PersistentPassData();

  enum MergeState { kInitState, kNotMerged, kAlwaysMerged, kSomeTimesMerged };

  // The intersection of all render pass output rects, RenderPassDrawQuad rect,
  // SurfaceDrawQuad rect, and clip rects from its ancestor render passes and
  // surface. This is the max size this render pass can be rendered into the
  // root surface. |parent_clip_rect| is in the dest root target space.
  gfx::Rect parent_clip_rect;

  // Whether the render passes is merged with its parent render pass. The render
  // mighe be embedded multiple times and has different status each time.
  MergeState merge_state = kInitState;
};

// Data associated with a CompositorRenderPass in a resolved frame. Has fixed
// portion that does not change and an aggregation portion that does change.
class VIZ_SERVICE_EXPORT ResolvedPassData {
 public:
  explicit ResolvedPassData(FixedPassData fixed_data);
  ~ResolvedPassData();
  ResolvedPassData(ResolvedPassData&& other);
  ResolvedPassData& operator=(ResolvedPassData&& other);

  const CompositorRenderPass& render_pass() const;
  AggregatedRenderPassId remapped_id() const { return fixed_.remapped_id; }
  CompositorRenderPassId render_pass_id() const {
    return fixed_.render_pass_id;
  }
  bool is_root() const { return fixed_.is_root; }
  const std::vector<ResolvedQuadData>& draw_quads() const {
    return fixed_.draw_quads;
  }

  // Returns true if the render pass is not embedded by another render pass and
  // is not the root render pass.
  bool IsUnembedded() const {
    return !fixed_.is_root && fixed_.embed_count == 0;
  }

  AggregationPassData& aggregation() { return aggregation_; }
  const AggregationPassData& aggregation() const { return aggregation_; }

  PersistentPassData& current_persistent_data() {
    return current_persistent_data_;
  }

  PersistentPassData& previous_persistent_data() {
    return previous_persistent_data_;
  }

  const PersistentPassData& previous_persistent_data() const {
    return previous_persistent_data_;
  }

  void CopyAndResetPersistentPassData();

  // Set `fixed_.render_pass` to `pass`. Should be called at the beginning of an
  // aggregation.
  void SetCompositorRenderPass(CompositorRenderPass* pass);

  // Set `fixed_.render_pass` back to null, to avoid the dangling pointer
  // after aggregation. Should be called at the end of an aggregation.
  void ResetCompositorRenderPass();

 private:
  friend class ResolvedFrameData;

  // Data that is constant for the life of the resolved pass.
  FixedPassData fixed_;

  // Data that will change each aggregation.
  AggregationPassData aggregation_;

  PersistentPassData current_persistent_data_;
  PersistentPassData previous_persistent_data_;
};

enum FrameDamageType {
  // The CompositorFrame should be considered fully damaged. This could be the
  // first CompositorFrame from the client, an intermediate CompositorFrame was
  // skipped so the damage is unknown or there is synthetic damage.
  kFull,
  // The damage contained in the CompositorFrame should be used.
  kFrame,
  // The CompositorFrame is the same as last aggregation and has no damage.
  kNone
};

// Holds computed information for a particular Surface+CompositorFrame. The
// CompositorFrame computed information will be updated whenever the active
// frame for the surface has changed. On destruction any resources registered
// with DisplayResourceProvider will be released.
//
// The first time a resolved frame is needed during aggregation
// UpdateForAggregation() then UpdateOffsetTags() must be called. That will
// populate all of the internal data. During aggregation the fixed data is
// viewed and aggregation data can be modified. After aggregation is over
// ResetAfterAggregation() will be called which resets aggregation data.
class VIZ_SERVICE_EXPORT ResolvedFrameData {
 public:
  using OffsetTagLookupFn =
      base::FunctionRef<gfx::Vector2dF(const OffsetTagDefinition&)>;

  ResolvedFrameData(DisplayResourceProvider* resource_provider,
                    Surface* surface,
                    uint64_t prev_frame_index,
                    AggregatedRenderPassId prev_root_pass_id);
  ~ResolvedFrameData();
  ResolvedFrameData(ResolvedFrameData&& other) = delete;
  ResolvedFrameData& operator=(ResolvedFrameData&& other) = delete;

  const SurfaceId& surface_id() const { return surface_id_; }
  Surface* surface() const { return surface_; }
  bool is_valid() const { return valid_; }
  uint64_t previous_frame_index() const { return previous_frame_index_; }

  gfx::Size size_in_pixels() const;
  float device_scale_factor() const;

  // Returns namespace ID for the client that submitted this frame. This is used
  // to deduplicate layer IDs from different clients.
  uint32_t GetClientNamespaceId() const;

  void SetFullDamageForNextAggregation();

  // Force release all resources registered with display resource provider. Note
  // there must be a new CompositorFrame available that doesn't use any existing
  // resources since resources (might) be missing on next draw.
  void ForceReleaseResource();

  // This must be called once before using ResolvedFrameData during aggregation.
  // If there is a new active CompositorFrame data will be fully updated, see
  // UpdatedActiveFrame() for details, otherwise previous data will be reused.
  void UpdateForAggregation(
      AggregatedRenderPassId::Generator& render_pass_id_generator);

  // This should be called each aggregation after UpdateForAggregation() to
  // update resolved frame for OffsetTags. If the active CompositorFrame defines
  // any tags, the tag values will be found and the resolved frame will be
  // modified.
  void UpdateOffsetTags(OffsetTagLookupFn lookup_value);

  // Sets frame index and marks as invalid. This also clears any existing
  // resolved pass data.
  void SetInvalid();

  bool WasUsedInAggregation() const;

  // Resets aggregation data and WasUsedInAggregation() will now return false.
  void ResetAfterAggregation();

  // All functions after this point are accessors for the resolved frame and
  // should only be called if is_valid() returns true.

  const CompositorFrameMetadata& GetMetadata() const;

  // Returns true if the root render pass is embedded from the the root surface
  // root render pass.
  bool WillDraw() const;

  // RenderPassData accessors.
  ResolvedPassData& GetRenderPassDataById(
      CompositorRenderPassId render_pass_id);
  const ResolvedPassData& GetRenderPassDataById(
      CompositorRenderPassId render_pass_id) const;

  ResolvedPassData& GetRootRenderPassData();
  const ResolvedPassData& GetRootRenderPassData() const;

  std::vector<ResolvedPassData>& GetResolvedPasses() {
    return resolved_passes_;
  }
  const std::vector<ResolvedPassData>& GetResolvedPasses() const {
    return resolved_passes_;
  }

  // See `FrameDamageType` definition for what each status means.
  FrameDamageType GetFrameDamageType() const;

  // Returns surface damage rect. This is based on changes from the
  // CompositorFrame aggregated last frame. This limited to the root render
  // passes damage_rect and does not include individual quads that add damage.
  gfx::Rect GetSurfaceDamage() const;

  // Returns the root render pass output_rect.
  const gfx::Rect& GetOutputRect() const;

 private:
  friend class ResolvedFrameDataTestHelper;

  // Data for a specific `OffsetTag`.
  struct OffsetTagData {
    // The offset value that is used.
    gfx::Vector2dF last_offset;
    gfx::Vector2dF current_offset;
    // The containing rect is the union of all tagged quad visible rects in
    // root render pass coordinate space before applying any offsets.
    gfx::Rect last_containing_rect;
    gfx::Rect current_containing_rect;
    bool defined_in_frame = false;
  };

  // Updates ResolvedPassData for a new active frame. It also updates surface
  // client and display resource provider with resources used in the new active
  // frame.
  //
  // This performs the following validation on the active CompositorFrame.
  // 1. Checks each ResourceId was registered with DisplayResourceProvider and
  //    is in |child_to_parent_map|.
  // 2. Checks that CompositorRenderPasses have unique ids.
  // 3. Checks that CompositorRenderPassDrawQuads only embed render passes that
  //    are drawn before. This has the side effect of disallowing any cycles.
  //
  // If validation fails then ResolvedPassData will be cleared and is_valid()
  // will return false.
  void UpdateActiveFrame(
      AggregatedRenderPassId::Generator& render_pass_id_generator);

  // Set `CompositorRenderPass` for all `resolved_passes_`. Each
  // `ResolvedPassData` must have been aggregated before.
  void ReuseActiveFrame();

  void RegisterWithResourceProvider();
  void MovePersistentPassDataFromPreviousFrame(
      const std::vector<ResolvedPassData>& previoius_resolved_passes);

  void ComputeOffsetTagContainingRects();
  void RebuildRenderPassesForOffsetTags();
  void RecomputeOffsetTagDamage();

  const raw_ptr<DisplayResourceProvider> resource_provider_;
  const SurfaceId surface_id_;
  const raw_ptr<Surface> surface_;

  // Child resource ID assigned by `resource_provider_`.
  int child_resource_id_ = 0;

  // Data associated with CompositorFrame with |frame_index_|.
  bool valid_ = false;
  uint64_t frame_index_ = kInvalidFrameIndex;
  uint64_t previous_frame_index_ = kInvalidFrameIndex;

  base::flat_map<OffsetTag, OffsetTagData> offset_tag_data_;
  // Additional damage that is due to OffsetTag changes between aggregations
  // that is unioned into the surface damage.
  gfx::Rect offset_tag_added_damage_;
  bool has_non_zero_offset_tag_value_ = false;

  // Holds a modified copy of render passes from current active CompositorFrame.
  std::vector<std::unique_ptr<CompositorRenderPass>> offset_tag_render_passes_;

  std::vector<ResolvedPassData> resolved_passes_;
  base::flat_map<CompositorRenderPassId,
                 raw_ptr<ResolvedPassData, CtnExperimental>>
      render_pass_id_map_;
  base::flat_map<CompositorRenderPassId, AggregatedRenderPassId>
      aggregated_id_map_;

  const AggregatedRenderPassId prev_root_pass_id_;

  // Track if the this resolved frame was used this aggregation.
  bool used_in_aggregation_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
