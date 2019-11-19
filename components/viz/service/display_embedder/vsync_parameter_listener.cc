// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/vsync_parameter_listener.h"

#include <utility>

namespace viz {

constexpr base::TimeDelta VSyncParameterListener::kMaxTimebaseSkew;

VSyncParameterListener::VSyncParameterListener(
    mojo::PendingRemote<mojom::VSyncParameterObserver> observer)
    : observer_(std::move(observer)) {}

VSyncParameterListener::~VSyncParameterListener() = default;

void VSyncParameterListener::OnVSyncParametersUpdated(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (ShouldSendUpdate(timebase, interval))
    observer_->OnUpdateVSyncParameters(timebase, interval);
}

bool VSyncParameterListener::ShouldSendUpdate(base::TimeTicks timebase,
                                              base::TimeDelta interval) {
  // Ignore updates with interval 0.
  if (interval.is_zero())
    return false;

  // Calculate an offset for the current timebase compared to the interval.
  base::TimeDelta offset = timebase.since_origin() % interval;

  // Always send updates if the interval changed.
  if (last_interval_ == interval) {
    base::TimeDelta offset_delta = (offset - last_offset_).magnitude();

    // Take into account modulus wrap around near multiples of interval. For
    // example, if interval=100μs then timebase=1099μs produces offset=99μs
    // while timebase=1101μs produces offset=1μs. The difference in offsets
    // should be 2μs instead of 98μs.
    if (offset_delta > interval / 2)
      offset_delta = interval - offset_delta;

    // The interval is the same and skew is small, so don't send an update.
    if (offset_delta < kMaxTimebaseSkew)
      return false;
  }

  last_offset_ = offset;
  last_interval_ = interval;

  return true;
}

}  // namespace viz
