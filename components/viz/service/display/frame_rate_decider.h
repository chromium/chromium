// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_RATE_DECIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_RATE_DECIDER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace viz {
class SurfaceManager;

enum class ToggleFrameRateCase : uint8_t {
  kNone = 0,
  kHardwareSupported = 1,
  kSingleVideoPerfectCadenceMatchesDisplay = 2,
  kSingleVideoPerfectCadenceDiffersFromDisplay = 3,
  kSingleVideoNoPerfectCadence = 4,
  kMultipleVideos = 5,
  kMaxValue = kMultipleVideos
};

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
        const FrameSinkId& id,
        mojom::CompositorFrameSinkType* type = nullptr) = 0;
  };

  // If provided in SetPreferredFrameInterval, this indicates that we don't have
  // any preferred setting and should let the platform decide the display's
  // refresh rate.
  static constexpr base::TimeDelta UnspecifiedFrameInterval() {
    return base::Seconds(0);
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
    const raw_ptr<FrameRateDecider> decider_;
  };

  // |hw_support_for_multiple_refresh_rates| indicates whether multiple refresh
  // rates are supported by the hardware or simulated by the BeginFrameSource.
  FrameRateDecider(SurfaceManager* surface_manager,
                   Client* client,
                   bool hw_support_for_multiple_refresh_rates,
                   bool output_surface_supports_set_frame_rate);
  ~FrameRateDecider() override;

  void SetSupportedFrameIntervals(
      base::flat_set<base::TimeDelta> supported_intervals);

  void set_min_num_of_frames_to_toggle_interval_for_testing(size_t num) {
    min_num_of_frames_to_toggle_interval_ = num;
  }

  // SurfaceObserver implementation.
  void OnSurfaceWillBeDrawn(Surface* surface) override;

  void SetHwSupportForMultipleRefreshRates(bool support);

 private:
  void StartAggregation();
  void EndAggregation();
  void UpdatePreferredFrameIntervalIfNeeded();
  void SetPreferredInterval(base::TimeDelta new_preferred_interval);
  ToggleFrameRateCase GetToggleFrameRateCase(
      const std::vector<base::TimeDelta>& fixed_interval_frame_sink_intervals)
      const;

  // If true, the refresh rate can be changed. It's either supported by HW
  // directly or by simulation in BeginFrameSource..
  bool multiple_refresh_rates_supported() const;

  bool inside_surface_aggregation_ = false;
  base::flat_map<SurfaceId, uint64_t> current_surface_id_to_active_index_;

  base::flat_set<FrameSinkId> frame_sinks_updated_in_previous_frame_;
  base::flat_set<FrameSinkId> frame_sinks_drawn_in_previous_frame_;
  base::flat_map<SurfaceId, uint64_t> prev_surface_id_to_active_index_;

  base::flat_set<base::TimeDelta> supported_intervals_;

  size_t num_of_frames_since_preferred_interval_changed_ = 0u;
  base::TimeDelta last_computed_preferred_frame_interval_;
  base::TimeDelta current_preferred_frame_interval_;

  size_t min_num_of_frames_to_toggle_interval_;

  base::MetricsSubSampler metrics_subsampler_;

  const raw_ptr<SurfaceManager> surface_manager_;
  const raw_ptr<Client> client_;

  // If true, allow to switch to the desired video frame rate without checking
  // whether it's single or multiple videos or whether the frame rate is
  // supported in the |supported_intervals_| list. There might not be a list at
  // all.
  bool hw_support_for_multiple_refresh_rates_;

  // For SetPreferredFrameInterval(), Display calls root_compositor_frame_sink
  // SetPreferredFrameInterval(). If |output_surface_supports_set_frame_rate_|
  // is true, Display also calls output_surface->SetFrameRate() and the new
  // frame rate is not limited to the list of |supported_intervals_|.
  const bool output_surface_supports_set_frame_rate_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_RATE_DECIDER_H_
