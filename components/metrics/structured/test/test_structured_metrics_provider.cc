// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/test/test_structured_metrics_provider.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/metrics/structured/test/test_key_data_provider.h"

namespace metrics::structured {

TestStructuredMetricsProvider::TestStructuredMetricsProvider() {
  if (temp_dir_.CreateUniqueTempDir()) {
    system_profile_provider_ = std::make_unique<MetricsProvider>();
    structured_metrics_recorder_ = std::unique_ptr<StructuredMetricsRecorder>(
        new StructuredMetricsRecorder(
            /*write_delay=*/base::Seconds(0), system_profile_provider_.get(),
            temp_dir_.GetPath()
                .Append(FILE_PATH_LITERAL("structured_metrics"))
                .Append(FILE_PATH_LITERAL("local_state_events"))));

    structured_metrics_recorder_->InitializeKeyDataProvider(
        std::make_unique<TestKeyDataProvider>(
            temp_dir_.GetPath()
                .Append(FILE_PATH_LITERAL("structured_metrics"))
                .Append(FILE_PATH_LITERAL("local_state_keys"))));
    structured_metrics_provider_ = std::unique_ptr<StructuredMetricsProvider>(
        new StructuredMetricsProvider(
            /*write_delay=*/base::Seconds(0),
            structured_metrics_recorder_.get()));
    Recorder::GetInstance()->AddObserver(this);
  }
}

TestStructuredMetricsProvider::TestStructuredMetricsProvider(
    std::unique_ptr<StructuredMetricsRecorder> recorder)
    : structured_metrics_recorder_(std::move(recorder)) {
  system_profile_provider_ = std::make_unique<MetricsProvider>();
  structured_metrics_provider_ =
      std::unique_ptr<StructuredMetricsProvider>(new StructuredMetricsProvider(
          /*write_delay=*/base::Seconds(0),
          structured_metrics_recorder_.get()));
  Recorder::GetInstance()->AddObserver(this);
}

TestStructuredMetricsProvider::~TestStructuredMetricsProvider() {
  Recorder::GetInstance()->RemoveObserver(this);
}

void TestStructuredMetricsProvider::EnableRecording() {
  structured_metrics_recorder_->EnableRecording();
}

void TestStructuredMetricsProvider::DisableRecording() {
  structured_metrics_recorder_->DisableRecording();
}

absl::optional<StructuredEventProto> TestStructuredMetricsProvider::FindEvent(
    uint64_t project_name_hash,
    uint64_t event_name_hash) {
  if (!structured_metrics_recorder_->can_provide_local_state_metrics()) {
    return absl::nullopt;
  }

  const EventsProto& local_state_events =
      *structured_metrics_recorder_->local_state_events_proto();

  for (const auto& event : local_state_events.non_uma_events()) {
    if (event.project_name_hash() == project_name_hash &&
        event.event_name_hash() == event_name_hash) {
      LOG(ERROR) << event.metrics()[0].name_hash();
      LOG(ERROR) << event.metrics()[1].name_hash();
      LOG(ERROR) << event.metrics()[1].value_int64();
      return event;
    }
  }

  if (ShouldCheckProfile()) {
    const EventsProto& profile_events =
        *structured_metrics_recorder_->profile_events_proto();
    for (const auto& event : profile_events.non_uma_events()) {
      if (event.project_name_hash() == project_name_hash &&
          event.event_name_hash() == event_name_hash) {
        return event;
      }
    }
  }

  return absl::nullopt;
}

std::vector<const StructuredEventProto*>
TestStructuredMetricsProvider::FindEvents(uint64_t project_name_hash,
                                          uint64_t event_name_hash) {
  std::vector<const StructuredEventProto*> events_vector;
  if (!structured_metrics_recorder_->can_provide_local_state_metrics()) {
    return events_vector;
  }

  const EventsProto& local_state_events =
      *structured_metrics_recorder_->local_state_events_proto();

  for (const auto& event : local_state_events.non_uma_events()) {
    if (event.project_name_hash() == project_name_hash &&
        event.event_name_hash() == event_name_hash) {
      events_vector.push_back(&event);
    }
  }

  if (ShouldCheckProfile()) {
    const EventsProto& profile_events =
        *structured_metrics_recorder_->profile_events_proto();

    for (const auto& event : profile_events.non_uma_events()) {
      if (event.project_name_hash() == project_name_hash &&
          event.event_name_hash() == event_name_hash) {
        events_vector.push_back(&event);
      }
    }
  }

  return events_vector;
}

void TestStructuredMetricsProvider::SetOnEventsRecordClosure(
    base::RepeatingCallback<void(const Event& event)> event_record_callback) {
  event_record_callback_ = std::move(event_record_callback);
}

void TestStructuredMetricsProvider::OnProfileAdded(
    const base::FilePath& profile_path) {
  structured_metrics_recorder_->OnProfileAdded(profile_path);
}

void TestStructuredMetricsProvider::OnEventRecord(const Event& event) {
  if (!event_record_callback_) {
    return;
  }

  event_record_callback_.Run(event);
}

void TestStructuredMetricsProvider::OnReportingStateChanged(bool enabled) {}

absl::optional<int> TestStructuredMetricsProvider::LastKeyRotation(
    uint64_t project_name_hash) {
  return structured_metrics_recorder_->LastKeyRotation(project_name_hash);
}

void TestStructuredMetricsProvider::AddProfilePath(
    const base::FilePath& user_path) {
  OnProfileAdded(temp_dir_.GetPath().Append(user_path));
}

bool TestStructuredMetricsProvider::ShouldCheckProfile() {
  return structured_metrics_recorder_->IsReadyToRecordProfileEvents();
}

}  // namespace metrics::structured
