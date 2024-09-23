// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_TEST_TEST_STRUCTURED_METRICS_RECORDER_H_
#define COMPONENTS_METRICS_STRUCTURED_TEST_TEST_STRUCTURED_METRICS_RECORDER_H_

#include <vector>

#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace metrics::structured {

class TestStructuredMetricsRecorder
    : StructuredMetricsClient::RecordingDelegate {
 public:
  TestStructuredMetricsRecorder();
  ~TestStructuredMetricsRecorder() override;

  TestStructuredMetricsRecorder(const TestStructuredMetricsRecorder& recorder) =
      delete;
  TestStructuredMetricsRecorder& operator=(
      const TestStructuredMetricsRecorder& recorder) = delete;

  // RecordingDelegate:
  void RecordEvent(Event&& event) override;
  bool IsReadyToRecord() const override;

  // Initializes the test recorder. Must be called to work properly.
  void Initialize();

  // Unsets the global pointer.
  void Destroy();

  // Returns the number of events captured.
  const std::vector<Event>& GetEvents();

  // Returns the test recorder if Initialize() has been called. Returns
  // nullptr otherwise.
  static TestStructuredMetricsRecorder* Get();

 private:
  std::vector<Event> events_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_TEST_TEST_STRUCTURED_METRICS_RECORDER_H_
