// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_DELEGATING_EVENTS_PROCESSOR_H_
#define COMPONENTS_METRICS_STRUCTURED_DELEGATING_EVENTS_PROCESSOR_H_

#include <memory>

#include "components/metrics/structured/event.h"
#include "components/metrics/structured/events_processor_interface.h"

namespace metrics::structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;

}

// DelegatingEventsProcessor manages a set of other EventsProcessorInterfaces.
// Calls to this events processor are forwarded to all of the registered events
// processors.
class DelegatingEventsProcessor final : public EventsProcessorInterface {
 public:
  DelegatingEventsProcessor();
  ~DelegatingEventsProcessor() override;

  // Adds a |events_processor| to forward calls to.
  void AddEventsProcessor(
      std::unique_ptr<EventsProcessorInterface> events_processor);

  // EventsProcessor:
  bool ShouldProcessOnEventRecord(const Event& event) override;
  void OnEventsRecord(Event* event) override;
  void OnEventRecorded(StructuredEventProto* event) override;
  void OnProvideIndependentMetrics(
      ChromeUserMetricsExtension* uma_proto) override;
  void OnProfileAdded(const base::FilePath& path) override;

 private:
  std::vector<std::unique_ptr<EventsProcessorInterface>> events_processors_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_DELEGATING_EVENTS_PROCESSOR_H_
