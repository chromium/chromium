// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENTS_PROCESSOR_INTERFACE_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENTS_PROCESSOR_INTERFACE_H_

#include "components/metrics/structured/event.h"

namespace metrics::structured {

// An interface allowing different classes to add fields to events after the
// events are recorded by a client.
class EventsProcessorInterface {
 public:
  EventsProcessorInterface() = default;

  EventsProcessorInterface(const EventsProcessorInterface& events_processor) =
      delete;
  EventsProcessorInterface& operator=(
      const EventsProcessorInterface& events_processor) = delete;

  virtual ~EventsProcessorInterface() = default;

  // Returns true if |event| should be processed by |this|.
  virtual bool ShouldProcessOnEventRecord(const Event& event) = 0;

  // Processes |event|. Note that this function may mutate |event|.
  virtual void OnEventsRecord(Event* event) = 0;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENTS_PROCESSOR_INTERFACE_H_
