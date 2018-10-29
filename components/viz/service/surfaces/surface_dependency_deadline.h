// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_

#include "components/viz/common/frame_sinks/begin_frame_source.h"

#include "components/viz/service/surfaces/surface_deadline_client.h"
#include "components/viz/service/viz_service_export.h"

namespace base {
class TickClock;
}  // namespace base

namespace viz {

class FrameDeadline;

class VIZ_SERVICE_EXPORT SurfaceDependencyDeadline : public BeginFrameObserver {
 public:
  SurfaceDependencyDeadline(SurfaceDeadlineClient* client,
                            BeginFrameSource* begin_frame_source,
                            const base::TickClock* tick_clock);
  ~SurfaceDependencyDeadline() override;

  // Sets up a deadline in wall time where
  // deadline = frame_start_time + deadline_in_frames * frame_interval.
  // It's possible for the deadline to already be in the past. In that case,
  // this method will return false. Otherwise, it will return true.
  bool Set(const FrameDeadline& frame_deadline);

  // If a deadline had been set, then cancel the deadline and return the
  // the duration of the event tracked by this object. If there was no
  // deadline set, then return base::nullopt.
  base::Optional<base::TimeDelta> Cancel();

  bool has_deadline() const { return deadline_.has_value(); }

  base::Optional<base::TimeTicks> deadline_for_testing() const {
    return deadline_;
  }

  // Takes on the same BeginFrameSource and deadline as |other|.
  void InheritFrom(const SurfaceDependencyDeadline& other);

  bool operator==(const SurfaceDependencyDeadline& other) const;
  bool operator!=(const SurfaceDependencyDeadline& other) const {
    return !(*this == other);
  }

  // BeginFrameObserver implementation.
  void OnBeginFrame(const BeginFrameArgs& args) override;
  const BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool WantsAnimateOnlyBeginFrames() const override;

 private:
  SurfaceDeadlineClient* const client_;
  BeginFrameSource* begin_frame_source_ = nullptr;
  const base::TickClock* tick_clock_;
  base::TimeTicks start_time_;
  base::Optional<base::TimeTicks> deadline_;

  BeginFrameArgs last_begin_frame_args_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDependencyDeadline);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
