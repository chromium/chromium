// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/overlay_transform.h"

namespace viz {
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

    virtual void AnnotateAggregatedFrame(CompositorFrame* frame) = 0;
  };

  SurfaceAggregator(SurfaceManager* manager,
                    DisplayResourceProvider* provider,
                    bool aggregate_only_damaged,
                    bool needs_surface_occluding_damage_rect);
  ~SurfaceAggregator();

  CompositorFrame Aggregate(const SurfaceId& surface_id,
                            base::TimeTicks expected_display_time,
                            gfx::OverlayTransform display_transform,
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
  void SetOutputColorSpace(const gfx::ColorSpace& output_color_space);

  void SetMaximumTextureSize(int max_texture_size);

  bool NotifySurfaceDamageAndCheckForDisplayDamage(const SurfaceId& surface_id);

  void SetFrameAnnotator(std::unique_ptr<FrameAnnotator> frame_annotator);

 private:
  struct ClipData;
  struct PrewalkResult;
  struct RoundedCornerInfo;
  struct ChildSurfaceInfo;
  struct RenderPassMapEntry;

  struct RenderPassInfo {
    // This is the id the pass is mapped to.
    int id;
    // This is true if the pass was used in the last aggregated frame.
    bool in_use = true;
  };

  // Helper function that gets a list of render passes and returns a map from
  // render pass ids to render passes.
  static base::flat_map<RenderPassId, RenderPassMapEntry> GenerateRenderPassMap(
      const RenderPassList& render_pass_list,
      bool is_root_surface);

  ClipData CalculateClipRect(const ClipData& surface_clip,
                             const ClipData& quad_clip,
                             const gfx::Transform& target_transform);

  RenderPassId RemapPassId(RenderPassId surface_local_pass_id,
                           const SurfaceId& surface_id);

  void HandleSurfaceQuad(const SurfaceDrawQuad* surface_quad,
                         float parent_device_scale_factor,
                         const gfx::Transform& target_transform,
                         const ClipData& clip_rect,
                         RenderPass* dest_pass,
                         bool ignore_undamaged,
                         gfx::Rect* damage_rect_in_quad_space,
                         bool* damage_rect_in_quad_space_valid,
                         const RoundedCornerInfo& rounded_corner_info);

  void EmitSurfaceContent(Surface* surface,
                          float parent_device_scale_factor,
                          const SharedQuadState* source_sqs,
                          const gfx::Rect& rect,
                          const gfx::Rect& source_visible_rect,
                          const gfx::Transform& target_transform,
                          const ClipData& clip_rect,
                          bool stretch_content_to_fill_bounds,
                          RenderPass* dest_pass,
                          bool ignore_undamaged,
                          gfx::Rect* damage_rect_in_quad_space,
                          bool* damage_rect_in_quad_space_valid,
                          const RoundedCornerInfo& rounded_corner_info,
                          bool is_reflection,
                          bool allow_merge);

  void EmitDefaultBackgroundColorQuad(
      const SurfaceDrawQuad* surface_quad,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      RenderPass* dest_pass,
      const RoundedCornerInfo& rounded_corner_info);

  void EmitGutterQuadsIfNecessary(
      const gfx::Rect& primary_rect,
      const gfx::Rect& fallback_rect,
      const SharedQuadState* primary_shared_quad_state,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      SkColor background_color,
      RenderPass* dest_pass,
      const RoundedCornerInfo& rounded_corner_info);

  SharedQuadState* CopySharedQuadState(
      const SharedQuadState* source_sqs,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      RenderPass* dest_render_pass,
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
      RenderPass* dest_render_pass,
      const RoundedCornerInfo& rounded_corner_info,
      const gfx::Rect& occluding_damage_rect,
      bool occluding_damage_rect_valid);

  void CopyQuadsToPass(
      const QuadList& source_quad_list,
      const SharedQuadStateList& source_shared_quad_state_list,
      float parent_device_scale_factor,
      const std::unordered_map<ResourceId, ResourceId>& resource_to_child_map,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      RenderPass* dest_pass,
      const SurfaceId& surface_id,
      const RoundedCornerInfo& rounded_corner_info,
      const gfx::Rect& occluding_damage_rect,
      bool occluding_damage_rect_valid);

  // Helper function that uses backtracking on the render pass tree of a surface
  // to find all surfaces embedded in it. If a surface is embedded multiple
  // times (due to use of a MirrorLayer), it will be reachable via multiple
  // paths from the root render pass. For each such a path the appropriate
  // transform is calculated.
  //  - |surface_id| specifies the surface to find all child surfaces of.
  //  - |render_pass_map| is a pre-computed map from render pass id to some info
  //    about the render pass, including the render pass itself and whether it
  //    has pixel moving backdrop filter.
  //  - |current_pass_entry| is the info about the current render pass to
  //    process.
  //  - |transform_to_root_target| is the accumulated transform of all render
  //    passes along the way to the current render pass.
  //  - |child_surfaces| is the main output of the function containing all child
  //    surfaces found in the process.
  //  - |pixel_moving_backdrop_filters_rect| is another output that is union of
  //    bounds of render passes that have a pixel moving backdrop filter.
  // TODO(mohsen): Consider refactoring this backtracking algorithm into a
  // self-contained class.
  void FindChildSurfaces(
      SurfaceId surface_id,
      base::flat_map<RenderPassId, RenderPassMapEntry>* render_pass_map,
      RenderPassMapEntry* current_pass_entry,
      const gfx::Transform& transform_to_root_target,
      base::flat_map<SurfaceRange, ChildSurfaceInfo>* child_surfaces,
      gfx::Rect* pixel_moving_backdrop_filters_rect);

  gfx::Rect PrewalkTree(Surface* surface,
                        bool in_moved_pixel_surface,
                        int parent_pass,
                        bool will_draw,
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
                             const RenderPass& root_render_pass);

  int ChildIdForSurface(Surface* surface);
  bool IsSurfaceFrameIndexSameAsPrevious(const Surface* surface) const;
  gfx::Rect DamageRectForSurface(const Surface* surface,
                                 const RenderPass& source,
                                 const gfx::Rect& full_rect) const;
  gfx::Rect CalculateOccludingSurfaceDamageRect(
      const DrawQuad* quad,
      const gfx::Transform& parent_quad_to_root_target_transform);
  void UnionSurfaceDamageRectsOnTop(const gfx::Rect& surface_rect,
                                    const gfx::Transform& target_transform,
                                    const RenderPass* pass);
  bool ProcessSurfaceOccludingDamage(const Surface* surface,
                                     const RenderPassList& render_pass_list,
                                     const gfx::Transform& target_transform,
                                     const RenderPass* dest_pass,
                                     gfx::Rect* occluding_damage_rect);
  bool RenderPassNeedsFullDamage(const RenderPass* pass) const;
  bool IsRootSurface(const Surface* surface) const;

  static void UnrefResources(base::WeakPtr<SurfaceClient> surface_client,
                             const std::vector<ReturnedResource>& resources);

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
      RenderPass* render_pass);
  // Appends quads directly to |root_pass|, applying |skew|.
  void CreateDeJellyNormalQuads(
      cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
      const cc::ListContainer<DrawQuad>::Iterator& end,
      RenderPass* root_pass,
      float skew);
  // Appends |render_pass| to |root_pass|, applying |skew|, |jelly_clip|,
  // |opacity|, and |blend_mode|.
  void AppendDeJellyRenderPass(float skew,
                               const gfx::Rect& jelly_clip,
                               float opacity,
                               SkBlendMode blend_mode,
                               RenderPass* root_pass,
                               std::unique_ptr<RenderPass> render_pass);
  // Appends quads from |quad_iterator| to |render_pass| for |state|.
  void AppendDeJellyQuadsForSharedQuadState(
      cc::ListContainer<DrawQuad>::Iterator* quad_iterator,
      const cc::ListContainer<DrawQuad>::Iterator& end,
      RenderPass* render_pass,
      const SharedQuadState* state);
  // Update |last_frame_had_jelly_|, should be called once per frame.
  void SetLastFrameHadJelly(bool had_jelly);

  SurfaceManager* manager_;
  DisplayResourceProvider* provider_;

  // Every Surface has its own RenderPass ID namespace. This structure maps
  // each source (SurfaceId, RenderPass id) to a unified ID namespace that's
  // used in the aggregated frame. An entry is removed from the map if it's not
  // used for one output frame.
  base::flat_map<std::pair<SurfaceId, RenderPassId>, RenderPassInfo>
      render_pass_allocator_map_;
  RenderPassId next_render_pass_id_;
  const bool aggregate_only_damaged_;
  bool output_is_secure_;

  // The color space for the root render pass. If this is different from its
  // blending color space (e.g. for HDR), then a final render pass to convert
  // between the two will be added. This space must always be valid.
  gfx::ColorSpace output_color_space_ = gfx::ColorSpace::CreateSRGB();
  // Maximum texture size which if positive, will limit the size of render
  // passes.
  int max_texture_size_ = 0;
  // The id for the final color conversion render pass.
  RenderPassId color_conversion_render_pass_id_ = 0;
  // The id for the optional render pass used to apply the display transform.
  RenderPassId display_transform_render_pass_id_ = 0;

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
  RenderPassList* dest_pass_list_;

  // The target display time for the aggregated frame.
  base::TimeTicks expected_display_time_;

  // This is the set of aggregated pass ids that are affected by filters that
  // move pixels.
  base::flat_set<RenderPassId> moved_pixel_passes_;

  // This is the set of aggregated pass ids that are drawn by copy requests, so
  // should not have their damage rects clipped to the root damage rect.
  base::flat_set<RenderPassId> copy_request_passes_;

  // This is the set of aggregated pass ids that has damage from contributing
  // content.
  base::flat_set<RenderPassId> contributing_content_damaged_passes_;

  // This maps each aggregated pass id to the set of (aggregated) pass ids
  // that its RenderPassDrawQuads depend on
  base::flat_map<RenderPassId, base::flat_set<RenderPassId>>
      render_pass_dependencies_;

  // The root damage rect of the currently-aggregating frame.
  gfx::Rect root_damage_rect_;

  // Occluding damage rect will be calculated for qualified candidates
  const bool needs_surface_occluding_damage_rect_;

  // This is the union of the damage rects of all surface on top
  // of the current surface.
  gfx::Rect damage_rects_union_of_surfaces_on_top_;

  // True if the frame that's currently being aggregated has copy requests.
  // This is valid during Aggregate after PrewalkTree is called.
  bool has_copy_requests_;

  // True if the frame that's currently being aggregated has cached render
  // passes. This is valid during Aggregate after PrewalkTree is called.
  bool has_cached_render_passes_;

  // For each FrameSinkId, contains a vector of SurfaceRanges that will damage
  // the display if they're damaged.
  base::flat_map<FrameSinkId, std::vector<SurfaceRange>> damage_ranges_;

  // Used to annotate the aggregated frame for debugging.
  std::unique_ptr<FrameAnnotator> frame_annotator_;

  int64_t display_trace_id_ = -1;
  base::flat_set<SurfaceId> undrawn_surfaces_;

  // Variables used for de-jelly:
  // Whether de-jelly may be active.
  bool de_jelly_enabled_ = false;
  // The set of surfacees being drawn for the first time. Used to determine if
  // de-jelly skew should be applied to a surface.
  base::flat_set<SurfaceId> new_surfaces_;
  // Whether the last drawn frame had de-jelly skew applied.
  bool last_frame_had_jelly_ = false;

  base::WeakPtrFactory<SurfaceAggregator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SurfaceAggregator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_
