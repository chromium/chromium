// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/recorder.h"

#include <utility>

#include "base/bind.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/histogram_util.h"

namespace metrics {
namespace structured {

Recorder::Recorder() = default;
Recorder::~Recorder() = default;

Recorder* Recorder::GetInstance() {
  static base::NoDestructor<Recorder> recorder;
  return recorder.get();
}

void Recorder::Record(const EventBase& event) {
  // All calls to StructuredMetricsProvider (the observer) must be on the UI
  // sequence, so re-call Record if needed. If a UI task runner hasn't been set
  // yet, ignore this Record.
  if (!ui_task_runner_)
    return;

  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Recorder::Record, base::Unretained(this), event));
    return;
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

void Recorder::ProfileAdded(const base::FilePath& profile_path) {
  // All calls to the StructuredMetricsProvider (the observer) must be on the UI
  // sequence.
  DCHECK(base::CurrentUIThread::IsSet());
  // TODO(crbug.com/1016655 ): investigate whether we can verify that
  // |profile_path| corresponds to a valid (non-guest, non-signin) profile.
  for (auto& observer : observers_)
    observer.OnProfileAdded(profile_path);
}

void Recorder::SetUiTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
}

void Recorder::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Recorder::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace structured
}  // namespace metrics
