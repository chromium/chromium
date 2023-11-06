// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/recorder.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/structured_metrics_features.h"

namespace metrics::structured {

Recorder::Recorder() = default;

Recorder::~Recorder() = default;

Recorder* Recorder::GetInstance() {
  static base::NoDestructor<Recorder> recorder;
  return recorder.get();
}

void Recorder::RecordEvent(Event&& event) {
  // All calls to StructuredMetricsProvider (the observer) must be on the UI
  // sequence, so re-call Record if needed. If a UI task runner hasn't been set
  // yet, ignore this Record.
  if (!ui_task_runner_) {
    LogInternalError(StructuredMetricsError::kUninitializedClient);
    return;
  }

  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Recorder::RecordEvent,
                                  base::Unretained(this), std::move(event)));
    return;
  }

  DCHECK(base::CurrentUIThread::IsSet());

  delegating_events_processor_.OnEventsRecord(&event);

  // Make a copy of an event that all observers can share.
  const auto event_clone = event.Clone();
  for (auto& observer : observers_) {
    observer.OnEventRecord(event_clone);
  }

  if (observers_.empty()) {
    // Other values of EventRecordingState are recorded in
    // StructuredMetricsProvider::OnRecord.
    LogEventRecordingState(EventRecordingState::kProviderMissing);
  }
}

void Recorder::ProfileAdded(const base::FilePath& profile_path) {
  // All calls to the StructuredMetricsProvider (the observer) must be on the UI
  // sequence.
  DCHECK(base::CurrentUIThread::IsSet());
  // TODO(crbug.com/1016655 ): investigate whether we can verify that
  // |profile_path| corresponds to a valid (non-guest, non-signin) profile.
  for (auto& observer : observers_) {
    observer.OnProfileAdded(profile_path);
  }

  // Notify the event processors.
  delegating_events_processor_.OnProfileAdded(profile_path);
}

void Recorder::OnReportingStateChanged(bool enabled) {
  for (auto& observer : observers_) {
    observer.OnReportingStateChanged(enabled);
  }
}

void Recorder::OnSystemProfileInitialized() {
  for (auto& observer : observers_) {
    observer.OnSystemProfileInitialized();
  }
}

void Recorder::SetUiTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
}

void Recorder::AddObserver(RecorderImpl* observer) {
  observers_.AddObserver(observer);
}

void Recorder::RemoveObserver(RecorderImpl* observer) {
  observers_.RemoveObserver(observer);
}

void Recorder::AddEventsProcessor(
    std::unique_ptr<EventsProcessorInterface> events_processor) {
  delegating_events_processor_.AddEventsProcessor(std::move(events_processor));
}

void Recorder::OnProvideIndependentMetrics(
    ChromeUserMetricsExtension* uma_proto) {
  delegating_events_processor_.OnProvideIndependentMetrics(uma_proto);
}

void Recorder::OnEventRecorded(StructuredEventProto* event) {
  delegating_events_processor_.OnEventRecorded(event);
}

}  // namespace metrics::structured
