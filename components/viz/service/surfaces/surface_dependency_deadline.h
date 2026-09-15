// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/viz_service_export.h"

namespace base {
class TickClock;
}  // namespace base

namespace viz {

class FrameDeadline;

class VIZ_SERVICE_EXPORT SurfaceDependencyDeadline {
 public:
  explicit SurfaceDependencyDeadline(const base::TickClock* tick_clock);

  SurfaceDependencyDeadline(const SurfaceDependencyDeadline&) = delete;
  SurfaceDependencyDeadline& operator=(const SurfaceDependencyDeadline&) =
      delete;

  ~SurfaceDependencyDeadline();

  // Sets up the frame's global deadline in wall time where
  // deadline = frame_start_time + deadline_in_frames * frame_interval.
  void SetFrameDeadline(const FrameDeadline& frame_deadline);

  // Sets up per-dependency deadlines. The kPerDependencyDeadlines feature must
  // be enabled.
  void SetDependencyDeadlines(
      base::flat_map<SurfaceId, base::TimeTicks> dependency_deadlines);

  // Sets up the view transition deadline in wall time. Pass base::TimeTicks()
  // for the no deadline case.
  void SetViewTransitionDeadline(base::TimeTicks view_transition_deadline);

  // Called when an activation dependency is resolved.
  void OnActivationDependencyResolved(const SurfaceId& activation_dependency);

  // Returns whether the deadline has passed.
  // With UsePerDependencyDeadlines:
  //   effective deadline == max(pending_dependency_deadlines,
  //                             view_transition_deadline)
  // When UsePerDependencyDeadlines is disabled:
  //   effective deadline == global_deadline
  bool HasDeadlinePassed() const;

  // If a deadline had been set, then cancel the deadline and return the
  // the duration of the event tracked by this object. If there was no
  // deadline set, then return std::nullopt.
  std::optional<base::TimeDelta> Cancel();

  bool has_deadline() const { return deadline_.has_value(); }

  std::optional<base::TimeTicks> deadline_for_testing() const {
    return deadline_;
  }

  const base::flat_map<SurfaceId, base::TimeTicks>&
  dependency_deadlines_for_testing() const {
    return dependency_deadlines_;
  }

  base::TimeTicks view_transition_deadline_for_testing() const {
    return view_transition_deadline_;
  }

  bool operator==(const SurfaceDependencyDeadline& other) const;

 private:
  raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks start_time_;
  // TODO(crbug.com/540877772): remove the global deadline_ when
  // kPerDependencyDeadlines is launched.
  std::optional<base::TimeTicks> deadline_;
  base::flat_map<SurfaceId, base::TimeTicks> dependency_deadlines_;
  base::TimeTicks view_transition_deadline_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_DEPENDENCY_DEADLINE_H_
