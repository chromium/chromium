// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager.h"

#include "base/containers/adapters.h"
#include "base/format_macros.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"

namespace breadcrumbs {
namespace {

// The maximum number of breadcrumbs which are expected to be useful to store.
// NOTE: Events are "bucketed" into groups by short time intervals to make it
// more efficient to manage the continuous dropping of old events. Since events
// are only dropped at the bucket level, it is expected that the total number of
// stored breadcrumbs will exceed this value. This value should be close to the
// upper limit of useful events. (Most events + timestamp breadcrumbs are
// currently longer than 10 characters.)
constexpr unsigned long kMaxUsefulBreadcrumbEvents = kMaxDataLength / 10;

// The minimum number of event buckets to keep, even if they are expired.
const int kMinEventBuckets = 2;

// The time, in minutes, after which events are removed (unless there are fewer
// than |kMinEventBuckets| event buckets, in which case no time limit applies).
const int kEventExpirationMinutes = 20;

}  // namespace

BreadcrumbManager::BreadcrumbManager(base::TimeTicks start_time)
    : start_time_(start_time) {}

BreadcrumbManager::~BreadcrumbManager() = default;

const std::list<std::string> BreadcrumbManager::GetEvents() {
  DropOldEvents();

  std::list<std::string> events;
  for (const EventBucket& event_bucket : base::Reversed(event_buckets_)) {
    for (const std::string& event : base::Reversed(event_bucket.events)) {
      events.push_front(event);
    }
  }
  return events;
}

void BreadcrumbManager::AddEvent(const std::string& event) {
  DCHECK_EQ(std::string::npos, event.find('\n'));
  const base::TimeDelta elapsed_time = GetElapsedTime();

  // If a bucket exists, it will be at the end of the list.
  const int minutes_elapsed = elapsed_time.InMinutes();
  if (event_buckets_.empty() ||
      event_buckets_.back().minutes_elapsed != minutes_elapsed) {
    event_buckets_.emplace_back(minutes_elapsed);
  }

  // Prepend a timestamp containing elapsed time in H:MM:SS format. This is
  // preferred over base::TimeDurationWithSeconds() and wall-clock time to
  // avoid revealing client language or time zone.
  const int64_t total_seconds = elapsed_time.InSeconds();
  const int64_t hours = total_seconds / base::Time::kSecondsPerHour;
  const int64_t minutes = (total_seconds / base::Time::kSecondsPerMinute) %
                          base::Time::kMinutesPerHour;
  const int64_t seconds = total_seconds % base::Time::kSecondsPerMinute;
  const std::string event_log =
      base::StringPrintf("%" PRIu64 ":%02" PRIu64 ":%02" PRIu64 " %s", hours,
                         minutes, seconds, event.c_str());

  event_buckets_.back().events.push_back(event_log);

  for (auto& observer : observers_) {
    observer.EventAdded(this, event_log);
  }

  DropOldEvents();
}

void BreadcrumbManager::DropOldEvents() {
  bool old_buckets_dropped = false;
  // Drop buckets that are more than kEventExpirationMinutes old.
  while (event_buckets_.size() > kMinEventBuckets) {
    const int oldest_bucket_minutes = event_buckets_.front().minutes_elapsed;
    if (GetElapsedTime().InMinutes() - oldest_bucket_minutes <
        kEventExpirationMinutes) {
      break;
    }
    event_buckets_.pop_front();
    old_buckets_dropped = true;
  }

  // Drop buckets if the data is unlikely to ever be needed.
  unsigned long newer_event_count = 0;
  auto event_bucket_it = event_buckets_.rbegin();
  while (event_bucket_it != event_buckets_.rend()) {
    const std::list<std::string>& bucket_events = event_bucket_it->events;
    if (newer_event_count > kMaxUsefulBreadcrumbEvents) {
      event_buckets_.erase(event_buckets_.begin(), event_bucket_it.base());
      old_buckets_dropped = true;
      break;
    }
    newer_event_count += bucket_events.size();
    ++event_bucket_it;
  }

  if (old_buckets_dropped) {
    for (auto& observer : observers_) {
      observer.OldEventsRemoved(this);
    }
  }
}

base::TimeDelta BreadcrumbManager::GetElapsedTime() {
  return base::TimeTicks::Now() - start_time_;
}

void BreadcrumbManager::AddObserver(BreadcrumbManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BreadcrumbManager::RemoveObserver(BreadcrumbManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

BreadcrumbManager::EventBucket::EventBucket(int minutes_elapsed)
    : minutes_elapsed(minutes_elapsed) {}
BreadcrumbManager::EventBucket::EventBucket(const EventBucket&) = default;
BreadcrumbManager::EventBucket::~EventBucket() = default;

}  // namespace breadcrumbs
