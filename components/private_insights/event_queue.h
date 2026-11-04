// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_EVENT_QUEUE_H_
#define COMPONENTS_PRIVATE_INSIGHTS_EVENT_QUEUE_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"

namespace private_insights {

// A bounded FIFO of timestamped events awaiting upload for a single
// population. Each population owns one queue for its own `EventProto`, so the
// upload machinery can treat populations uniformly while each population keeps
// its own events, capacity and health histograms.
//
// The queue owns no threads and is bound to the sequence it is constructed on.
// Events are detached with `TakeAll()`, handed to the upload pipeline, and
// given back with `Requeue()` if the upload failed. A `WeakPtr` to the queue
// can be bound into the requeue callback so a late reply after the owner is
// gone is a no-op.
//
// `histogram_prefix` names the population's health histograms, e.g.
// "PrivateMetrics.PrivateInsights.ContextualCueEvents". The queue records:
//   <prefix>.Logging.QueuedCount       size after every Push()
//   <prefix>.Logging.RemovedCount      events evicted by a Push() (if any)
//   <prefix>.Requeuing.RequeuedCount   events put back by a Requeue()
//   <prefix>.Requeuing.DroppedCount    events that didn't fit on Requeue()
template <typename EventProto>
class EventQueue {
 public:
  struct Entry {
    base::Time timestamp;
    EventProto event;
  };
  using Entries = base::circular_deque<Entry>;

  explicit EventQueue(std::string_view histogram_prefix)
      : queued_count_histogram_(
            base::StrCat({histogram_prefix, ".Logging.QueuedCount"})),
        removed_count_histogram_(
            base::StrCat({histogram_prefix, ".Logging.RemovedCount"})),
        requeued_count_histogram_(
            base::StrCat({histogram_prefix, ".Requeuing.RequeuedCount"})),
        dropped_count_histogram_(
            base::StrCat({histogram_prefix, ".Requeuing.DroppedCount"})) {}

  EventQueue(const EventQueue&) = delete;
  EventQueue& operator=(const EventQueue&) = delete;

  ~EventQueue() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  // Appends `event`, stamped with the current time, then evicts from the
  // front until the queue holds at most `max_events` entries. A non-positive
  // `max_events` (e.g. a misconfigured Finch param) keeps nothing.
  void Push(EventProto event, int max_events) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    entries_.emplace_back(base::Time::Now(), std::move(event));
    // saturated_cast clamps negative values to 0.
    const size_t capacity = base::saturated_cast<size_t>(max_events);
    int removed_count = 0;
    while (entries_.size() > capacity) {
      entries_.pop_front();
      removed_count++;
    }
    if (removed_count > 0) {
      base::UmaHistogramCounts100(removed_count_histogram_, removed_count);
    }
    base::UmaHistogramCounts100(queued_count_histogram_,
                                static_cast<int>(entries_.size()));
  }

  // Detaches and returns every entry, leaving the queue empty and ready for
  // new events. (Swap rather than move: a moved-from `circular_deque` is only
  // "valid but unspecified", whereas after a swap the queue is guaranteed
  // empty.)
  Entries TakeAll() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Entries detached;
    detached.swap(entries_);
    return detached;
  }

  // Prepends `events` (preserving their original order) without letting the
  // queue grow beyond `max_events`. Events that don't fit are dropped, oldest
  // first. Records nothing if `events` is empty. A non-positive `max_events`
  // drops everything.
  void Requeue(Entries events, int max_events) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (events.empty()) {
      return;
    }

    // saturated_cast clamps negative values to 0.
    const size_t capacity = base::saturated_cast<size_t>(max_events);
    int requeued_count = 0;
    while (!events.empty() && entries_.size() < capacity) {
      entries_.push_front(std::move(events.back()));
      events.pop_back();
      requeued_count++;
    }
    int dropped_count = static_cast<int>(events.size());

    base::UmaHistogramCounts100(requeued_count_histogram_, requeued_count);
    base::UmaHistogramCounts100(dropped_count_histogram_, dropped_count);
  }

  size_t size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return entries_.size();
  }

  bool empty() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return entries_.empty();
  }

  // Read-only view of the queued entries, oldest first.
  const Entries& entries() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return entries_;
  }

  base::WeakPtr<EventQueue> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const std::string queued_count_histogram_;
  const std::string removed_count_histogram_;
  const std::string requeued_count_histogram_;
  const std::string dropped_count_histogram_;

  Entries entries_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<EventQueue> weak_ptr_factory_{this};
};

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_EVENT_QUEUE_H_
