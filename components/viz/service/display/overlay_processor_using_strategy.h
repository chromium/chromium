// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_USING_STRATEGY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_USING_STRATEGY_H_

#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/quad_list.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_candidate_temporal_tracker.h"
#include "components/viz/service/display/overlay_combination_cache.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/overlay_processor_strategy.h"
#include "components/viz/service/display/overlay_proposed_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/skia/include/core/SkM44.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/overlay_transform.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(b/181974042):  Remove when color space is plumbed.
#include "ui/gfx/color_space.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace viz {

class DisplayResourceProvider;

// OverlayProcessor subclass that goes through a list of strategies to determine
// overlay candidates. This is used by Android and Ozone platforms.
class VIZ_SERVICE_EXPORT OverlayProcessorUsingStrategy
    : public OverlayProcessorInterface {
 public:
  using CandidateList = OverlayCandidateList;

  OverlayProcessorUsingStrategy(const OverlayProcessorUsingStrategy&) = delete;
  OverlayProcessorUsingStrategy& operator=(
      const OverlayProcessorUsingStrategy&) = delete;

  ~OverlayProcessorUsingStrategy() override;

  gfx::Rect GetPreviousFrameOverlaysBoundingRect() const final;
  gfx::Rect GetAndResetOverlayDamage() final;

  // Override OverlayProcessor.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  void SetViewportSize(const gfx::Size& size) override {}
  void SetFrameSequenceNumber(uint64_t frame_sequence_number_) override;
  // Attempts to replace quads from the specified root render pass with
  // overlays. This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkM44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds)
      // TODO(petermcneeley) : Restore to "final" once
      // |OverlayProcessorDelegated| has been reintegrated into
      // |OverlayProcessorOzone|.
      override;

  // This function takes a pointer to the std::optional instance so the
  // instance can be reset. When overlay strategy covers the entire output
  // surface, we no longer need the output surface as a separate overlay. This
  // is also used by SurfaceControl to adjust rotation.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  void AdjustOutputSurfaceOverlay(
      std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;

  OverlayProcessorUsingStrategy();

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited. Candidates with |overlay_handled| set to
  // true must also have their |display_rect| converted to integer
  // coordinates if necessary.
  void CheckOverlaySupport(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidate_list);

  // Clears the cache of attempted overlay combinations and their results.
  void ClearOverlayCombinationCache();

  // This should be called during overlay processing to register whether or not
  // there is a candidate that requires an overlay so that the manager can allow
  // the overlay on the display with the requirement only.
  virtual void RegisterOverlayRequirement(bool requires_overlay) {}

  // Disable overlay if there has been a copy request in the last 10 frames
  // 10 was chosen because worst case the copy request might be 15 fps and
  // we might have display with 120 Hz.
  static const int kCopyRequestSkipOverlayFrames = 10;

 protected:
  virtual gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const;

  std::vector<std::unique_ptr<OverlayProcessorStrategy>> strategies_;
  raw_ptr<OverlayProcessorStrategy> last_successful_strategy_ = nullptr;

  gfx::Rect overlay_damage_rect_;
  gfx::Rect previous_frame_overlay_rect_;

  struct OverlayPrioritizationConfig {
    // Threshold criteria required for a proposed candidate to be considered for
    // overlay promotion
    bool changing_threshold = true;
    bool damage_rate_threshold = true;

    // Sorting criteria that determines the relative order of consideration for
    // a overlay candidate.
    bool power_gain_sort = true;
  };

  OverlayPrioritizationConfig prioritization_config_;
  OverlayCandidateTemporalTracker::Config tracker_config_;

  // This is controlled by the "UseMultipleOverlays" feature's "max_overlays"
  // param.
  const int max_overlays_config_;
  // This will remain 1 until hardware support for more than one overlay is
  // confirmed in `OverlayProcessorOzone::ReceiveHardwareCapabilities`.
  int max_overlays_considered_ = 1;

#if BUILDFLAG(IS_CHROMEOS_ASH)
 protected:
  // TODO(b/181974042):  Remove when color space is plumbed.
  gfx::ColorSpace primary_plane_color_space_;
#endif

 private:
  // Keeps track of overlay information needed to update damage correctly.
  struct OverlayStatus;
  using OverlayStatusMap = std::map<gfx::Rect, OverlayStatus>;

  struct OverlayStatus {
    OverlayStatus() = delete;
    OverlayStatus(const OverlayCandidate& candidate,
                  const gfx::Rect& key,
                  const OverlayStatusMap& prev_overlays);
    OverlayStatus(const OverlayStatus&);
    OverlayStatus& operator=(const OverlayStatus&);
    ~OverlayStatus();

    gfx::Rect overlay_rect;
    gfx::RectF damage_rect;
    uint32_t damage_index;
    float damage_area_estimate;
    bool has_mask_filter;
    int plane_z_order;
    bool is_underlay;
    bool is_opaque;
    bool is_new;
    bool prev_was_opaque;
    bool prev_was_underlay;
    bool prev_has_mask_filter;
  };

  // The platform specific implementation to check overlay support that will be
  // called by `CheckOverlaySupport()`.
  virtual void CheckOverlaySupportImpl(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidate_list) = 0;

  // Updates |damage_rect| by removing damage caused by overlays.
  void UpdateDamageRect(const SurfaceDamageRectList& surface_damage_rect_list,
                        gfx::Rect& damage_rect);
  gfx::Rect ComputeDamageExcludingOverlays(
      const SurfaceDamageRectList& surface_damage_rect_list,
      const gfx::Rect& existing_damage);

  // Iterates through a list of strategies and attempts to overlay with each.
  // Returns true if one of the attempts is successful. Has to be called after
  // InitializeStrategies(). A |primary_plane| represents the output surface's
  // buffer that comes from |BufferQueue|. It is passed in here so it could be
  // pass through to hardware through CheckOverlaySupport. It is not passed
  // through as a const member because the underlay strategy changes the
  // |primary_plane|'s blending setting.
  bool AttemptWithStrategies(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds,
      gfx::Rect* incoming_damage);

  // Skips overlay when we have recently had copy output requests
  // on root render pass to avoid flickering during screen capture
  bool BlockForCopyRequests(const AggregatedRenderPass* root_render_pass);

  // Determines if we should attempt multiple overlays. This is based on
  // `max_overlays_considered_`, the strategies proposed, and if any of the
  // candidates require an overlay.
  bool ShouldAttemptMultipleOverlays(
      const std::vector<OverlayProposedCandidate>& sorted_candidates);

  // Attempts to promote multiple candidates to overlays. Returns a boolean
  // indicating if any of the attempted candidates were successfully promoted to
  // overlays.
  //
  // TODO(khaslett): Write unit tests for this function before launching
  // UseMultipleOverlays feature.
  bool AttemptMultipleOverlays(
      const std::vector<OverlayProposedCandidate>& sorted_candidates,
      OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      AggregatedRenderPass* render_pass,
      OverlayCandidateList& candidates);

  // Assigns `plane_z_order`s to the proposed underlay candidates based on their
  // DrawQuad orderings.
  //
  // TODO(khaslett): Write unit tests for this function before launching
  // UseMultipleOverlays feature.
  void AssignUnderlayZOrders(
      std::vector<std::vector<OverlayProposedCandidate>::iterator>&
          underlay_iters);

  // This function reorders and removes |proposed_candidates| based on a
  // heuristic designed to maximize the effectiveness of the limited number
  // of Hardware overlays. Effectiveness here is primarily about power and
  // secondarily about of performance.
  virtual void SortProposedOverlayCandidates(
      std::vector<OverlayProposedCandidate>* proposed_candidates);

  // Used by Android pre-SurfaceControl to notify promotion hints, and by
  // Ozone to notify overlay manager what overlays are actually promoted.
  virtual void NotifyOverlayPromotion(
      DisplayResourceProvider* display_resource_provider,
      const OverlayCandidateList& candidate_list,
      const QuadList& quad_list);

  // Used to update |min_working_scale_| and |max_failed_scale_|. |scale_factor|
  // should be the src->dst scaling amount that is < 1.0f and |success| should
  // be whether that scaling worked or not.
  void UpdateDownscalingCapabilities(float scale_factor, bool success);

  // Moves `curr_overlays` into `prev_overlays`, and updates `curr_overlays` to
  // reflect the overlays that will be promoted this frame in `candidates`.
  void UpdateOverlayStatusMap(const OverlayCandidateList& candidates);

  std::unordered_map<ProposedCandidateKey,
                     OverlayCandidateTemporalTracker,
                     ProposedCandidateKeyHasher>
      tracked_candidates_;

  // These variables are used only for UMA purposes.
  void OnOverlaySwitchUMA(ProposedCandidateKey overlay_tracking_key);
  base::TimeTicks last_time_interval_switch_overlay_tick_;
  ProposedCandidateKey prev_overlay_tracking_id_;
  uint64_t frame_sequence_number_ = 0;

  // These values are used for tracking how much we can downscale with overlays
  // and is used for when we require an overlay so we can determine how much we
  // can downscale without failing.
  float min_working_scale_ = 1.0f;
  float max_failed_scale_ = 0.0f;

  // These keep track of the status of promoted overlays from one frame to the
  // next. These maps are updated by calling UpdateOverlayStatusMap(), and are
  // used by UpdateDamageRect() to update damage properly.
  OverlayStatusMap prev_overlays_;
  OverlayStatusMap curr_overlays_;

  OverlayCombinationCache overlay_combination_cache_;

  // Used to count the number of frames we should wait until enabling overlay
  // again.
  int copy_request_counter_ = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_USING_STRATEGY_H_
