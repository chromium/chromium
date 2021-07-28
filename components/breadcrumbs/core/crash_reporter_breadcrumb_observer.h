// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
#define COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_

#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"

class CrashReporterBreadcrumbObserverTest;

namespace breadcrumbs {

// Key for breadcrumbs attached to crash reports.
extern const char kBreadcrumbsProductDataKey[];

// Combines breadcrumbs from multiple BreadcrumbManagers and sends the merged
// breadcrumb events to the embedder's crash reporter (e.g., crashpad, breakpad)
// for attachment to crash reports.
class CrashReporterBreadcrumbObserver : public BreadcrumbManagerObserver {
 public:
  // Creates a singleton instance.
  static CrashReporterBreadcrumbObserver& GetInstance();

  // Sets breadcrumb events associated with the previous application session.
  void SetPreviousSessionEvents(const std::vector<std::string>& events);

  // Starts collecting breadcrumb events logged to |breadcrumb_manager|.
  void ObserveBreadcrumbManager(BreadcrumbManager* breadcrumb_manager);

  // Stops collecting breadcrumb events logged to |breadcrumb_manager|.
  void StopObservingBreadcrumbManager(BreadcrumbManager* breadcrumb_manager);

  // Starts collecting breadcrumb events logged to |breadcrumb_manager_service|.
  void ObserveBreadcrumbManagerService(
      BreadcrumbManagerKeyedService* breadcrumb_manager_service);

  // Stops collecting breadcrumb events logged to |breadcrumb_manager_service|.
  void StopObservingBreadcrumbManagerService(
      BreadcrumbManagerKeyedService* breadcrumb_manager_service);

 private:
  friend base::NoDestructor<CrashReporterBreadcrumbObserver>;
  friend class ::CrashReporterBreadcrumbObserverTest;

  CrashReporterBreadcrumbObserver();
  ~CrashReporterBreadcrumbObserver() override;

  // Updates the breadcrumbs stored in the crash log.
  void UpdateBreadcrumbEventsCrashKey();

  // BreadcrumbObserver:
  void EventAdded(BreadcrumbManager* manager,
                  const std::string& event) override;

  // A string which stores the received breadcrumbs. It may be truncated to the
  // maximum breadcrumbs string length (which aims to stay below the crash
  // reporter's maximum string length) when a new event is added, in order to
  // reduce overall memory usage.
  std::string breadcrumbs_;

  // Tracks observed BreadcrumbManagers and stops observing them on destruction.
  base::ScopedMultiSourceObservation<BreadcrumbManager,
                                     BreadcrumbManagerObserver>
      breadcrumb_manager_observations_{this};

  // Tracks observed BreadcrumbManagerKeyedServices and stops observing them on
  // destruction.
  base::ScopedMultiSourceObservation<BreadcrumbManagerKeyedService,
                                     BreadcrumbManagerObserver>
      breadcrumb_manager_service_observations_{this};
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_OBSERVER_H_
