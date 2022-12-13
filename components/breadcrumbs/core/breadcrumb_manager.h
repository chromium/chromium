// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_H_

#include <string>

#include "base/containers/circular_deque.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/time/time.h"

namespace breadcrumbs {

class BreadcrumbManagerObserver;

// The maximum number of breadcrumbs to keep track of. Once this is reached,
// the oldest breadcrumbs will be dropped.
extern const size_t kMaxBreadcrumbs;

// Stores events logged with `AddEvent` in memory, which can later be retrieved
// with `GetEvents`.
class BreadcrumbManager {
 public:
  // Returns the singleton BreadcrumbManager. Creates it if it does not exist.
  static BreadcrumbManager& GetInstance();

  BreadcrumbManager(const BreadcrumbManager&) = delete;
  BreadcrumbManager& operator=(const BreadcrumbManager&) = delete;

  // Returns the current list of collected breadcrumb events. Events returned
  // will have a timestamp prepended, representing when each event was added.
  // Calling `AddEvents()` will invalidate iterators pointing to the list.
  const base::circular_deque<std::string>& GetEvents();

  // Logs a breadcrumb event with message data `event`. Drops the oldest stored
  // event if needed to keep the number of events below `kMaxBreadcrumbs`.
  // `event` must not include newline characters as newlines are used by
  // BreadcrumbPersistentStorageManager as a delimiter.
  void AddEvent(const std::string& event);

  // Adds breadcrumb events associated with the previous application session.
  // Note: this behaves the same as `AddEvent()`, but takes multiple events and
  // adds them to the start of the breadcrumbs log.
  void SetPreviousSessionEvents(const std::vector<std::string>& events);

  // Adds and removes observers.
  void AddObserver(BreadcrumbManagerObserver* observer);
  void RemoveObserver(BreadcrumbManagerObserver* observer);

  // Resets timestamps to 0:00:00 and removes all events. Does not remove
  // observers or notify observers about removed events.
  void ResetForTesting();

 private:
  friend class base::NoDestructor<BreadcrumbManager>;

  BreadcrumbManager();
  ~BreadcrumbManager();

  // Returns the time since `start_time_`.
  base::TimeDelta GetElapsedTime();

  // The time when breadcrumbs logging started, used to calculate elapsed time
  // for event timestamps.
  base::TimeTicks start_time_ = base::TimeTicks::Now();

  // List of event strings, up to a maximum of `kMaxBreadcrumbs`. Newer events
  // are at the end.
  base::circular_deque<std::string> breadcrumbs_;

  base::ObserverList<BreadcrumbManagerObserver, /*check_empty=*/true>
      observers_;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_H_
