// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_TEST_TEST_STRUCTURED_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_TEST_TEST_STRUCTURED_METRICS_PROVIDER_H_

#include "base/files/scoped_temp_dir.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_recorder.h"

namespace metrics::structured {

class EventsProto;

// TestStructuredMetricsProvider is a wrapper of StructuredMetricsProvider to
// be used for testing.
class TestStructuredMetricsProvider : public Recorder::RecorderImpl {
 public:
  TestStructuredMetricsProvider();
  explicit TestStructuredMetricsProvider(
      scoped_refptr<StructuredMetricsRecorder> recorder);
  virtual ~TestStructuredMetricsProvider();
  TestStructuredMetricsProvider(const TestStructuredMetricsProvider&) = delete;
  TestStructuredMetricsProvider& operator=(
      const TestStructuredMetricsProvider&) = delete;

  const EventsProto& ReadEvents() const;

  // Returns pointer to the first event with the hash |project_name_hash| and
  // |event_name_hash|. If no event is found, returns std::nullopt.
  std::optional<const StructuredEventProto*> FindEvent(
      uint64_t project_name_hash,
      uint64_t event_name_hash);

  // Returns a vector of pointers to the events with the hash
  // |project_name_hash| and |event_name_hash|.
  std::vector<const StructuredEventProto*> FindEvents(
      uint64_t project_name_hash,
      uint64_t event_name_hash);

  void EnableRecording();
  void DisableRecording();

  // Waits until the recorder is fully initialized.
  void WaitUntilReady();

  // Sets a callback that will be called after the event is flushed to
  // persistence.
  void SetOnEventsRecordClosure(
      base::RepeatingCallback<void(const Event& event)> event_record_callback);

 private:
  // Recorder::RecorderImpl:
  void OnEventRecord(const Event& event) override;

  scoped_refptr<StructuredMetricsRecorder> structured_metrics_recorder_;

  base::ScopedTempDir temp_dir_;

  base::RepeatingCallback<void(const Event& event)> event_record_callback_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_TEST_TEST_STRUCTURED_METRICS_PROVIDER_H_
