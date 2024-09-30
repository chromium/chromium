// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_

#include <vector>

#include "base/check_is_test.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/direct_composition_support.h"

namespace viz {
class DisplayResourceProvider;

class VIZ_SERVICE_EXPORT DCLayerOverlayProcessor final
    : public gl::DirectCompositionOverlayCapsObserver,
      public base::PowerStateObserver {
 public:
  using FilterOperationsMap =
      base::flat_map<AggregatedRenderPassId,
                     raw_ptr<cc::FilterOperations, CtnExperimental>>;
  // When |skip_initialization_for_testing| is true, object will be isolated
  // for unit tests.
  explicit DCLayerOverlayProcessor(
      int allowed_yuv_overlay_count,
      bool disable_video_overlay_if_moving,
      bool skip_initialization_for_testing = false);

  DCLayerOverlayProcessor(const DCLayerOverlayProcessor&) = delete;
  DCLayerOverlayProcessor& operator=(const DCLayerOverlayProcessor&) = delete;

  ~DCLayerOverlayProcessor() override;

  // Encapsulates all of the information about a render pass's overlays that
  // are returned back to OverlayProcessorWin. This is passed to Process() as an
  // in/out parameter.
  struct VIZ_SERVICE_EXPORT RenderPassOverlayData {
    RenderPassOverlayData();
    ~RenderPassOverlayData();

    RenderPassOverlayData(RenderPassOverlayData&&);
    RenderPassOverlayData& operator=(RenderPassOverlayData&&);

    // Damage rect of the render pass. Set by OverlayProcessorWin and may be
    // optimized in UpdateDamageRect() if overlays are promoted.
    gfx::Rect damage_rect;

    // List of overlays that are actually promoted. Only used for output back to
    // OverlayProcessorWin. Contains all the information necessary to draw the
    // overlay quads in SkiaRenderer.
    OverlayCandidateList promoted_overlays;
  };
  using RenderPassOverlayDataMap =
      base::flat_map<raw_ptr<AggregatedRenderPass>, RenderPassOverlayData>;

  // Virtual for testing. All render passes that should be considered for
  // overlays in this frame should be in |render_pass_overlay_data_map|. After
  // this function executes, |render_pass_overlay_data_map[render_pass]| will
  // contain the all of the overlays promoted for |render_pass|. The z-order
  // of the overlays are assigned relative to other overlays within the render
  // pass, with positive z-orders being overlays and negative z-orders being
  // underlays. The caller must aggregate overlays from all render passes into
  // a global overlay list, taking into account the render pass's z-order.
  virtual void Process(
      const DisplayResourceProvider* resource_provider,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      const SurfaceDamageRectList& surface_damage_rect_list_in_root_space,
      bool is_page_fullscreen_mode,
      RenderPassOverlayDataMap& render_pass_overlay_data_map);

  // DirectCompositionOverlayCapsObserver implementation.
  void OnOverlayCapsChanged() override;
  // base::PowerStateObserver implementation.
  void OnBatteryPowerStatusChange(
      PowerStateObserver::BatteryPowerStatus battery_power_status) override;

  void UpdateHasHwOverlaySupport();
  void UpdateSystemHDRStatus();
  void UpdateP010VideoProcessorSupport();
  void UpdateAutoHDRVideoProcessorSupport();

  void set_frames_since_last_qualified_multi_overlays_for_testing(int value) {
    frames_since_last_qualified_multi_overlays_ = value;
  }
  void set_system_hdr_enabled_on_any_display_for_testing(bool value) {
    system_hdr_enabled_on_any_display_ = value;
  }
  void set_system_hdr_disabled_on_any_display_for_testing(bool value) {
    system_hdr_disabled_on_any_display_ = value;
  }
  void set_has_p010_video_processor_support_for_testing(bool value) {
    has_p010_video_processor_support_ = value;
  }
  void set_has_auto_hdr_video_processor_support_for_testing(bool value) {
    has_auto_hdr_video_processor_support_ = value;
  }
  void set_is_on_battery_power_for_testing(bool value) {
    is_on_battery_power_ = value;
  }
  void set_disable_video_overlay_if_moving_for_testing(bool value) {
    disable_video_overlay_if_moving_ = value;
  }
  bool force_overlay_for_auto_hdr() {
    return system_hdr_enabled_on_any_display_ &&
           has_auto_hdr_video_processor_support_ && !is_on_battery_power_;
  }
  size_t get_previous_frame_render_pass_count() const {
    CHECK_IS_TEST();
    return previous_frame_render_pass_states_.size();
  }
  std::vector<AggregatedRenderPassId> get_previous_frame_render_pass_ids()
      const {
    std::vector<AggregatedRenderPassId> ids;
    for (const auto& [id, _] : previous_frame_render_pass_states_) {
      ids.push_back(id);
    }
    return ids;
  }

  // This struct only contains minimal information about the overlays, enough to
  // perform damage optimizations across frames.
  struct OverlayRect {
    gfx::Rect rect;
    bool is_overlay = true;  // If false, it's an underlay.
    friend bool operator==(const OverlayRect&, const OverlayRect&) = default;
  };

  // Promote a single quad in isolation, like how |Process| would internally.
  // This ignores per-frame limitations such as max number of YUV quads, etc.
  // This also adds other properties needed for delegated compositing.
  std::optional<OverlayCandidate> FromTextureOrYuvQuad(
      const DisplayResourceProvider* resource_provider,
      const AggregatedRenderPass* render_pass,
      const QuadList::ConstIterator& it,
      bool is_page_fullscreen_mode) const;

 private:
  // Information about a render pass's overlays from the previous frame. The
  // previous frame's overlays are used for optimizations, which are done
  // independently for each render pass. These optimizations try to remove
  // render pass packing damage if the overlays are not changed between frames,
  // which potentially allows us to skip drawing the render pass. We also add
  // damage from overlays in the previous frame in the scenarios where we skip
  // overlays in the current frame or if the overlays have changed. This damage
  // needs to be re-added because the content under the overlays from the
  // previous frame are likely out of date if they were optimized out.
  struct RenderPassPreviousFrameState {
    RenderPassPreviousFrameState();
    ~RenderPassPreviousFrameState();

    RenderPassPreviousFrameState(RenderPassPreviousFrameState&&);
    RenderPassPreviousFrameState& operator=(
        RenderPassPreviousFrameState&& other);

    // Whether the render pass had any promoted underlay quads that were opaque
    // in the previous frame.
    bool underlay_is_opaque = true;

    // The output rect of the render pass in the previous frame.
    gfx::Rect display_rect;

    // Rects of all overlay and underlay quads that were promoted in the
    // previous frame.
    std::vector<OverlayRect> overlay_rects;
  };

  // Information about a render pass's overlays in the current frame being
  // processed. This struct primarily serves to encapsulate all parameters
  // relating to a render pass into one object that can be passed between
  // multiple functions. These objects do not persist after this current frame
  // is processed. While RenderPassOverlayData stores information that are
  // exposed and returned to OverlayProcessorWin, this struct contains data used
  // only internally to this class.
  struct RenderPassCurrentFrameState {
    RenderPassCurrentFrameState();
    ~RenderPassCurrentFrameState();

    RenderPassCurrentFrameState(RenderPassCurrentFrameState&&);
    RenderPassCurrentFrameState& operator=(RenderPassCurrentFrameState&& other);

    // The surface damage rect list for the frame, in *render pass space*.
    SurfaceDamageRectList surface_damage_rect_list;

    // Overlay quad candidates in the render pass's quad list. These are
    // overlays that have been identified as potential candidates for promotion
    // and are collected in the initial stage of processing. Some or all of
    // these candidates may or may not be actually promoted. We're storing
    // iterators instead of the actual quad because some functions such as
    // IsPossiblefullScreenLetterboxing and ProcessForUnderlay require knowing
    // the position of the quad in the quad list.
    std::vector<QuadList::Iterator> candidates;

    // Rects of overlays that have been processed and successfully promoted and
    // added to |RenderPassOverlayData::promoted_overlays|.
    std::vector<OverlayRect> overlay_rects;

    // Overlay damages that can be removed from the render pass's damage rect
    // at the end of processing overlays. This vector stores indices of damages
    // in |surface_damage_rect_list| that can be removed.
    std::vector<size_t> damages_to_be_removed;
  };
  using RenderPassCurrentFrameStateMap =
      base::flat_map<raw_ptr<AggregatedRenderPass>,
                     RenderPassCurrentFrameState>;

  // Information about overlays in the current frame being processed. Unlike
  // fields in RenderPassCurrentFrameState, these fields are not specific to any
  // render pass. They are global to the entire frame. Similarly, this struct
  // exists primarily to encapsulate variables into one object to pass between
  // functions.
  struct GlobalOverlayState {
    // Actual number of yuv quads that are successfully processed and added as
    // an overlay. Used to determine whether overlay should be skipped.
    int processed_yuv_overlay_count = 0;

    // Total number of yuv quads.
    int yuv_quads = 0;

    // Number of yuv quads that were considered for overlay promotion and have a
    // non-empty surface damage.
    int damaged_yuv_quads = 0;

    // Tracks whether we have anything other than clear video overlays e.g. low
    // latency canvas or protected video which are allowed for multiple
    // overlays.
    bool has_non_clear_video_overlays = false;

    // Used for recording overlay histograms.
    bool has_occluding_damage_rect = false;

    // Whether to reject all overlays for this frame. This can be true if we
    // have more than one overlay quad and not all of them are promoted to
    // overlays.
    bool reject_overlays = false;
  };

  // Collects the overlay candidates for a render pass. Coordinate systems for
  // all parameters should be in render pass space.
  //
  // If video capture is enabled, overlays are not processed. In this case, the
  // render pass's previous frame data is erased since there will be no overlays
  // in the current frame.
  //
  // This function adds overlay candidates for |render_pass| into
  // |render_pass_state| and accumulates information about |render_pass|
  // into |global_overlay_state|. If overlays should be skipped for this
  // render pass, the damage rect in |overlay_data| is unioned with the previous
  // frame's overlay damages, and the previous frame state is cleared.
  void CollectCandidates(
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const FilterOperationsMap& render_pass_backdrop_filters,
      RenderPassOverlayData& overlay_data,
      RenderPassCurrentFrameState& render_pass_state,
      GlobalOverlayState& global_overlay_state);

  // Promotes overlay candidates for a render pass. Coordinate systems for all
  // parameters should be in in render pass space.
  //
  // The render pass's corresponding RenderPassPreviousFrameState object in
  // |previous_frame_overlay_candidate_rects_| is updated to contain this
  // frame's data.
  //
  // This function adds overlays that have been promoted into |overlay_data|
  // and accumulates their rects into the damage rect. It also updates all of
  // |current_frame_state|'s fields and |processed_yuv_overlay_count| to reflect
  // the actual number of overlays promoted.
  void PromoteCandidates(
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const FilterOperationsMap& render_pass_filters,
      const RenderPassPreviousFrameState& previous_frame_state,
      bool is_page_fullscreen_mode,
      RenderPassOverlayData& overlay_data,
      RenderPassCurrentFrameState& current_frame_state,
      GlobalOverlayState& global_overlay_state);

  // Detects overlay processing skip inside |render_pass|.
  bool ShouldSkipOverlay(AggregatedRenderPass* render_pass) const;

  // Creates an OverlayCandidate for a quad candidate and updates the states
  // for the render pass.
  void UpdateDCLayerOverlays(
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPass* render_pass,
      const QuadList::Iterator& it,
      const gfx::Rect& quad_rectangle_in_target_space,
      const RenderPassPreviousFrameState& previous_frame_state,
      bool is_overlay,
      bool is_page_fullscreen_mode,
      RenderPassOverlayData& overlay_data,
      RenderPassCurrentFrameState& current_frame_state,
      GlobalOverlayState& global_overlay_state);

  void ProcessForOverlay(
      AggregatedRenderPass* render_pass,
      const QuadList::Iterator& it,
      const RenderPassPreviousFrameState& previous_frame_state,
      RenderPassCurrentFrameState& current_frame_state) const;
  void ProcessForUnderlay(
      AggregatedRenderPass* render_pass,
      const QuadList::Iterator& it,
      const gfx::Rect& quad_rectangle_in_target_space,
      const RenderPassPreviousFrameState& previous_frame_state,
      const GlobalOverlayState& global_overlay_state,
      RenderPassOverlayData& overlay_data,
      RenderPassCurrentFrameState& current_frame_state,
      OverlayCandidate& dc_layer);

  void UpdateDamageRect(
      AggregatedRenderPass* render_pass,
      const RenderPassPreviousFrameState& previous_frame_state,
      RenderPassOverlayData& overlay_data,
      RenderPassCurrentFrameState& current_frame_state) const;

  void RemoveOverlayDamageRect(
      const QuadList::Iterator& it,
      RenderPassCurrentFrameState& render_pass_state) const;

  // Remove all video overlay candidates if any overlays in any render passes
  // have moved in the last several frames.
  //
  // We do this because it could cause visible stuttering of playback on certain
  // older hardware. The stuttering does not occur if other overlay quads move
  // while a non-moving video is playing.
  //
  // This only tracks clear video quads because hardware-protected videos cannot
  // be accessed by the viz compositor, so they must be promoted to overlay,
  // even if they could cause stutter. Software-protected video aren't required
  // to be in overlay, but we also exclude them from de-promotion to keep the
  // protection benefits of being in an overlay.
  void RemoveClearVideoQuadCandidatesIfMoving(
      const DisplayResourceProvider* resource_provider,
      RenderPassOverlayDataMap& render_pass_overlay_data_map,
      RenderPassCurrentFrameStateMap& render_pass_current_state_map);

  bool has_overlay_support_;
  bool has_p010_video_processor_support_ = false;
  bool has_auto_hdr_video_processor_support_ = false;
  // At least one monitor that has system HDR enabled.
  bool system_hdr_enabled_on_any_display_ = false;
  // At least one monitor that has system HDR disabled or doesn't support HDR.
  bool system_hdr_disabled_on_any_display_ = true;
  const int allowed_yuv_overlay_count_;
  uint64_t frames_since_last_qualified_multi_overlays_ = 0;

  bool allow_promotion_hinting_ = false;
  bool is_on_battery_power_ = false;

  // Information about overlays from the previous frame.
  base::flat_map<AggregatedRenderPassId, RenderPassPreviousFrameState>
      previous_frame_render_pass_states_;

  // Used in `RemoveClearVideoQuadCandidatesIfMoving`
  // List of clear video content candidate bounds. These rects are in root space
  // and contains the candidate rects for all render passes.
  // TODO(crbug.com/40272272): Compute these values using
  // |previous_frame_render_pass_states_| and remove this field.
  std::vector<gfx::Rect> previous_frame_overlay_candidate_rects_;
  int frames_since_last_overlay_candidate_rects_change_ = 0;
  bool no_undamaged_overlay_promotion_;
  bool disable_video_overlay_if_moving_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DC_LAYER_OVERLAY_H_
