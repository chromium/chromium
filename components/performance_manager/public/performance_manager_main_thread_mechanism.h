// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_MAIN_THREAD_MECHANISM_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_MAIN_THREAD_MECHANISM_H_

#include "base/observer_list_types.h"

#include <memory>
#include <vector>

namespace content {
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

namespace performance_manager {

// Interface for implementing PerformanceManager mechanism hooks that occur on
// the main thread. All methods are invoked on the main thread.
class PerformanceManagerMainThreadMechanism : public base::CheckedObserver {
 public:
  using Throttles = std::vector<std::unique_ptr<content::NavigationThrottle>>;

  ~PerformanceManagerMainThreadMechanism() override = default;

  PerformanceManagerMainThreadMechanism(
      const PerformanceManagerMainThreadMechanism&) = delete;
  PerformanceManagerMainThreadMechanism& operator=(
      const PerformanceManagerMainThreadMechanism&) = delete;

  // Invoked when a NavigationHandle is committed, providing an opportunity for
  // the mechanism to apply throttles.
  virtual Throttles CreateThrottlesForNavigation(
      content::NavigationHandle* handle) = 0;

 protected:
  PerformanceManagerMainThreadMechanism() = default;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_MAIN_THREAD_MECHANISM_H_
