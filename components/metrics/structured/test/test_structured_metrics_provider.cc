// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/test/test_structured_metrics_provider.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/metrics/structured/test/test_event_storage.h"
#include "components/metrics/structured/test/test_key_data_provider.h"

namespace metrics::structured {

TestStructuredMetricsProvider::TestStructuredMetricsProvider() {
  if (temp_dir_.CreateUniqueTempDir()) {
    structured_metrics_recorder_ =
        base::MakeRefCounted<StructuredMetricsRecorder>(
            std::make_unique<TestKeyDataProvider>(
                temp_dir_.GetPath()
                    .Append(FILE_PATH_LITERAL("structured_metrics"))
                    .Append(FILE_PATH_LITERAL("device_keys"))),
            std::make_unique<TestEventStorage>());
    Recorder::GetInstance()->SetRecorder(this);
  }
}

TestStructuredMetricsProvider::TestStructuredMetricsProvider(
    scoped_refptr<StructuredMetricsRecorder> recorder)
    : structured_metrics_recorder_(std::move(recorder)) {
  Recorder::GetInstance()->SetRecorder(this);
}

TestStructuredMetricsProvider::~TestStructuredMetricsProvider() {
  Recorder::GetInstance()->UnsetRecorder(this);
}

const EventsProto& TestStructuredMetricsProvider::ReadEvents() const {
  return *static_cast<const TestEventStorage*>(
              structured_metrics_recorder_->event_storage())
              ->events();
}

std::optional<const StructuredEventProto*>
TestStructuredMetricsProvider::FindEvent(uint64_t project_name_hash,
                                         uint64_t event_name_hash) {
  if (!structured_metrics_recorder_->CanProvideMetrics()) {
    return std::nullopt;
  }

  const EventsProto& events = TestStructuredMetricsProvider::ReadEvents();

  for (const auto& event : events.events()) {
    if (event.project_name_hash() == project_name_hash &&
        event.event_name_hash() == event_name_hash) {
      return &event;
    }
  }
  return std::nullopt;
}

std::vector<const StructuredEventProto*>
TestStructuredMetricsProvider::FindEvents(uint64_t project_name_hash,
                                          uint64_t event_name_hash) {
  std::vector<const StructuredEventProto*> events_vector;
  if (!structured_metrics_recorder_->CanProvideMetrics()) {
    return events_vector;
  }

  const EventsProto& events = TestStructuredMetricsProvider::ReadEvents();
  for (const auto& event : events.events()) {
    if (event.project_name_hash() == project_name_hash &&
        event.event_name_hash() == event_name_hash) {
      events_vector.push_back(&event);
    }
  }
  return events_vector;
}

void TestStructuredMetricsProvider::EnableRecording() {
  structured_metrics_recorder_->EnableRecording();
}

void TestStructuredMetricsProvider::DisableRecording() {
  structured_metrics_recorder_->DisableRecording();
}

void TestStructuredMetricsProvider::WaitUntilReady() {
  base::RunLoop run_loop;
  structured_metrics_recorder_->SetOnReadyToRecord(
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
}

void TestStructuredMetricsProvider::SetOnEventsRecordClosure(
    base::RepeatingCallback<void(const Event& event)> event_record_callback) {
  event_record_callback_ = std::move(event_record_callback);
}

void TestStructuredMetricsProvider::OnEventRecord(const Event& event) {
  structured_metrics_recorder_->OnEventRecord(event);
  if (!event_record_callback_) {
    return;
  }

  event_record_callback_.Run(event);
}

}  // namespace metrics::structured
