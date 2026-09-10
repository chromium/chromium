// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_dependency_deadline.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/tick_clock.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/frame_deadline.h"

namespace viz {

SurfaceDependencyDeadline::SurfaceDependencyDeadline(
    const base::TickClock* tick_clock)
    : tick_clock_(tick_clock) {
  DCHECK(tick_clock_);
}

SurfaceDependencyDeadline::~SurfaceDependencyDeadline() {
  // The deadline must be canceled before destruction.
  DCHECK(!deadline_);
}

void SurfaceDependencyDeadline::SetFrameDeadline(
    const FrameDeadline& frame_deadline) {
  Cancel();
  start_time_ = frame_deadline.frame_start_time();
  deadline_ = frame_deadline.ToWallTime();
}

void SurfaceDependencyDeadline::SetViewTransitionDeadline(
    base::TimeTicks view_transition_deadline) {
  view_transition_deadline_ = view_transition_deadline;
}

bool SurfaceDependencyDeadline::HasDeadlinePassed() const {
  if (!features::UsePerDependencyDeadlines()) {
    return tick_clock_->NowTicks() >= deadline_;
  }

  if (!deadline_) {
    return true;
  }

  base::TimeTicks now = tick_clock_->NowTicks();
  if (!view_transition_deadline_.is_null() &&
      now < view_transition_deadline_) {
    return false;
  }

  return now >= deadline_;
}

std::optional<base::TimeDelta> SurfaceDependencyDeadline::Cancel() {
  if (!deadline_)
    return std::nullopt;

  deadline_.reset();
  view_transition_deadline_ = base::TimeTicks();

  return tick_clock_->NowTicks() - start_time_;
}

bool SurfaceDependencyDeadline::operator==(
    const SurfaceDependencyDeadline& other) const {
  return deadline_ == other.deadline_ &&
         view_transition_deadline_ == other.view_transition_deadline_;
}

}  // namespace viz
