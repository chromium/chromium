// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

// Interface to observe PerformanceManager events that happen on the main
// thread. All methods are invoked on the main thread.
class PerformanceManagerObserver : public base::CheckedObserver {
 public:
  ~PerformanceManagerObserver() override = default;

  // Invoked when a PageNode is created for |web_contents|. The PageNode can be
  // retrieved via PerformanceManager::GetPageNodeForWebContents(). The PageNode
  // will be destroyed when |web_contents| is destroyed or when the
  // PerformanceManagerRegistry is destroyed, whichever comes first.
  virtual void OnPageNodeCreatedForWebContents(
      content::WebContents* web_contents) {}

  // Invoked before the PM is torn down on the main thread.
  virtual void OnBeforePerformanceManagerDestroyed() {}

 protected:
  PerformanceManagerObserver() = default;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_OBSERVER_H_
