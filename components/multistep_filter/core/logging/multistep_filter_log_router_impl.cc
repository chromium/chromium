// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_log_router_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"

namespace multistep_filter {

MultistepFilterLogRouterImpl::MultistepFilterLogRouterImpl() = default;

MultistepFilterLogRouterImpl::~MultistepFilterLogRouterImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::ListValue MultistepFilterLogRouterImpl::GetBufferedLogs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ListValue logs;
  for (const LogEntry& entry : buffer_) {
    logs.Append(entry.ToValue());
  }
  return logs;
}

void MultistepFilterLogRouterImpl::AddObserver(
    MultistepFilterLogRouter::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
  is_logging_enabled_.store(true, std::memory_order_relaxed);
}

void MultistepFilterLogRouterImpl::RemoveObserver(
    MultistepFilterLogRouter::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
  if (observers_.empty()) {
    is_logging_enabled_.store(false, std::memory_order_relaxed);
  }
}

bool MultistepFilterLogRouterImpl::IsLoggingEnabled() const {
  return is_logging_enabled_.load(std::memory_order_relaxed);
}

void MultistepFilterLogRouterImpl::RouteLogMessage(LogEntry entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsLoggingEnabled()) {
    return;
  }

  buffer_.push_back(std::move(entry));
  if (buffer_.size() > kMaxBufferSize) {
    buffer_.pop_front();
  }

  for (MultistepFilterLogRouter::Observer& observer : observers_) {
    observer.OnLogEntryAdded(buffer_.back());
  }
}

void MultistepFilterLogRouterImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  is_logging_enabled_.store(false, std::memory_order_relaxed);
  for (MultistepFilterLogRouter::Observer& observer : observers_) {
    observer.OnLogRouterShutdown();
  }
  buffer_.clear();
}

base::RepeatingCallback<void(LogEntry)>
MultistepFilterLogRouterImpl::GetLogCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&MultistepFilterLogRouterImpl::RouteLogMessage,
                          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace multistep_filter
