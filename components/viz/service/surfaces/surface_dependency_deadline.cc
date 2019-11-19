// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_dependency_deadline.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/tick_clock.h"
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

void SurfaceDependencyDeadline::Set(const FrameDeadline& frame_deadline) {
  Cancel();
  start_time_ = frame_deadline.frame_start_time();
  deadline_ = frame_deadline.ToWallTime();
}

bool SurfaceDependencyDeadline::HasDeadlinePassed() const {
  return tick_clock_->NowTicks() >= deadline_;
}

base::Optional<base::TimeDelta> SurfaceDependencyDeadline::Cancel() {
  if (!deadline_)
    return base::nullopt;

  deadline_.reset();

  base::TimeDelta duration = tick_clock_->NowTicks() - start_time_;

  UMA_HISTOGRAM_TIMES("Compositing.SurfaceDependencyDeadline.Duration",
                      duration);

  return duration;
}

bool SurfaceDependencyDeadline::operator==(
    const SurfaceDependencyDeadline& other) const {
  return deadline_ == other.deadline_;
}

}  // namespace viz
