// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_WATCHER_H_
#define COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_WATCHER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/recorder.h"

namespace metrics::structured {

class StructuredMetricsService;

// Watches the state of the Structured Metrics Recorder to know when an event is
// recorded.
class StructuredMetricsWatcher : public Recorder::RecorderImpl {
 public:
  explicit StructuredMetricsWatcher(StructuredMetricsService* service);

  StructuredMetricsWatcher(const StructuredMetricsWatcher&) = delete;
  StructuredMetricsWatcher& operator=(const StructuredMetricsWatcher&) = delete;

  ~StructuredMetricsWatcher() override;

  // Recorder::RecorderImpl:
  void OnEventRecord(const Event& event) override;
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnReportingStateChanged(bool enabled) override;

  const std::vector<Event>& events() const { return events_; }

 private:
  // Maintain copy of the events to be displayed by the debug page.
  std::vector<Event> events_;

  raw_ptr<StructuredMetricsService> service_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_DEBUG_STRUCTURED_STRUCTURED_METRICS_WATCHER_H_
