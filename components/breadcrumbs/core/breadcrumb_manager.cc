// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager.h"

#include "base/containers/adapters.h"
#include "base/containers/circular_deque.h"
#include "base/format_macros.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"

namespace breadcrumbs {

const size_t kMaxBreadcrumbs = 30;

// static
BreadcrumbManager& BreadcrumbManager::GetInstance() {
  static base::NoDestructor<BreadcrumbManager> breadcrumb_manager;
  return *breadcrumb_manager;
}

const base::circular_deque<std::string>& BreadcrumbManager::GetEvents() {
  return breadcrumbs_;
}

void BreadcrumbManager::AddEvent(const std::string& event) {
  DCHECK_EQ(std::string::npos, event.find('\n'));
  const base::TimeDelta elapsed_time = GetElapsedTime();

  // Prepend a timestamp containing elapsed time in H:MM:SS format. This is
  // preferred over base::TimeDurationWithSeconds() and wall-clock time to
  // avoid revealing client language or time zone.
  const int64_t total_seconds = elapsed_time.InSeconds();
  const int64_t hours = total_seconds / base::Time::kSecondsPerHour;
  const int64_t minutes = (total_seconds / base::Time::kSecondsPerMinute) %
                          base::Time::kMinutesPerHour;
  const int64_t seconds = total_seconds % base::Time::kSecondsPerMinute;
  const std::string event_with_timestamp =
      base::StringPrintf("%" PRIu64 ":%02" PRIu64 ":%02" PRIu64 " %s", hours,
                         minutes, seconds, event.c_str());

  if (breadcrumbs_.size() == kMaxBreadcrumbs)
    breadcrumbs_.pop_front();
  breadcrumbs_.push_back(event_with_timestamp);

  for (auto& observer : observers_) {
    observer.EventAdded(event_with_timestamp);
  }
}

void BreadcrumbManager::SetPreviousSessionEvents(
    const std::vector<std::string>& events) {
  // Insert `events` into the event log, skipping the oldest events (which are
  // at the front) if needed to keep the event log below `kMaxBreadcrumbs`.
  const size_t breadcrumbs_capacity = kMaxBreadcrumbs - breadcrumbs_.size();
  const size_t breadcrumbs_to_insert =
      std::min(events.size(), breadcrumbs_capacity);

  breadcrumbs_.insert(breadcrumbs_.begin(),
                      events.end() - breadcrumbs_to_insert, events.end());

  for (auto& observer : observers_) {
    observer.PreviousSessionEventsAdded();
  }
}

BreadcrumbManager::BreadcrumbManager() = default;
BreadcrumbManager::~BreadcrumbManager() = default;

base::TimeDelta BreadcrumbManager::GetElapsedTime() {
  return base::TimeTicks::Now() - start_time_;
}

void BreadcrumbManager::AddObserver(BreadcrumbManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BreadcrumbManager::RemoveObserver(BreadcrumbManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BreadcrumbManager::ResetForTesting() {
  start_time_ = base::TimeTicks::Now();
  breadcrumbs_.clear();
}

}  // namespace breadcrumbs
