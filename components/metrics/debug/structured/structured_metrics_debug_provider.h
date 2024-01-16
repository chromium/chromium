// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_DEBUG_PROVIDER_H_
#define COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_DEBUG_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_recorder.h"

namespace metrics::structured {

class StructuredMetricsService;

// Provides events recorded by Structured Metrics Recorder.
// TODO(b/314841749): Update this class to use a watcher interface.
class StructuredMetricsDebugProvider
    : public StructuredMetricsRecorder::Observer {
 public:
  explicit StructuredMetricsDebugProvider(StructuredMetricsService* service);

  StructuredMetricsDebugProvider(const StructuredMetricsDebugProvider&) =
      delete;
  StructuredMetricsDebugProvider& operator=(
      const StructuredMetricsDebugProvider&) = delete;

  ~StructuredMetricsDebugProvider() override;

  // StructuredMetricsRecorder::Observer:
  void OnEventRecorded(const StructuredEventProto& event) override;

  const base::Value::List& events() const { return events_; }

 private:
  // Loads the events that are recorded before the page is loaded.
  void LoadRecordedEvents();

  // Maintain copy of the events to be displayed by the debug page.
  base::Value::List events_;

  raw_ptr<StructuredMetricsService> service_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_DEBUG_PROVIDER_H_
