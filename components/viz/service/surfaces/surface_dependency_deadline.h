// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_

#include "base/optional.h"
#include "base/time/time.h"

#include "base/macros.h"
#include "components/viz/service/viz_service_export.h"

namespace base {
class TickClock;
}  // namespace base

namespace viz {

class FrameDeadline;

class VIZ_SERVICE_EXPORT SurfaceDependencyDeadline {
 public:
  explicit SurfaceDependencyDeadline(const base::TickClock* tick_clock);
  ~SurfaceDependencyDeadline();

  // Sets up a deadline in wall time where
  // deadline = frame_start_time + deadline_in_frames * frame_interval.
  void Set(const FrameDeadline& frame_deadline);

  // Returns whether the deadline has passed.
  bool HasDeadlinePassed() const;

  // If a deadline had been set, then cancel the deadline and return the
  // the duration of the event tracked by this object. If there was no
  // deadline set, then return base::nullopt.
  base::Optional<base::TimeDelta> Cancel();

  bool has_deadline() const { return deadline_.has_value(); }

  base::Optional<base::TimeTicks> deadline_for_testing() const {
    return deadline_;
  }

  bool operator==(const SurfaceDependencyDeadline& other) const;
  bool operator!=(const SurfaceDependencyDeadline& other) const {
    return !(*this == other);
  }

 private:
  const base::TickClock* tick_clock_;
  base::TimeTicks start_time_;
  base::Optional<base::TimeTicks> deadline_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDependencyDeadline);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
