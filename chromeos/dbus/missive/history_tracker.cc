// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/history_tracker.h"

#include <atomic>

#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "components/reporting/proto/synced/health.pb.h"

namespace reporting {

HistoryTracker::HistoryTracker(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : sequenced_task_runner_(sequenced_task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

HistoryTracker::~HistoryTracker() = default;

// static
HistoryTracker* HistoryTracker::Get() {
  static HistoryTracker tracker{
      base::ThreadPool::CreateSequencedTaskRunner({})};
  return &tracker;
}

void HistoryTracker::AddObserver(HistoryTracker::Observer* observer) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](HistoryTracker::Observer* observer) {
            auto* const tracker = HistoryTracker::Get();
            DCHECK_CALLED_ON_VALID_SEQUENCE(tracker->sequence_checker_);
            tracker->observer_list_.AddObserver(observer);
          },
          observer));
}

void HistoryTracker::RemoveObserver(const HistoryTracker::Observer* observer) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const HistoryTracker::Observer* observer) {
            auto* const tracker = HistoryTracker::Get();
            DCHECK_CALLED_ON_VALID_SEQUENCE(tracker->sequence_checker_);
            tracker->observer_list_.RemoveObserver(observer);
          },
          observer));
}

bool HistoryTracker::debug_state() const {
  return debug_state_.load();
}

void HistoryTracker::set_debug_state(bool state) {
  const bool old_state = debug_state_.exchange(state);
  LOG_IF(WARNING, old_state != state)
      << "Debug state " << (state ? "enabled" : "disabled");
}

void HistoryTracker::retrieve_data(
    base::OnceCallback<void(const ERPHealthData&)> cb) {
  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce([]() {
        auto* const tracker = HistoryTracker::Get();
        DCHECK_CALLED_ON_VALID_SEQUENCE(tracker->sequence_checker_);
        return tracker->data_;
      }),
      std::move(cb));  // Call cb(tracker->data_) on the current thread.
}

void HistoryTracker::set_data(ERPHealthData data, base::OnceClosure cb) {
  sequenced_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](ERPHealthData data) {
            auto* const tracker = HistoryTracker::Get();
            DCHECK_CALLED_ON_VALID_SEQUENCE(tracker->sequence_checker_);
            tracker->data_ = std::move(data);
            for (const auto& observer : tracker->observer_list_) {
              observer.OnNewData(tracker->data_);
            }
          },
          std::move(data)),
      std::move(cb));  // Call cb() on the current thread.
}
}  // namespace reporting
