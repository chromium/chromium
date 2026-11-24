// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_WIN_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_WIN_H_

#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gl/vsync_thread_win.h"

namespace viz {

// Receives begin frames from VSyncThreadWin.
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceWin
    : public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient,
      public gl::VSyncThreadWin::VSyncObserver {
 public:
  ExternalBeginFrameSourceWin(
      uint32_t restart_id,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      gfx::AcceleratedWidget window = gfx::kNullAcceleratedWidget);

  ExternalBeginFrameSourceWin(const ExternalBeginFrameSourceWin&) = delete;
  ExternalBeginFrameSourceWin& operator=(const ExternalBeginFrameSourceWin&) =
      delete;

  ~ExternalBeginFrameSourceWin() override;

  // ExternalBeginFrameSource implementation.
  BeginFrameArgs GetMissedBeginFrameArgs(BeginFrameObserver* obs) override;
  void SetPreferredInterval(base::TimeDelta interval) override;
  void SetVSyncDisplayID(int64_t display_id, bool force_update) override;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  // gl::VSyncThreadWin::VSyncObserver implementation.
  void OnVSync(base::TimeTicks vsync_time, base::TimeDelta interval) override;

 protected:
  // Queries the hardware VSync interval of the monitor containing |window_|.
  // Virtual for testing.
  virtual base::TimeDelta QueryHardwareVSyncInterval() const;

 private:
  void OnVSyncOnSequence(base::TimeTicks vsync_time,
                         base::TimeDelta vsync_thread_interval);
  void UpdateHardwareVSyncInterval();
  base::TimeDelta AdjustVSyncInterval(
      base::TimeDelta vsync_thread_interval) const;
  void CheckRunAtHalfRefreshRate(base::TimeDelta vsync_interval);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const gfx::AcceleratedWidget window_;

  BeginFrameArgsGenerator begin_frame_args_generator_;

  bool observing_vsync_ = false;
  bool run_at_half_refresh_rate_ = false;
  // The VSync interval from the single global VSyncThreadWin (primary or
  // fastest display).
  base::TimeDelta vsync_thread_interval_ = BeginFrameArgs::DefaultInterval();
  // The hardware VSync interval of the monitor containing |window_|, which may
  // differ from |vsync_thread_interval_| on mixed-refresh-rate multi-monitor
  // setups.
  base::TimeDelta hardware_vsync_interval_;
  base::TimeDelta preferred_interval_;
  base::TimeTicks last_expected_vblank_time_;

  base::WeakPtrFactory<ExternalBeginFrameSourceWin> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_WIN_H_
