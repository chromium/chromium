// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_H_

namespace performance_manager {

enum class ViewportIntersection {
  kUnknown,
  kNotIntersecting,
  kIntersecting,
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_VIEWPORT_INTERSECTION_H_
