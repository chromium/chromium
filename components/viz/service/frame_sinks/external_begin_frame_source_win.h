// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_WIN_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_WIN_H_

#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/frame_rate_decider.h"
#include "components/viz/service/viz_service_export.h"
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
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ExternalBeginFrameSourceWin(const ExternalBeginFrameSourceWin&) = delete;
  ExternalBeginFrameSourceWin& operator=(const ExternalBeginFrameSourceWin&) =
      delete;

  ~ExternalBeginFrameSourceWin() override;

  // ExternalBeginFrameSource implementation.
  BeginFrameArgs GetMissedBeginFrameArgs(BeginFrameObserver* obs) override;
  void SetPreferredInterval(base::TimeDelta interval) override;
  void SetVSyncDisplayID(int64_t display_id) override;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  // gl::VSyncThreadWin::VSyncObserver implementation.
  void OnVSync(base::TimeTicks vsync_time, base::TimeDelta interval) override;

 private:
  void OnVSyncOnSequence(base::TimeTicks vsync_time, base::TimeDelta interval);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  BeginFrameArgsGenerator begin_frame_args_generator_;

  bool observing_vsync_ = false;
  bool run_at_half_refresh_rate_ = false;
  bool skip_next_vsync_ = false;
  base::TimeDelta vsync_interval_ = BeginFrameArgs::DefaultInterval();

  base::WeakPtrFactory<ExternalBeginFrameSourceWin> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_WIN_H_
