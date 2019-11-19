// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_RATE_DECIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_RATE_DECIDER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class SurfaceManager;

// The class is used to decide the optimal refresh rate the display should run
// at based on the content sources being updated onscreen and the ideal rate at
// which these sources would like to produce updates.
class VIZ_SERVICE_EXPORT FrameRateDecider : public SurfaceObserver {
 public:
  class VIZ_SERVICE_EXPORT Client {
   public:
    virtual ~Client() = default;

    // Sets the preferred frame interval for the Display.
    virtual void SetPreferredFrameInterval(base::TimeDelta interval) = 0;

    // Queries the frame interval desired for a particular frame sink id.
    virtual base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
        const FrameSinkId& id) = 0;
  };

  // If provided in SetPreferredFrameInterval, this indicates that we don't have
  // any preferred setting and should let the platform decide the display's
  // refresh rate.
  static constexpr base::TimeDelta UnspecifiedFrameInterval() {
    return base::TimeDelta::FromSeconds(0);
  }

  // This object should be created and held for the duration when surface
  // aggregation for a frame to be presented by the display is in progress. It
  // is used by the FrameRateDecider to keep track of surfaces drawn and updated
  // in every frame.
  class VIZ_SERVICE_EXPORT ScopedAggregate {
   public:
    explicit ScopedAggregate(FrameRateDecider* decider);
    ~ScopedAggregate();

   private:
    FrameRateDecider* const decider_;
  };

  FrameRateDecider(SurfaceManager* surface_manager, Client* client);
  ~FrameRateDecider() override;

  void SetSupportedFrameIntervals(
      std::vector<base::TimeDelta> supported_intervals);

  void set_min_num_of_frames_to_toggle_interval_for_testing(size_t num) {
    min_num_of_frames_to_toggle_interval_ = num;
  }

  // SurfaceObserver implementation.
  void OnSurfaceWillBeDrawn(Surface* surface) override;

 private:
  void StartAggregation();
  void EndAggregation();
  void UpdatePreferredFrameIntervalIfNeeded();
  bool multiple_refresh_rates_supported() const {
    return supported_intervals_.size() > 1u;
  }

  bool inside_surface_aggregation_ = false;
  base::flat_map<SurfaceId, uint64_t> current_surface_id_to_active_index_;

  base::flat_set<FrameSinkId> frame_sinks_updated_in_previous_frame_;
  base::flat_map<SurfaceId, uint64_t> prev_surface_id_to_active_index_;

  std::vector<base::TimeDelta> supported_intervals_;

  size_t num_of_frames_since_preferred_interval_changed_ = 0u;
  base::TimeDelta last_computed_preferred_frame_interval_;
  base::TimeDelta current_preferred_frame_interval_;

  size_t min_num_of_frames_to_toggle_interval_ = 60u;
  SurfaceManager* const surface_manager_;
  Client* const client_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_RATE_DECIDER_H_
