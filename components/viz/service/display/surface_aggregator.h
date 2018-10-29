// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_

#include <memory>
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

  SurfaceAggregator(SurfaceManager* manager,
                    DisplayResourceProvider* provider,
                    bool aggregate_only_damaged);
  ~SurfaceAggregator();

  CompositorFrame Aggregate(const SurfaceId& surface_id,
                            base::TimeTicks expected_display_time,
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

  // Set the color spaces for the created RenderPasses, which is propagated
  // to the output surface.
  void SetOutputColorSpace(const gfx::ColorSpace& blending_color_space,
                           const gfx::ColorSpace& output_color_space);

  bool NotifySurfaceDamageAndCheckForDisplayDamage(const SurfaceId& surface_id);

 private:
  struct ClipData {
    ClipData() : is_clipped(false) {}
    ClipData(bool is_clipped, const gfx::Rect& rect)
        : is_clipped(is_clipped), rect(rect) {}

    bool is_clipped;
    gfx::Rect rect;
  };

  struct PrewalkResult {
    PrewalkResult();
    ~PrewalkResult();
    // This is the set of Surfaces that were referenced by another Surface, but
    // not included in a SurfaceDrawQuad.
    base::flat_set<SurfaceId> undrawn_surfaces;
    bool may_contain_video = false;
  };

  struct RenderPassInfo {
    // This is the id the pass is mapped to.
    int id;
    // This is true if the pass was used in the last aggregated frame.
    bool in_use = true;
  };

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
                         bool* damage_rect_in_quad_space_valid);

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
                          bool* damage_rect_in_quad_space_valid);

  void EmitDefaultBackgroundColorQuad(const SurfaceDrawQuad* surface_quad,
                                      const gfx::Transform& target_transform,
                                      const ClipData& clip_rect,
                                      RenderPass* dest_pass);

  void EmitGutterQuadsIfNecessary(
      const gfx::Rect& primary_rect,
      const gfx::Rect& fallback_rect,
      const SharedQuadState* primary_shared_quad_state,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      SkColor background_color,
      RenderPass* dest_pass);

  SharedQuadState* CopySharedQuadState(const SharedQuadState* source_sqs,
                                       const gfx::Transform& target_transform,
                                       const ClipData& clip_rect,
                                       RenderPass* dest_render_pass);

  SharedQuadState* CopyAndScaleSharedQuadState(
      const SharedQuadState* source_sqs,
      const gfx::Transform& scaled_quad_to_target_transform,
      const gfx::Transform& target_transform,
      const gfx::Rect& quad_layer_rect,
      const gfx::Rect& visible_quad_layer_rect,
      const ClipData& clip_rect,
      RenderPass* dest_render_pass,
      float x_scale,
      float y_scale);

  void CopyQuadsToPass(
      const QuadList& source_quad_list,
      const SharedQuadStateList& source_shared_quad_state_list,
      float parent_device_scale_factor,
      const std::unordered_map<ResourceId, ResourceId>& resource_to_child_map,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      RenderPass* dest_pass,
      const SurfaceId& surface_id);
  gfx::Rect PrewalkTree(Surface* surface,
                        bool in_moved_pixel_surface,
                        int parent_pass,
                        bool will_draw,
                        PrewalkResult* result);
  void CopyUndrawnSurfaces(PrewalkResult* prewalk);
  void CopyPasses(const CompositorFrame& frame, Surface* surface);
  void AddColorConversionPass();

  // Remove Surfaces that were referenced before but aren't currently
  // referenced from the ResourceProvider.
  // Also notifies SurfaceAggregatorClient of newly added and removed
  // child surfaces.
  void ProcessAddedAndRemovedSurfaces();

  void PropagateCopyRequestPasses();

  int ChildIdForSurface(Surface* surface);
  gfx::Rect DamageRectForSurface(const Surface* surface,
                                 const RenderPass& source,
                                 const gfx::Rect& full_rect) const;

  static void UnrefResources(base::WeakPtr<SurfaceClient> surface_client,
                             const std::vector<ReturnedResource>& resources);

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

  // The color space for the root render pass. If this is different from
  // |blending_color_space_|, then a final render pass to convert between
  // the two will be added. This space must always be valid.
  gfx::ColorSpace output_color_space_ = gfx::ColorSpace::CreateSRGB();
  // The color space in which blending is done, used for all non-root render
  // passes. This space must always be valid.
  gfx::ColorSpace blending_color_space_ = gfx::ColorSpace::CreateSRGB();
  // The id for the final color conversion render pass.
  RenderPassId color_conversion_render_pass_id_ = 0;

  base::flat_map<SurfaceId, int> surface_id_to_resource_child_id_;

  // The following state is only valid for the duration of one Aggregate call
  // and is only stored on the class to avoid having to pass through every
  // function call.

  // This is the set of surfaces referenced in the aggregation so far, used to
  // detect cycles.
  base::flat_set<SurfaceId> referenced_surfaces_;

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

  // True if the frame that's currently being aggregated has copy requests.
  // This is valid during Aggregate after PrewalkTree is called.
  bool has_copy_requests_;

  // True if the frame that's currently being aggregated has cached render
  // passes. This is valid during Aggregate after PrewalkTree is called.
  bool has_cached_render_passes_;

  // For each FrameSinkId, contains a vector of SurfaceRanges that will damage
  // the display if they're damaged.
  base::flat_map<FrameSinkId, std::vector<SurfaceRange>> damage_ranges_;

  int64_t display_trace_id_ = -1;

  base::WeakPtrFactory<SurfaceAggregator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceAggregator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_
