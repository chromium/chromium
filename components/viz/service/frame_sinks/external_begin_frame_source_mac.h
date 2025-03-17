// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MAC_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MAC_H_

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/display/mac/display_link_mac.h"
#include "ui/display/types/display_constants.h"

namespace viz {
class OutputSurface;

// An external begin frame source for use on macOS. This listens to a
// DisplayLinkMac in order to tick.
class VIZ_COMMON_EXPORT ExternalBeginFrameSourceMac
    : public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient,
      public DelayBasedTimeSourceClient {
 public:
  using MultipleHWRefreshRatesCallback = base::RepeatingCallback<void(bool)>;

  ExternalBeginFrameSourceMac(uint32_t restart_id,
                              int64_t display_id,
                              OutputSurface* output_surface);

  ExternalBeginFrameSourceMac(const ExternalBeginFrameSourceMac&) = delete;
  ExternalBeginFrameSourceMac& operator=(const ExternalBeginFrameSourceMac&) =
      delete;
  ~ExternalBeginFrameSourceMac() override;

  // BeginFrameSource implementation.
  void SetVSyncDisplayID(int64_t display_id) override;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  // DelayBasedTimeSourceClient implementation.
  void OnTimerTick() override;

  // BeginFrameSource implementation.
  BeginFrameArgs GetMissedBeginFrameArgs(BeginFrameObserver* obs) override;
  void SetPreferredInterval(base::TimeDelta interval) override;
  base::TimeDelta GetMinimumFrameInterval() override;
  base::flat_set<base::TimeDelta> GetSupportedFrameIntervals(
      base::TimeDelta interval) override;

  // CVDisplayLink Callback on the Viz thread.
  void OnDisplayLinkCallback(ui::VSyncParamsMac params);

  // BeginFrameSource implementation.
  // Callback to RootCompositorFrameSinkImpl::SetDisplayVSyncParameters.
  // When the frame rate changes, VSyncParameters should be updated.
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;

  void SetMultipleHWRefreshRatesCallback(
      MultipleHWRefreshRatesCallback callback);

 private:
  void CreateDelayBasedTimeSourceIfNeeded();

  void StartBeginFrame();
  void StopBeginFrame();

  BeginFrameArgsGenerator begin_frame_args_generator_;

  bool needs_begin_frames_ = false;

  // Used for preferred frame intervals.
  int vsync_subsampling_factor_ = 1;
  int vsyncs_to_skip_ = 0;

  // CVDisplayLink and related structures to set timer parameters.
  int64_t display_id_ = display::kInvalidDisplayId;
  scoped_refptr<ui::DisplayLinkMac> display_link_mac_;

  // CVDisplayLink callback.
  std::unique_ptr<ui::VSyncCallbackMac> vsync_callback_mac_;

  // The default interval is 60Hz.
  base::TimeDelta nominal_refresh_period_ = BeginFrameArgs::DefaultInterval();

  base::TimeDelta preferred_interval_ = BeginFrameArgs::DefaultInterval();

  // Timer used to drive callbacks.
  // TODO(crbug.com/40062488): Only use this when it is not possible or
  // efficient to use `display_link_`.
  std::unique_ptr<DelayBasedTimeSource> time_source_;
  base::TimeTicks last_frame_time_;
  base::TimeDelta last_interval_;

  bool just_started_begin_frame_ = false;

  const raw_ptr<OutputSurface, DanglingUntriaged> output_surface_;
  UpdateVSyncParametersCallback update_vsync_params_callback_;

  MultipleHWRefreshRatesCallback multiple_hw_refresh_rates_callback_;

  // When true, CADisplayLink can take any preferred refresh rate set by
  // FrameRateDecider as is. No supported_interval list is provided to
  // FrameRateDecider. No preferred refresh rate is adjusted.
  bool hw_takes_any_refresh_rate_ = false;

  // Screen refresh interval caps.
  base::TimeDelta min_refresh_interval_;
  base::TimeDelta max_refresh_interval_;
  base::TimeDelta granularity_;

  base::WeakPtrFactory<ExternalBeginFrameSourceMac> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_MAC_H_
