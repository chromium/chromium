// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_

#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/resolved_frame_data.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/overlay_transform.h"

namespace viz {
class DisplayResourceProvider;
class Surface;
class SurfaceDrawQuad;
class SurfaceManager;

struct MaskFilterInfoExt;

class VIZ_SERVICE_EXPORT SurfaceAggregator : public SurfaceObserver {
 public:
  using FrameSinkIdMap = base::flat_map<FrameSinkId, LocalSurfaceId>;

  // To control when to add an extra render pass to avoid readback from the
  // root render pass. This is useful for root pass drawing to vulkan secondary
  // command buffer, which does not support readback.
  enum class ExtraPassForReadbackOption {
    // No special handling needed.
    kNone,
    // Add an extra render pass only if readback is needed.
    kAddPassForReadback,
    // Always add an extra pass. Useful for debugging.
    kAlwaysAddPass,
  };

  // Interface that can modify the aggregated CompositorFrame to annotate it.
  // For example it could add extra quads.
  class FrameAnnotator {
   public:
    virtual ~FrameAnnotator() = default;

    virtual void AnnotateAggregatedFrame(AggregatedFrame* frame) = 0;
  };

  SurfaceAggregator(SurfaceManager* manager,
                    DisplayResourceProvider* provider,
                    bool needs_surface_damage_rect_list,
                    ExtraPassForReadbackOption extra_pass_option =
                        ExtraPassForReadbackOption::kNone,
                    bool prevent_merging_surfaces_to_root_pass = false);

  SurfaceAggregator(const SurfaceAggregator&) = delete;
  SurfaceAggregator& operator=(const SurfaceAggregator&) = delete;

  ~SurfaceAggregator() override;

  // These constants are used for all time related metrics recorded in
  // SurfaceAggregator.
  static constexpr base::TimeDelta kHistogramMinTime = base::Microseconds(5);
  static constexpr base::TimeDelta kHistogramMaxTime = base::Milliseconds(10);
  static constexpr int kHistogramTimeBuckets = 50;

  // |target_damage| represents an area on the output surface that might have
  // been invalidated. It can be used in cases where we still want to support
  // partial damage but the target surface might need contents outside the
  // damage rect of the root surface.
  AggregatedFrame Aggregate(const SurfaceId& surface_id,
                            base::TimeTicks expected_display_time,
                            gfx::OverlayTransform display_transform,
                            const gfx::Rect& target_damage = gfx::Rect(),
                            int64_t display_trace_id = -1);

  // Returns latest frame data after previous Aggregate() call. This only valid
  // until next CompositorFrame is processed, so should be called directly after
  // Aggregate() to make sure no CompositorFrame did arrive between the calls.
  const ResolvedFrameData* GetLatestFrameData(const SurfaceId& surface_id);

  const base::flat_set<SurfaceId>& previous_contained_surfaces() const {
    return previous_contained_surfaces_;
  }
  const FrameSinkIdMap& previous_contained_frame_sinks() const {
    return previous_contained_frame_sinks_;
  }
  void SetFullDamageForSurface(const SurfaceId& surface_id);
  void set_output_is_secure(bool secure) { output_is_secure_ = secure; }
  void set_take_copy_requests(bool value) { take_copy_requests_ = value; }

  // Set the color spaces for the created RenderPasses, which is propagated
  // to the output surface.
  void SetDisplayColorSpaces(const gfx::DisplayColorSpaces& color_spaces);

  void SetMaxRenderTargetSize(int max_size);

  // Checks if damage to `surface_id` potentially contributes to the display
  // damage.
  bool CheckForDisplayDamage(const SurfaceId& surface_id);

  // When a client submits a CompositorFrame without resources it's typically
  // done to force return of existing resources to the client. This function
  // forces the release in this cases. Returns true if some resources were
  // released and draw needs to happen for DisplayResourceProvider to unlock
  // them.
  bool ForceReleaseResourcesIfNeeded(const SurfaceId& surface_id);

  bool HasFrameAnnotator() const;
  void SetFrameAnnotator(std::unique_ptr<FrameAnnotator> frame_annotator);
  void DestroyFrameAnnotator();

 private:
  struct PrewalkResult;

  struct AggregateStatistics {
    int prewalked_surface_count = 0;
    int copied_surface_count = 0;
    // True if the current frame contains a pixel-moving foreground filter
    // render pass.
    bool has_pixel_moving_filter = false;
    // True if the current frame contains a unembedded render pass.
    bool has_unembedded_pass = false;

    base::TimeDelta prewalk_time;
    base::TimeDelta copy_time;
  };

  // SurfaceObserver implementation.
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override;

  // Get resolved frame data for the resolved surfaces active frame. Returns
  // null if there is no matching surface or the surface doesn't have an active
  // CompositorFrame.
  ResolvedFrameData* GetResolvedFrame(const SurfaceRange& range);
  ResolvedFrameData* GetResolvedFrame(const SurfaceId& surface_id);

  // - |source_pass| is the render pass that contains |surface_quad|.
  // - |embedder_client_namespace_id| is portion of layer_id that uniquely
  //   identifies the client which contains |surface_quad|.
  // - |target_transform| is the transform from the coordinate space of
  //   |source_pass| to |dest_pass|.
  // - |added_clip_rect| is an added clip rect in the |dest_pass| coordinate
  //   space.
  // - |dest_root_target_clip_rect| is on the root render pass space of the root
  //   surface, the same coordinate space as |root_damage_rect_|. This is only
  //   used for SurfaceDamageRectList computation and should not be used for
  //   Clipping quads.
  void HandleSurfaceQuad(
      const CompositorRenderPass& source_pass,
      const SurfaceDrawQuad* surface_quad,
      uint32_t embedder_client_namespace_id,
      float parent_device_scale_factor,
      const gfx::Transform& target_transform,
      const std::optional<gfx::Rect> added_clip_rect,
      const std::optional<gfx::Rect> dest_root_target_clip_rect,
      AggregatedRenderPass* dest_pass,
      const MaskFilterInfoExt& mask_filter_info_pair);

  void EmitSurfaceContent(
      ResolvedFrameData& resolved_frame,
      float parent_device_scale_factor,
      const SurfaceDrawQuad* surface_quad,
      uint32_t embedder_client_namespace_id,
      const gfx::Transform& target_transform,
      const std::optional<gfx::Rect> added_clip_rect,
      const std::optional<gfx::Rect> dest_root_target_clip_rect,
      AggregatedRenderPass* dest_pass,
      const MaskFilterInfoExt& mask_filter_info_pair);

  void EmitDefaultBackgroundColorQuad(
      const SurfaceDrawQuad* surface_quad,
      uint32_t embedder_client_namespace_id,
      const gfx::Transform& target_transform,
      const std::optional<gfx::Rect> clip_rect,
      AggregatedRenderPass* dest_pass,
      const MaskFilterInfoExt& mask_filter_info_pair);

  void EmitGutterQuadsIfNecessary(
      const gfx::Rect& primary_rect,
      const gfx::Rect& fallback_rect,
      const SharedQuadState* primary_shared_quad_state,
      uint32_t embedder_client_namespace_id,
      const gfx::Transform& target_transform,
      const std::optional<gfx::Rect> clip_rect,
      SkColor4f background_color,
      AggregatedRenderPass* dest_pass,
      const MaskFilterInfoExt& mask_filter_info_pair);

  void CopyQuadsToPass(
      ResolvedFrameData& resolved_frame,
      ResolvedPassData& resolved_pass,
      AggregatedRenderPass* dest_pass,
      float parent_device_scale_factor,
      const gfx::Transform& target_transform,
      const std::optional<gfx::Rect> clip_rect,
      const std::optional<gfx::Rect> dest_root_target_clip_rect,
      const MaskFilterInfoExt& mask_filter_info_pair);

  // Recursively walks through the render pass and updates the
  // |intersects_damage_under| flag on all RenderPassDrawQuads(RPDQ).
  // The function returns the damage rect of the render pass in its own content
  // space.
  //  - |resolved_frame| is the resolved frame containing the render pass.
  //  - |resolved_pass| contains the render pass data corresponding to the
  //    render pass to be walked.
  //  - |render_pass_map| is a map that contains all render passes and their
  //    entry data.
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
  //  - |parent_pass| is the render pass that embeds |resolved_pass| or null for
  //    the root render pass.
  //  - |result| is the result of a prewalk of the surface that contains the
  //    render pass.
  gfx::Rect PrewalkRenderPass(ResolvedFrameData& resolved_frame,
                              ResolvedPassData& resolved_pass,
                              const gfx::Rect& damage_from_parent,
                              const gfx::Transform& target_to_root_transform,
                              const ResolvedPassData* parent_pass,
                              PrewalkResult& result);

  // Walk the Surface tree from |resolved_frame|. Validate the resources of the
  // current surface and its descendants, check if there are any copy requests,
  // and return the combined damage rect.
  gfx::Rect PrewalkSurface(ResolvedFrameData& resolved_frame,
                           ResolvedPassData* parent_pass,
                           const gfx::Rect& damage_from_parent,
                           PrewalkResult& result);

  void CopyUndrawnSurfaces(PrewalkResult* prewalk);
  void CopyPasses(ResolvedFrameData& resolved_frame);
  void AddColorConversionPass();
  void AddRootReadbackPass();
  void AddDisplayTransformPass();
  void AddRenderPassHelper(AggregatedRenderPassId render_pass_id,
                           const gfx::Rect& render_pass_output_rect,
                           const gfx::Rect& render_pass_damage_rect,
                           gfx::ContentColorUsage pass_color_usage,
                           bool pass_has_transparent_background,
                           bool pass_is_color_conversion_pass,
                           const gfx::Transform& quad_state_to_target_transform,
                           bool quad_state_contents_opaque,
                           SkBlendMode quad_state_blend_mode,
                           AggregatedRenderPassId quad_pass_id);

  // Remove Surfaces that were referenced before but aren't currently
  // referenced from the ResourceProvider.
  // Also notifies SurfaceAggregatorClient of newly added and removed
  // child surfaces.
  void ProcessAddedAndRemovedSurfaces();

  void MarkAndPropagateCopyRequestPasses(ResolvedPassData& resolved_pass);

  bool CheckFrameSinksChanged(const SurfaceId& surface_id);

  // This function adds a damage rect to |surface_damage_rect_list_|. The
  // surface damage rect comes from |resolved_frame| if provided, otherwise
  // |default_damage_rect| will be used.
  //
  // |dest_root_target_clip_rect| is on the root render pass space of the root
  // surface, the same coordinate space as |root_damage_rect_|.
  //
  // |surface_damage_rect_list_| is different from the |root_damage_rect_| which
  // is the union of all surface damages. This function records per-surface
  // damage rects to |surface_damage_rect_list_| in a top-to-bottom order and is
  // called at each surface in the frame.
  void AddSurfaceDamageToDamageList(
      const gfx::Rect& default_damage_rect,
      const gfx::Transform& parent_target_transform,
      const std::optional<gfx::Rect> dest_root_target_clip_rect,
      const gfx::Transform& dest_transform_to_root_target,
      ResolvedFrameData* resolved_frame);

  void AddRenderPassFilterDamageToDamageList(
      const ResolvedFrameData& resolved_frame,
      const CompositorRenderPassDrawQuad* render_pass_quad,
      const gfx::Transform& parent_target_transform,
      const std::optional<gfx::Rect> dest_root_target_clip_rect,
      const gfx::Transform& dest_transform_to_root_target);

  // Determine the overlay damage and location in the surface damage list.
  const DrawQuad* FindQuadWithOverlayDamage(
      const CompositorRenderPass& source_pass,
      AggregatedRenderPass* dest_pass,
      const gfx::Transform& pass_to_root_target_transform,
      size_t* overlay_damage_index);

  bool IsRootSurface(const Surface* surface) const;

  // This method transforms the delegated ink metadata to be in the root target
  // space, so that it can eventually be drawn onto the back buffer in the
  // correct position. It should only ever be called when a frame contains
  // delegated ink metadata, in which case this function will transform it and
  // then store it in the |delegated_ink_metadata_| member.
  void TransformAndStoreDelegatedInkMetadata(
      const gfx::Transform& parent_quad_to_root_target_transform,
      const gfx::DelegatedInkMetadata* metadata,
      const AggregatedRenderPassId render_pass_id);

  // Preliminary check to see if a surface contained in |surface_quad| can
  // potentially merge its root render pass. If so, returns true.
  static bool CanPotentiallyMergePass(const SurfaceDrawQuad& surface_quad);

  // Logs the surface information for debugging purposes.
  void DebugLogSurface(const Surface* surface, bool will_draw);

  // Records UMA histograms and resets |stats_|.
  void RecordStatHistograms();

  // Resets member variables that were used during Aggregate().
  void ResetAfterAggregate();

  void SetRenderPassDamageRect(AggregatedRenderPass* copy_pass,
                               ResolvedPassData& resolved_pass);

  const raw_ptr<SurfaceManager> manager_;
  const raw_ptr<DisplayResourceProvider> provider_;

  // If true, per-surface damage rect list will be produced.
  const bool needs_surface_damage_rect_list_;

  const ExtraPassForReadbackOption extra_pass_for_readback_option_;

  // If true, don't merge surfaces in the root render pass. This means renderer
  // frames get put into their own RPDQ overlay. Note that if delegated
  // compositing on the UI quads fails, we will end up with an unnecessary
  // render pass backing since we can't re-merge these surfaces after overlay
  // processing.
  const bool prevent_merging_surfaces_to_root_pass_ = false;

  // Will be true for duration of Aggregate() function.
  bool is_inside_aggregate_ = false;

  bool output_is_secure_ = false;

  // Whether |CopyOutputRequests| should be moved over to the aggregated frame.
  bool take_copy_requests_ = true;

  // The color space for the root render pass. If this is different from its
  // blending color space (e.g. for HDR), then a final render pass to convert
  // between the two will be added. This space must always be valid.
  gfx::DisplayColorSpaces display_color_spaces_;

  // Maximum texture size which if larger than zero, will limit the size of
  // render passes.
  int max_render_target_size_ = 0;
  // The id for the final color conversion render pass.
  AggregatedRenderPassId color_conversion_render_pass_id_;
  // The id for the extra pass added to avoid readback from root pass.
  AggregatedRenderPassId readback_render_pass_id_;
  // The id for the optional render pass used to apply the display transform.
  AggregatedRenderPassId display_transform_render_pass_id_;

  // Persistent storage for ResolvedFrameData.
  std::map<SurfaceId, ResolvedFrameData> resolved_frames_;

  // The following state is only valid for the duration of one Aggregate call
  // and is only stored on the class to avoid having to pass through every
  // function call.

  // This is the set of surfaces referenced in the aggregation so far, used to
  // detect cycles.
  base::flat_set<SurfaceId> referenced_surfaces_;

  SurfaceId root_surface_id_;
  gfx::Transform root_surface_transform_;

  std::optional<AggregateStatistics> stats_;

  // For each Surface used in the last aggregation, gives the frame_index at
  // that time.
  base::flat_set<SurfaceId> previous_contained_surfaces_;
  base::flat_set<SurfaceId> contained_surfaces_;
  FrameSinkIdMap previous_contained_frame_sinks_;
  FrameSinkIdMap contained_frame_sinks_;

  // This is the pass list for the aggregated frame.
  raw_ptr<AggregatedRenderPassList> dest_pass_list_ = nullptr;

  // The target display time for the aggregated frame.
  base::TimeTicks expected_display_time_;
  int64_t display_trace_id_ = -1;

  // Map from SurfaceRange to SurfaceId for current aggregation.
  base::flat_map<SurfaceRange, SurfaceId> resolved_surface_ranges_;

  // The root damage rect of the currently-aggregating frame.
  gfx::Rect root_damage_rect_;

  // A pointer to the list of surface damage rects from the current
  // AggregatedFrame, used for overlay optimization.
  raw_ptr<SurfaceDamageRectList> surface_damage_rect_list_;

  // The aggregate color content usage of the currently-aggregating frame. This
  // is computed by the prewalk, and is used to determine the format and color
  // space of all render passes. Note that that is more heavy-handed than is
  // desirable.
  gfx::ContentColorUsage root_content_color_usage_ =
      gfx::ContentColorUsage::kSRGB;

  // True if the frame that's currently being aggregated has copy requests.
  // This is valid during Aggregate after PrewalkSurface is called.
  bool has_copy_requests_ = false;

  // True if any RenderPasses in the aggregated frame have a backdrop filter
  // that moves pixels. This is valid during Aggregate after PrewalkSurface is
  // called.
  bool has_pixel_moving_backdrop_filter_ = false;

  // For each FrameSinkId, contains a vector of SurfaceRanges that will damage
  // the display if they're damaged.
  base::flat_map<FrameSinkId, std::vector<SurfaceRange>> damage_ranges_;

  // Used to annotate the aggregated frame for debugging.
  std::unique_ptr<FrameAnnotator> frame_annotator_;

  // Used to avoid excessive UMA logging per frame.
  base::MetricsSubSampler metrics_subsampler_;

  // Whether the last drawn frame had a color conversion pass applied. Used in
  // production on Windows only (does not interact with jelly).
  bool last_frame_had_color_conversion_pass_ = false;
  // Whether last frame had an extra render pass added to avoid readback from
  // root frame buffer.
  bool last_frame_had_readback_pass_ = false;

  // The metadata used for drawing a delegated ink trail on the end of a normal
  // ink stroke. It needs to be transformed to root coordinates and then put on
  // the final aggregated frame. This is only populated during aggregation when
  // a surface contains delegated ink metadata on its frame, and it is cleared
  // after it is placed on the final aggregated frame during aggregation.
  std::unique_ptr<gfx::DelegatedInkMetadata> delegated_ink_metadata_;
  // Whether the last aggregated frame contained delegated ink metadata or not.
  // Used to determine if the root render pass needs to remain expanded by the
  // target damage or not, because that allows a frame to be drawn after inking
  // is finished to remove the last drawn ink trail.
  bool last_frame_had_delegated_ink_ = false;
  // Tracks the timestamp of the delegated ink metadata that is being added to
  // the aggregated frame in `Aggregate()`. The role of this member is to track
  // consecutive aggregate frames with the same delegated ink metadata in the
  // event that there are no new compositor frames but delegated ink points are
  // still being sent to viz from the browser process.
  base::TimeTicks previous_ink_metadata_time_;
  // Tracks the number of consecutive aggregate frames with the same delegated
  // ink metadata.
  int identical_ink_metadata_count_ = 0;

  // The current surface has zero_damage_rect and is not recorded in
  // surface_damage_rect_list_ . Set by AddSurfaceDamageToDamageList() and read
  // by FindQuadWithOverlayDamage().
  bool current_zero_damage_rect_is_not_recorded_ = false;

  // Used to generate new unique render pass ids in the aggregated namespace.
  AggregatedRenderPassId::Generator render_pass_id_generator_;

  // Flow ids for aggregated frames. Used for tracing.
  std::unordered_set<int64_t> flow_ids_for_resolved_frames_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SURFACE_AGGREGATOR_H_
