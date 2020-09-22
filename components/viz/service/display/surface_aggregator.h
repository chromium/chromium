// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/delegated_ink_metadata.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/service/display/render_pass_id_remapper.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/overlay_transform.h"

namespace viz {
class AggregatedFrame;
class CompositorFrame;
class DisplayResourceProvider;
class Surface;
class SurfaceClient;
class SurfaceDrawQuad;
class SurfaceManager;

class VIZ_SERVICE_EXPORT SurfaceAggregator {
 public:
  using SurfaceIndexMap = base::flat_map<SurfaceId, uint64_t>;
  using FrameSinkIdMap = base::flat_map<FrameSinkId, LocalSurfaceId>;

  // Interface that can modify the aggregated CompositorFrame to annotate it.
  // For example it could add extra quads.
  class FrameAnnotator {
   public:
    virtual ~FrameAnnotator() = default;

    virtual void AnnotateAggregatedFrame(AggregatedFrame* frame) = 0;
  };

  SurfaceAggregator(SurfaceManager* manager,
                    DisplayResourceProvider* provider,
                    bool aggregate_only_damaged,
                    bool needs_surface_occluding_damage_rect);
  ~SurfaceAggregator();

  // |target_damage| represents an area on the output surface that might have
  // been invalidated. It can be used in cases where we still want to support
  // partial damage but the target surface might need contents outside the
  // damage rect of the root surface.
  AggregatedFrame Aggregate(const SurfaceId& surface_id,
                            base::TimeTicks expected_display_time,
                            gfx::OverlayTransform display_transform,
                            const gfx::Rect& target_damage = gfx::Rect(),
                            int64_t display_trace_id = -1);
  void ReleaseResources(const SurfaceId& surface_id);
  const SurfaceIndexMap& previous_contained_surfaces() const {
    return previous_contained_surfaces_;
  }
  const FrameSinkIdMap& previous_contained_frame_sinks() const {
    return previous_contained_frame_sinks_;
  }
  void SetFullDamageForSurface(const SurfaceId& surface_id);
  void set_output_is_secure(bool secure) { output_is_secure_ = secure; }

  // Only used with experimental de-jelly effect.
  bool last_frame_had_jelly() const { return last_frame_had_jelly_; }

  // Set the color spaces for the created RenderPasses, which is propagated
  // to the output surface.
  void SetDisplayColorSpaces(const gfx::DisplayColorSpaces& color_spaces);

  void SetMaxRenderTargetSize(int max_size);

  bool NotifySurfaceDamageAndCheckForDisplayDamage(const SurfaceId& surface_id);

  bool HasFrameAnnotator() const;
  void SetFrameAnnotator(std::unique_ptr<FrameAnnotator> frame_annotator);
  void DestroyFrameAnnotator();

 private:
  struct ClipData;
  struct PrewalkResult;
  struct RoundedCornerInfo;
  struct ChildSurfaceInfo;
  struct RenderPassMapEntry;

  // Helper function that gets a list of render passes and returns a map from
  // render pass ids to render passes.
  static base::flat_map<CompositorRenderPassId, RenderPassMapEntry>
  GenerateRenderPassMap(const CompositorRenderPassList& render_pass_list,
                        bool is_root_surface);

  ClipData CalculateClipRect(const ClipData& surface_clip,
                             const ClipData& quad_clip,
                             const gfx::Transform& target_transform);

  void HandleSurfaceQuad(const SurfaceDrawQuad* surface_quad,
                         float parent_device_scale_factor,
                         const gfx::Transform& target_transform,
                         const ClipData& clip_rect,
                         AggregatedRenderPass* dest_pass,
                         bool ignore_undamaged,
                         gfx::Rect* damage_rect_in_quad_space,
                         bool* damage_rect_in_quad_space_valid,
                         const RoundedCornerInfo& rounded_corner_info);

  void EmitSurfaceContent(Surface* surface,
                          float parent_device_scale_factor,
                          const SurfaceDrawQuad* surface_quad,
                          const gfx::Transform& target_transform,
                          const ClipData& clip_rect,
                          AggregatedRenderPass* dest_pass,
                          bool ignore_undamaged,
                          gfx::Rect* damage_rect_in_quad_space,
                          bool* damage_rect_in_quad_space_valid,
                          const RoundedCornerInfo& rounded_corner_info);

  void EmitDefaultBackgroundColorQuad(
      const SurfaceDrawQuad* surface_quad,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      AggregatedRenderPass* dest_pass,
      const RoundedCornerInfo& rounded_corner_info);

  void EmitGutterQuadsIfNecessary(
      const gfx::Rect& primary_rect,
      const gfx::Rect& fallback_rect,
      const SharedQuadState* primary_shared_quad_state,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      SkColor background_color,
      AggregatedRenderPass* dest_pass,
      const RoundedCornerInfo& rounded_corner_info);

  SharedQuadState* CopySharedQuadState(
      const SharedQuadState* source_sqs,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      AggregatedRenderPass* dest_render_pass,
      const RoundedCornerInfo& rounded_corner_info,
      const gfx::Rect& occluding_damage_rect,
      bool occluding_damage_rect_valid);

  SharedQuadState* CopyAndScaleSharedQuadState(
      const SharedQuadState* source_sqs,
      const gfx::Transform& scaled_quad_to_target_transform,
      const gfx::Transform& target_transform,
      const gfx::Rect& quad_layer_rect,
      const gfx::Rect& visible_quad_layer_rect,
      const ClipData& clip_rect,
      AggregatedRenderPass* dest_render_pass,
      const RoundedCornerInfo& rounded_corner_info,
      const gfx::Rect& occluding_damage_rect,
      bool occluding_damage_rect_valid);

  void CopyQuadsToPass(
      const CompositorRenderPass& source_pass,
      AggregatedRenderPass* dest_pass,
      float parent_device_scale_factor,
      const std::unordered_map<ResourceId, ResourceId>& resource_to_child_map,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      const SurfaceId& surface_id,
      const RoundedCornerInfo& rounded_corner_info);

  // Recursively walks through the render pass and updates the
  // |can_use_backdrop_filter_cache| flag on all RenderPassDrawQuads(RPDQ).
  // The function returns the damage rect of the render pass in its own content
  // space.
  //  - |render_pass_entry| specifies the render pass in the entry map to be
  //  prewalked
  //  - |surface| is the surface containing the render pass.
  //  - |render_pass_map| is a map that contains all render passes and their
  //    entry data.
  //  - |will_draw| indicates that the surface can be aggregated into the final
  //    frame and might be drawn (based on damage/occlusion/etc.) if it is set
  //    to true. Or the surface isn't in the aggregated frame and is only
  //    needed for CopyOutputRequests if set to false.
  //  - |damage_from_parent| is the damage rect passed along from parent or
  //    a chain of ancestor render passes, transformed into the local space of
  //    the current render pass. This happens when the root render
  //    pass of |surface| is merged to its parent render pass (and so on).
  //    |damage_from_parent| represents the current effective accumulated damage
  //    from the parent render pass into which the surface quad containing the
  //    |surface| is being merged. This includes the damage from quads under
  //    the surface quad in the render pass merged to, plus its |damage_rect|
  //    and damage passed onto it by its parent if any.
  //    If there's no merging of |surface|, |accummulated_damage| is empty.
  //  - |target_to_root_transform| is the transform from current render pass to
  //    the root.
  //  - |in_moved_pixel_rp| marks if the current render pass is embedded by an
  //    ancestor render pass with a pixel-moving foreground filter.
  //  - |result| is the result of a prewalk of the surface that contains the
  //    render pass.
  gfx::Rect PrewalkRenderPass(
      RenderPassMapEntry* render_pass_entry,
      const Surface* surface,
      base::flat_map<CompositorRenderPassId, RenderPassMapEntry>*
          render_pass_map,
      bool will_draw,
      const gfx::Rect& damage_from_parent,
      const gfx::Transform& target_to_root_transform,
      bool in_moved_pixel_rp,
      PrewalkResult* result);

  // Walk the Surface tree from |surface|. Validate the resources of the
  // current surface and its descendants, check if there are any copy requests,
  // and return the combined damage rect.
  gfx::Rect PrewalkSurface(Surface* surface,
                           bool in_moved_pixel_rp,
                           AggregatedRenderPassId parent_pass,
                           bool will_draw,
                           const gfx::Rect& damage_from_parent,
                           PrewalkResult* result);
  void CopyUndrawnSurfaces(PrewalkResult* prewalk);
  void CopyPasses(const CompositorFrame& frame, Surface* surface);
  void AddColorConversionPass();
  void AddDisplayTransformPass();

  // Remove Surfaces that were referenced before but aren't currently
  // referenced from the ResourceProvider.
  // Also notifies SurfaceAggregatorClient of newly added and removed
  // child surfaces.
  void ProcessAddedAndRemovedSurfaces();

  void PropagateCopyRequestPasses();

  // Returns true if the quad list from the render pass provided can be merged
  // with its target render pass based on rounded corners.
  bool CanMergeRoundedCorner(const RoundedCornerInfo& rounded_corner_info,
                             const CompositorRenderPass& root_render_pass);

  int ChildIdForSurface(Surface* surface);
  bool IsSurfaceFrameIndexSameAsPrevious(const Surface* surface) const;
  gfx::Rect DamageRectForSurface(const Surface* surface,
                                 const CompositorRenderPass& source,
                                 const gfx::Rect& full_rect) const;
  gfx::Rect CalculateOccludingSurfaceDamageRect(
      const DrawQuad* quad,
      const gfx::Transform& parent_quad_to_root_target_transform);

  // This function adds |damage_rect| to
  // |damage_rects_union_of_surfaces_on_top_|. |damage_rect| is in the quad
  // content space while both clip_rect and
  // |damage_rects_union_of_surfaces_on_top_| are already on the root target
  // space.
  void UnionSurfaceDamageRectsOnTop(
      const gfx::Rect& damage_rect,
      const gfx::Transform& parent_to_root_target_transform,
      const ClipData& clip_rect);

  // Determine the overlay occluding damage.
  const DrawQuad* ProcessSurfaceOccludingDamage(
      const CompositorRenderPass& source_pass,
      AggregatedRenderPass* dest_pass,
      const gfx::Transform& parent_target_transform,
      const SurfaceId& surface_id,
      const ClipData& clip_rect,
      gfx::Rect* occluding_damage_rect);

  // Returns true if the render pass with the given id and cache_render_pass
  // flag would need full damage.
  bool RenderPassNeedsFullDamage(const AggregatedRenderPassId& id,
                                 bool cache_render_pass) const;

  bool IsRootSurface(const Surface* surface) const;

  static void UnrefResources(base::WeakPtr<SurfaceClient> surface_client,
                             const std::vector<ReturnedResource>& resources);

  // This method transforms the delegated ink metadata to be in the root target
  // space, so that it can eventually be drawn onto the back buffer in the
  // correct position. It should only ever be called when a frame contains
  // delegated ink metadata, in which case this function will transform it and
  // then store it in the |delegated_ink_metadata_| member.
  void TransformAndStoreDelegatedInkMetadata(
      const gfx::Transform& parent_quad_to_root_target_transform,
      std::unique_ptr<DelegatedInkMetadata> metadata);

  // Preliminary check to see if a surface contained in |surface_quad| can
  // potentially merge its root render pass. If so, returns true.
  static bool CanPotentiallyMergePass(const SurfaceDrawQuad& surface_quad);

  // De-Jelly Effect:
  // HandleDeJelly applies a de-jelly transform to quads in the root render
  // pass.
  void HandleDeJelly(Surface* surface);
  // CreateDeJellyRenderPassQuads promotes skewed quads from the root render
  // pass into |render_pass|. Skew is applied when |render_pass| is drawn.
  void CreateDeJellyRenderPassQuads(
      cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
      const cc::ListContainer<DrawQuad>::Iterator& end,
      const gfx::Rect& jelly_clip,
      float skew,
      AggregatedRenderPass* render_pass);
  // Appends quads directly to |root_pass|, applying |skew|.
  void CreateDeJellyNormalQuads(
      cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
      const cc::ListContainer<DrawQuad>::Iterator& end,
      AggregatedRenderPass* root_pass,
      float skew);
  // Appends |render_pass| to |root_pass|, applying |skew|, |jelly_clip|,
  // |opacity|, and |blend_mode|.
  void AppendDeJellyRenderPass(
      float skew,
      const gfx::Rect& jelly_clip,
      float opacity,
      SkBlendMode blend_mode,
      AggregatedRenderPass* root_pass,
      std::unique_ptr<AggregatedRenderPass> render_pass);
  // Appends quads from |quad_iterator| to |render_pass| for |state|.
  void AppendDeJellyQuadsForSharedQuadState(
      cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
      const cc::ListContainer<DrawQuad>::Iterator& end,
      AggregatedRenderPass* render_pass,
      const SharedQuadState* state);
  // Update |last_frame_had_jelly_|, should be called once per frame.
  void SetLastFrameHadJelly(bool had_jelly);

  // Resets member variables that were used during Aggregate().
  void ResetAfterAggregate();

  SurfaceManager* const manager_;
  DisplayResourceProvider* const provider_;

  const bool aggregate_only_damaged_;

  // Occluding damage rect will be calculated for qualified candidates
  const bool needs_surface_occluding_damage_rect_;

  // Whether de-jelly may be active.
  const bool de_jelly_enabled_;

  bool output_is_secure_ = false;

  // The color space for the root render pass. If this is different from its
  // blending color space (e.g. for HDR), then a final render pass to convert
  // between the two will be added. This space must always be valid.
  gfx::DisplayColorSpaces display_color_spaces_;

  // Maximum texture size which if larger than zero, will limit the size of
  // render passes.
  int max_render_target_size_ = 0;
  // The id for the final color conversion render pass.
  AggregatedRenderPassId color_conversion_render_pass_id_;
  // The id for the optional render pass used to apply the display transform.
  AggregatedRenderPassId display_transform_render_pass_id_;

  base::flat_map<SurfaceId, int> surface_id_to_resource_child_id_;

  // The following state is only valid for the duration of one Aggregate call
  // and is only stored on the class to avoid having to pass through every
  // function call.

  // This is the set of surfaces referenced in the aggregation so far, used to
  // detect cycles.
  base::flat_set<SurfaceId> referenced_surfaces_;

  SurfaceId root_surface_id_;
  gfx::Transform root_surface_transform_;

  // For each Surface used in the last aggregation, gives the frame_index at
  // that time.
  SurfaceIndexMap previous_contained_surfaces_;
  SurfaceIndexMap contained_surfaces_;
  FrameSinkIdMap previous_contained_frame_sinks_;
  FrameSinkIdMap contained_frame_sinks_;

  // After surface validation, every Surface in this set is valid.
  base::flat_set<SurfaceId> valid_surfaces_;

  // This is the pass list for the aggregated frame.
  AggregatedRenderPassList* dest_pass_list_ = nullptr;

  // The target display time for the aggregated frame.
  base::TimeTicks expected_display_time_;

  // This is the set of aggregated pass ids that are affected by filters that
  // move pixels.
  base::flat_set<AggregatedRenderPassId> moved_pixel_passes_;

  // This is the set of aggregated pass ids that are drawn by copy requests, so
  // should not have their damage rects clipped to the root damage rect.
  base::flat_set<AggregatedRenderPassId> copy_request_passes_;

  // This is the set of aggregated pass ids that has damage from contributing
  // content.
  base::flat_set<AggregatedRenderPassId> contributing_content_damaged_passes_;

  // This maps each aggregated pass id to the set of (aggregated) pass ids
  // that its RenderPassDrawQuads depend on
  base::flat_map<AggregatedRenderPassId, base::flat_set<AggregatedRenderPassId>>
      render_pass_dependencies_;

  // The root damage rect of the currently-aggregating frame.
  gfx::Rect root_damage_rect_;

  // The aggregate color content usage of the currently-aggregating frame. This
  // is computed by the prewalk, and is used to determine the format and color
  // space of all render passes. Note that that is more heavy-handed than is
  // desirable.
  gfx::ContentColorUsage root_content_color_usage_ =
      gfx::ContentColorUsage::kSRGB;

  // This is the union of the damage rects of all surface on top
  // of the current surface.
  gfx::Rect damage_rects_union_of_surfaces_on_top_;

  // True if the frame that's currently being aggregated has copy requests.
  // This is valid during Aggregate after PrewalkSurface is called.
  bool has_copy_requests_ = false;

  // True if the frame that's currently being aggregated has cached render
  // passes. This is valid during Aggregate after PrewalkSurface is called.
  bool has_cached_render_passes_ = false;

  // True if any RenderPasses in the aggregated frame have a backdrop filter
  // that moves pixels. This is valid during Aggregate after PrewalkSurface is
  // called.
  bool has_pixel_moving_backdrop_filter_ = false;

  // For each FrameSinkId, contains a vector of SurfaceRanges that will damage
  // the display if they're damaged.
  base::flat_map<FrameSinkId, std::vector<SurfaceRange>> damage_ranges_;

  // Used to annotate the aggregated frame for debugging.
  std::unique_ptr<FrameAnnotator> frame_annotator_;

  int64_t display_trace_id_ = -1;
  base::flat_set<SurfaceId> undrawn_surfaces_;

  // Variables used for de-jelly:
  // The set of surfacees being drawn for the first time. Used to determine if
  // de-jelly skew should be applied to a surface.
  base::flat_set<SurfaceId> new_surfaces_;
  // Whether the last drawn frame had de-jelly skew applied. Used in production
  // on Android only.
  bool last_frame_had_jelly_ = false;
  // Whether the last drawn frame had a color conversion pass applied. Used in
  // production on Windows only (does not interact with jelly).
  bool last_frame_had_color_conversion_pass_ = false;

  // The metadata used for drawing a delegated ink trail on the end of a normal
  // ink stroke. It needs to be transformed to root coordinates and then put on
  // the final aggregated frame. This is only populated during aggregation when
  // a surface contains delegated ink metadata on its frame, and it is cleared
  // after it is placed on the final aggregated frame during aggregation.
  std::unique_ptr<DelegatedInkMetadata> delegated_ink_metadata_;

  // A helper class used to remap render pass IDs from the surface namespace to
  // a common space, to avoid collisions.
  RenderPassIdRemapper pass_id_remapper_;

  base::WeakPtrFactory<SurfaceAggregator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SurfaceAggregator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_
