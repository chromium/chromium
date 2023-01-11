// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/timed_event_recorder.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/metrics/cast_event_builder.h"
#include "chromecast/metrics/metrics_recorder.h"

namespace chromecast {

#define MAKE_SURE_SEQUENCE(task_runner, classname_with_method, ...)            \
  if (!task_runner->RunsTasksInCurrentSequence()) {                            \
    task_runner->PostTask(                                                     \
        FROM_HERE, base::BindOnce(&classname_with_method,                      \
                                  weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                    \
  }

TimedEventRecorder::TimedEventRecorder(MetricsRecorder* metrics_recorder)
    : metrics_recorder_(metrics_recorder),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(metrics_recorder_);
}

TimedEventRecorder::~TimedEventRecorder() = default;

void TimedEventRecorder::MeasureTimeUntilEvent(
    const std::string& event_name,
    const std::string& measurement_name,
    base::TimeTicks now) {
  MAKE_SURE_SEQUENCE(task_runner_, TimedEventRecorder::MeasureTimeUntilEvent,
                     event_name, measurement_name, now);
  event_name_to_measurements_[event_name].push_back({measurement_name, now});
}

void TimedEventRecorder::RecordEvent(const std::string& event_name,
                                     base::TimeTicks now) {
  MAKE_SURE_SEQUENCE(task_runner_, TimedEventRecorder::RecordEvent, event_name,
                     now);
  auto it_measurements = event_name_to_measurements_.find(event_name);
  if (it_measurements == event_name_to_measurements_.end()) {
    return;
  }
  for (const auto& measurement : it_measurements->second) {
    auto delta_ms = (now - measurement.start_time).InMilliseconds();
    DVLOG(1) << "Latency event: " << measurement.name << ": " << delta_ms
             << "ms";

    auto event_builder =
        metrics_recorder_->CreateEventBuilder(measurement.name);
    event_builder->SetExtraValue(delta_ms);
    metrics_recorder_->RecordCastEvent(std::move(event_builder));
  }
  // Clear all measurements for this end event.
  it_measurements->second.clear();
}

}  // namespace chromecast
