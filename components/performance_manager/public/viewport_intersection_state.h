// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_STATE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_STATE_H_

namespace performance_manager {

enum class ViewportIntersectionState {
  // Either the frame is not rendered (e.g. display: none) or is scrolled out
  // of view.
  kNotIntersecting,
  // Intersects with the viewport.
  kIntersecting,
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_STATE_H_
