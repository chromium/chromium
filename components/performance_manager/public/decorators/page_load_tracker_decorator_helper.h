// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_HELPER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/performance_manager_main_thread_observer.h"

namespace performance_manager {

// This class must be instantiated on the UI thread in order to maintain the
// PageLoadTracker decorator of PageNodes.
class PageLoadTrackerDecoratorHelper
    : public PerformanceManagerMainThreadObserverDefaultImpl {
 public:
  PageLoadTrackerDecoratorHelper();
  ~PageLoadTrackerDecoratorHelper() override;
  PageLoadTrackerDecoratorHelper(const PageLoadTrackerDecoratorHelper& other) =
      delete;
  PageLoadTrackerDecoratorHelper& operator=(
      const PageLoadTrackerDecoratorHelper&) = delete;

  // PerformanceManagerMainThreadObserver:
  void OnPageNodeCreatedForWebContents(
      content::WebContents* web_contents) override;

 private:
  class WebContentsObserver;

  // Linked list of WebContentsObservers created by this
  // PageLoadTrackerDecoratorHelper. Each WebContentsObservers removes itself
  // from the list and destroys itself when its associated WebContents is
  // destroyed. Additionally, all WebContentsObservers that are still in this
  // list when the destructor of PageLoadTrackerDecoratorHelper is invoked are
  // destroyed.
  raw_ptr<WebContentsObserver> first_web_contents_observer_ = nullptr;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_HELPER_H_
