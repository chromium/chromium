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
    : sequenced_task_runner_(sequenced_task_runner) {}

HistoryTracker::~HistoryTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
HistoryTracker* HistoryTracker::Get() {
  static HistoryTracker tracker{
      base::ThreadPool::CreateSequencedTaskRunner({})};
  return &tracker;
}

void HistoryTracker::AddObserver(HistoryTracker::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void HistoryTracker::RemoveObserver(const HistoryTracker::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(cb).Run(data_);
}

void HistoryTracker::set_data(ERPHealthData data, base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_ = std::move(data);
  std::move(cb).Run();
  for (const auto& observer : observer_list_) {
    observer.OnNewData(data_);
  }
}
}  // namespace reporting
