// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_FLING_SCHEDULER_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_FLING_SCHEDULER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/input/fling_controller.h"
#include "components/input/fling_scheduler_base.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h"
#include "components/viz/service/viz_service_export.h"

namespace input {
class FlingController;
class RenderInputRouter;
}  // namespace input

namespace viz {

class VIZ_SERVICE_EXPORT FlingSchedulerAndroid
    : public input::FlingSchedulerBase,
      public BeginFrameObserverBase {
 public:
  FlingSchedulerAndroid(input::RenderInputRouter* rir,
                        const FrameSinkId& frame_sink_id);

  FlingSchedulerAndroid(const FlingSchedulerAndroid&) = delete;
  FlingSchedulerAndroid& operator=(const FlingSchedulerAndroid&) = delete;

  ~FlingSchedulerAndroid() override;

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<input::FlingController> fling_controller) override;
  void DidStopFlingingOnBrowser(
      base::WeakPtr<input::FlingController> fling_controller) override;
  bool ProgressFlingOnFlingStart() override;
  bool ShouldUseMobileFlingCurve() override;
  gfx::Vector2dF GetPixelsPerInch(
      const gfx::PointF& position_in_screen) override;

  // FlingSchedulerBase
  void ProgressFlingOnBeginFrameIfneeded(base::TimeTicks current_time) override;
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override;

 protected:
  BeginFrameSource* GetBeginFrameSource();

  raw_ref<input::RenderInputRouter> rir_;
  base::WeakPtr<input::FlingController> fling_controller_;
  bool observing_begin_frame_source_ = false;
  raw_ptr<BeginFrameSource> begin_frame_source_ = nullptr;

 private:
  FRIEND_TEST_ALL_PREFIXES(FlingSchedulerTest, ScheduleNextFlingProgress);
  FRIEND_TEST_ALL_PREFIXES(FlingSchedulerTest, FlingCancelled);
  FRIEND_TEST_ALL_PREFIXES(FlingSchedulerTest,
                           ResetStateOnBeginFrameSourceChange);

  void StartObservingBeginFrames();
  void StopObservingBeginFrames();

  // BeginFrameObserverBase implementation.
  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override;
  void OnBeginFrameSourcePausedChanged(bool paused) override {}

  const FrameSinkId frame_sink_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_FLING_SCHEDULER_ANDROID_H_
