// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/test/test_structured_metrics_recorder.h"

namespace metrics::structured {

TestStructuredMetricsRecorder* g_test_recorder = nullptr;

TestStructuredMetricsRecorder::TestStructuredMetricsRecorder() = default;
TestStructuredMetricsRecorder::~TestStructuredMetricsRecorder() {
  Destroy();
}

void TestStructuredMetricsRecorder::RecordEvent(Event&& event) {
  // No-op if not properly initialized.
  if (!IsReadyToRecord()) {
    return;
  }

  events_.emplace_back(std::move(event));
}

bool TestStructuredMetricsRecorder::IsReadyToRecord() const {
  return true;
}

TestStructuredMetricsRecorder* TestStructuredMetricsRecorder::Get() {
  return g_test_recorder;
}

void TestStructuredMetricsRecorder::Initialize() {
  g_test_recorder = this;
  StructuredMetricsClient::Get()->SetDelegate(this);
}

void TestStructuredMetricsRecorder::Destroy() {
  g_test_recorder = nullptr;
  StructuredMetricsClient::Get()->UnsetDelegate();
}

const std::vector<Event>& TestStructuredMetricsRecorder::GetEvents() {
  return events_;
}

}  // namespace metrics::structured
