// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
#define COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_

#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/no_destructor.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"

namespace breadcrumbs {

// Key for breadcrumbs attached to crash reports.
inline constexpr char kBreadcrumbsProductDataKey[] = "breadcrumbs";

// Concatenates breadcrumbs from the BreadcrumbManager and sends the merged
// string to the embedder's crash reporter (e.g., Crashpad, Breakpad) for
// attachment to crash reports.
class CrashReporterBreadcrumbObserver : public BreadcrumbManagerObserver {
 public:
  // Creates a singleton instance that observes the BreadcrumbManager.
  static CrashReporterBreadcrumbObserver& GetInstance();

 private:
  friend base::NoDestructor<CrashReporterBreadcrumbObserver>;

  CrashReporterBreadcrumbObserver();
  ~CrashReporterBreadcrumbObserver() override;

  // BreadcrumbObserver:
  void EventAdded(const std::string& event) override;
  void PreviousSessionEventsAdded() override;

  // Updates the breadcrumbs stored in the crash log.
  void UpdateBreadcrumbEventsCrashKey();
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
