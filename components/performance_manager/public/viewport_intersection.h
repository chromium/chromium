// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_H_

namespace performance_manager {

// Mirrors `blink::mojom::FrameVisibility` with an explicit `kUnknown` initial
// state before the first visibility update is received. For outermost main
// frames, this is always `kIntersecting`. For backwards compatibility, if
// `features::kRenderedOutOfViewIsNotVisible` is disabled all rendered frames
// use `kIntersecting` even if they are outside the viewport.
enum class ViewportIntersection {
  kUnknown,
  // Intersection is undefined because the frame is not rendered (e.g.
  // `display: none` or `visibility: hidden`).
  kNotRendered,
  // The frame is rendered in layout, but scrolled completely outside the
  // viewport (0 px intersecting the viewport).
  kRenderedOutOfViewport,
  // The frame is rendered and at least partially intersects the viewport.
  kIntersecting,
};

// Categorizes the area of the frame's intersection with the outermost
// viewport relative to `blink::features::kLargeFrameSizePercentThreshold`.
// For outermost main frames, this is always `kLarge`.
enum class ViewportIntersectionArea {
  kUnknown,
  // Intersects <= threshold of the viewport (or not intersecting).
  kSmallOrNone,
  // Intersects > threshold of the viewport.
  kLarge,
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_H_
