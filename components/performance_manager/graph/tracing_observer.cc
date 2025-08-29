// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/tracing_observer.h"

#include "base/task/sequenced_task_runner.h"

namespace performance_manager {

TracingObserverList::TracingObserverList()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  notify_closure_ = base::BindRepeating(&TracingObserverList::NotifyObservers,
                                        weak_ptr_factory_.GetWeakPtr());
  base::TrackEvent::AddSessionObserver(this);
}

TracingObserverList::~TracingObserverList() {
  base::TrackEvent::RemoveSessionObserver(this);
}

void TracingObserverList::AddObserver(TracingObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void TracingObserverList::RemoveObserver(TracingObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void TracingObserverList::NotifyObservers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.Notify(&TracingObserver::OnTraceSessionStart);
}

void TracingObserverList::OnStart(const perfetto::DataSourceBase::StartArgs&) {
  task_runner_->PostTask(FROM_HERE, notify_closure_);
}

}  // namespace performance_manager
