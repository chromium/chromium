// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_OBSERVER_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"

namespace breadcrumbs {

class BreadcrumbManager;

class BreadcrumbManagerObserver : public base::CheckedObserver {
 public:
  BreadcrumbManagerObserver(const BreadcrumbManagerObserver&) = delete;
  BreadcrumbManagerObserver& operator=(const BreadcrumbManagerObserver&) =
      delete;

  // Called when a new |event| has been added to |manager|. Similar to
  // |BreadcrumbManager::GetEvents|, |event| will have the timestamp at which it
  // was logged prepended to the string which was passed to
  // |BreadcrumbManager::AddEvent|.
  virtual void EventAdded(BreadcrumbManager* manager,
                          const std::string& event) {}

  // Called when old events have been removed.
  virtual void OldEventsRemoved(BreadcrumbManager* manager) {}

 protected:
  BreadcrumbManagerObserver() = default;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_OBSERVER_H_
