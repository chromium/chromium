// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_H_

#include <compare>

namespace performance_manager {

class ViewportIntersection {
 public:
  static ViewportIntersection CreateNotIntersecting();
  static ViewportIntersection CreateIntersecting(
      bool is_intersecting_large_area);

  ViewportIntersection(const ViewportIntersection&);
  ViewportIntersection(ViewportIntersection&&);
  ViewportIntersection& operator=(const ViewportIntersection&);
  ViewportIntersection& operator=(ViewportIntersection&&);

  friend constexpr std::strong_ordering operator<=>(
      const ViewportIntersection&,
      const ViewportIntersection&) = default;

  // Returns true if the frame is intersecting with the viewport.
  bool is_intersecting() const;

  // Returns true if the frame is intersecting with a large area of the
  // viewport, or is presumed to be intersecting with a large area of the
  // viewport (because its parent is).
  bool is_intersecting_large_area() const;

 private:
  enum class State {
    // Either the frame is not rendered (e.g. display: none) or is scrolled out
    // of view.
    kNotIntersecting,
    // Intersects with the viewport.
    kIntersectingNonLargeArea,
    // Intersects with a large area of the viewport.
    kIntersectingLargeArea,
  };

  explicit ViewportIntersection(State state);

  State state_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_H_
