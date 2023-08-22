// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_TEST_TEST_STRUCTURED_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_TEST_TEST_STRUCTURED_METRICS_PROVIDER_H_

#include "base/files/scoped_temp_dir.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_provider.h"
#include "components/metrics/structured/structured_metrics_recorder.h"

namespace metrics::structured {

class EventsProto;

// TestStructuredMetricsProvider is a wrapper of StructuredMetricsProvider to
// be used for testing.

class TestStructuredMetricsProvider : public Recorder::RecorderImpl {
 public:
  TestStructuredMetricsProvider();
  ~TestStructuredMetricsProvider() override;
  TestStructuredMetricsProvider(const TestStructuredMetricsProvider&) = delete;
  TestStructuredMetricsProvider& operator=(
      const TestStructuredMetricsProvider&) = delete;

  const EventsProto& ReadEvents();

  // Returns pointer to the first event with the hash |project_name_hash| and
  // |event_name_hash|. If no event is found, returns absl::nullopt.
  absl::optional<const StructuredEventProto*> FindEvent(
      uint64_t project_name_hash,
      uint64_t event_name_hash);

  // Returns a vector of pointers to the events with the hash
  // |project_name_hash| and |event_name_hash|.
  std::vector<const StructuredEventProto*> FindEvents(
      uint64_t project_name_hash,
      uint64_t event_name_hash);

  void EnableRecording();
  void DisableRecording();

  // Sets a callback that will be called after the event is flushed to
  // persistence.
  void SetOnEventsRecordClosure(
      base::RepeatingCallback<void(const Event& event)> event_record_callback);

 private:
  // Recorder::RecorderImpl:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnEventRecord(const Event& event) override;
  void OnReportingStateChanged(bool enabled) override;
  absl::optional<int> LastKeyRotation(uint64_t project_name_hash) override;

  std::unique_ptr<MetricsProvider> system_profile_provider_;

  std::unique_ptr<StructuredMetricsRecorder> structured_metrics_recorder_;

  std::unique_ptr<StructuredMetricsProvider> structured_metrics_provider_;

  base::ScopedTempDir temp_dir_;

  base::RepeatingCallback<void(const Event& event)> event_record_callback_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_TEST_TEST_STRUCTURED_METRICS_PROVIDER_H_
