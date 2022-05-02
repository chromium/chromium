// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/recorder.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/structured_metrics_features.h"

namespace metrics {
namespace structured {

Recorder::Recorder() = default;
Recorder::~Recorder() = default;

Recorder* Recorder::GetInstance() {
  static base::NoDestructor<Recorder> recorder;
  return recorder.get();
}

void Recorder::RecordEvent(Event&& event) {
  auto event_base = EventBase::FromEvent(std::move(event));
  if (event_base.has_value())
    Record(std::move(event_base.value()));
}

void Recorder::Record(EventBase&& event) {
  // All calls to StructuredMetricsProvider (the observer) must be on the UI
  // sequence, so re-call Record if needed. If a UI task runner hasn't been set
  // yet, ignore this Record.
  if (!ui_task_runner_) {
    LogInternalError(StructuredMetricsError::kUninitializedClient);
    return;
  }

  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Recorder::Record, base::Unretained(this), event));
    return;
  }

  // If the feature is disabled, it means that the event was recorded directly
  // and not through the mojo API.
  if (!base::FeatureList::IsEnabled(kUseCrosApiInterface)) {
    LogIsEventRecordedUsingMojo(false);
  }

  DCHECK(base::CurrentUIThread::IsSet());
  for (auto& observer : observers_)
    observer.OnRecord(event);

  if (observers_.empty()) {
    // Other values of EventRecordingState are recorded in
    // StructuredMetricsProvider::OnRecord.
    LogEventRecordingState(EventRecordingState::kProviderMissing);
  }
}

bool Recorder::IsReadyToRecord() const {
  // No initialization needed. Always ready to record.
  return true;
}

void Recorder::ProfileAdded(const base::FilePath& profile_path) {
  // All calls to the StructuredMetricsProvider (the observer) must be on the UI
  // sequence.
  DCHECK(base::CurrentUIThread::IsSet());
  // TODO(crbug.com/1016655 ): investigate whether we can verify that
  // |profile_path| corresponds to a valid (non-guest, non-signin) profile.
  for (auto& observer : observers_)
    observer.OnProfileAdded(profile_path);
}

absl::optional<int> Recorder::LastKeyRotation(uint64_t project_name_hash) {
  absl::optional<int> result;
  // |observers_| will contain at most one observer, despite being an
  // ObserverList.
  for (auto& observer : observers_) {
    result = observer.LastKeyRotation(project_name_hash);
  }
  return result;
}

void Recorder::OnReportingStateChanged(bool enabled) {
  for (auto& observer : observers_) {
    observer.OnReportingStateChanged(enabled);
  }
}

void Recorder::OnHardwareClassInitialized() {
  for (auto& observer : observers_) {
    observer.OnHardwareClassInitialized();
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

}  // namespace structured
}  // namespace metrics
