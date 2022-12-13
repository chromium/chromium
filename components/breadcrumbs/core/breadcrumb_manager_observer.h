// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_OBSERVER_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"

namespace breadcrumbs {

class BreadcrumbManagerObserver : public base::CheckedObserver {
 public:
  BreadcrumbManagerObserver(const BreadcrumbManagerObserver&) = delete;
  BreadcrumbManagerObserver& operator=(const BreadcrumbManagerObserver&) =
      delete;

  // Called when a new `event` has been added to the BreadcrumbManager. Similar
  // to `BreadcrumbManager::GetEvents()`, `event` has the timestamp when it was
  // logged prepended to the string passed to `BreadcrumbManager::AddEvent()`.
  virtual void EventAdded(const std::string& event) {}

  // Called when the previous session's events have been retrieved from file and
  // added to the BreadcrumbManager.
  virtual void PreviousSessionEventsAdded() {}

 protected:
  BreadcrumbManagerObserver();
  ~BreadcrumbManagerObserver() override;

 private:
  // Tracks observed BreadcrumbManager and stops observing on destruction.
  base::ScopedObservation<BreadcrumbManager, BreadcrumbManagerObserver>
      breadcrumb_manager_observation_{this};
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_OBSERVER_H_
