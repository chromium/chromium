// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_INTERVAL_DECIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_INTERVAL_DECIDER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/viz/common/quads/frame_interval_inputs.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/frame_interval_matchers.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace viz {
class SurfaceManager;

// This class computes the ideal frame interval the display should use. It does
// this by using an ordered list of `FrameIntervalMatcher`. Matchers should
// generally be stateless to support updating the list at run time. This is the
// replacement for `FrameRateDecider`.
class VIZ_SERVICE_EXPORT FrameIntervalDecider {
 public:
  using FrameIntervalClass = FrameIntervalMatcher::FrameIntervalClass;
  using Result = FrameIntervalMatcher::Result;
  using ResultCallback = FrameIntervalMatcher::ResultCallback;
  using FixedIntervalSettings = FrameIntervalMatcher::FixedIntervalSettings;
  using ContinuousRangeSettings = FrameIntervalMatcher::ContinuousRangeSettings;
  using Settings = FrameIntervalMatcher::Settings;

  // This object should be created and held for the duration when surface
  // aggregation for a frame to be presented by the display is in progress. It
  // is used by the FrameIntervalDecider to keep track of drawn surfaces.
  // Constructed by calling `FrameIntervalDecider::WrapAggregate`.
  class VIZ_SERVICE_EXPORT ScopedAggregate : public SurfaceObserver {
   public:
    ~ScopedAggregate() override;

    ScopedAggregate(const ScopedAggregate&) = delete;
    ScopedAggregate& operator=(ScopedAggregate& other) = delete;

    // SurfaceObserver implementation.
    void OnSurfaceWillBeDrawn(Surface* surface) override;

   private:
    friend FrameIntervalDecider;

    ScopedAggregate(FrameIntervalDecider& decider,
                    SurfaceManager& surface_manager,
                    base::TimeTicks frame_time);

    const raw_ref<FrameIntervalDecider> decider_;
    const raw_ref<SurfaceManager> surface_manager_;
    const base::TimeTicks frame_time_;

    base::flat_map<FrameSinkId, FrameIntervalInputs> drawn_frame_sinks_;
  };

  FrameIntervalDecider();
  ~FrameIntervalDecider();

  const Settings& settings() const { return settings_; }

  void UpdateSettings(
      Settings settings,
      std::vector<std::unique_ptr<FrameIntervalMatcher>> matchers);
  std::unique_ptr<ScopedAggregate> WrapAggregate(
      SurfaceManager& surface_manager,
      base::TimeTicks frame_time);

 private:
  void Decide(base::TimeTicks frame_time,
              base::flat_map<FrameSinkId, FrameIntervalInputs> inputs_map);

  // It's not possible to compare `FrameIntervalClass` and frame interval
  // directly, so assume true if result changes type.
  static bool MayDecreaseFrameInterval(const std::optional<Result>& from,
                                       const std::optional<Result>& to);

  Settings settings_;
  std::vector<std::unique_ptr<FrameIntervalMatcher>> matchers_;

  base::TimeTicks current_result_frame_time_;
  std::optional<Result> current_result_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_FRAME_INTERVAL_DECIDER_H_
