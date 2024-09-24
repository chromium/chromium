// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/viewport_intersection.h"

namespace performance_manager {

// static
ViewportIntersection ViewportIntersection::CreateNotIntersecting() {
  return ViewportIntersection(State::kNotIntersecting);
}

// static
ViewportIntersection ViewportIntersection::CreateIntersecting(
    bool is_intersecting_large_area) {
  return ViewportIntersection(is_intersecting_large_area
                                  ? State::kIntersectingLargeArea
                                  : State::kIntersectingNonLargeArea);
}

ViewportIntersection::ViewportIntersection(const ViewportIntersection&) =
    default;
ViewportIntersection::ViewportIntersection(ViewportIntersection&&) = default;
ViewportIntersection& ViewportIntersection::operator=(
    const ViewportIntersection&) = default;
ViewportIntersection& ViewportIntersection::operator=(ViewportIntersection&&) =
    default;

bool ViewportIntersection::is_intersecting() const {
  return state_ != State::kNotIntersecting;
}

bool ViewportIntersection::is_intersecting_large_area() const {
  return state_ == State::kIntersectingLargeArea;
}

ViewportIntersection::ViewportIntersection(State state) : state_(state) {}

}  // namespace performance_manager
