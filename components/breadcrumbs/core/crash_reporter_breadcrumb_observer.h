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
extern const char kBreadcrumbsProductDataKey[];

// Concatenates breadcrumbs from the BreadcrumbManager and sends the merged
// string to the embedder's crash reporter (e.g., Crashpad, Breakpad) for
// attachment to crash reports.
class CrashReporterBreadcrumbObserver : public BreadcrumbManagerObserver {
 public:
  // Creates a singleton instance that observes the BreadcrumbManager.
  static CrashReporterBreadcrumbObserver& GetInstance();

  // Sets breadcrumb events associated with the previous application session.
  // Note: this behaves the same as EventAdded(), but takes multiple events and
  // adds them to the start of the breadcrumbs log.
  void SetPreviousSessionEvents(const std::vector<std::string>& events);

  // Removes all events.
  void ResetForTesting();

 private:
  friend base::NoDestructor<CrashReporterBreadcrumbObserver>;

  CrashReporterBreadcrumbObserver();
  ~CrashReporterBreadcrumbObserver() override;

  // BreadcrumbObserver:
  void EventAdded(const std::string& event) override;

  // Updates the breadcrumbs stored in the crash log.
  void UpdateBreadcrumbEventsCrashKey();

  // The full list of received breadcrumbs that will be sent to the crash
  // report. Older events are at the front. A maximum size is enforced for
  // privacy purposes, so old events may be removed when new events are added.
  base::circular_deque<std::string> breadcrumbs_;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
