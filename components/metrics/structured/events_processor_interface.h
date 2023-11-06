// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENTS_PROCESSOR_INTERFACE_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENTS_PROCESSOR_INTERFACE_H_

#include "base/files/file_path.h"
#include "components/metrics/structured/event.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

namespace {

using ::metrics::ChromeUserMetricsExtension;
using ::metrics::StructuredEventProto;

}  // namespace

// An interface allowing different classes to add fields and metadata to events
// after the events are recorded by a client.
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

  // Processes |event| proto. Note that this function may mutate |event|.
  // This is called by the StructuredMetricsRecorder::OnRecordEvent once the
  // StructuredEventProto is built.
  virtual void OnEventRecorded(StructuredEventProto* event) = 0;

  // Attach metadata when |ProvideIndependentMetrics| is called from the
  // MetricsService. This will be called before events are attached.
  virtual void OnProvideIndependentMetrics(
      ChromeUserMetricsExtension* uma_proto) = 0;

  // Notify the processor that a profile has been added.
  virtual void OnProfileAdded(const base::FilePath& path) {}
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENTS_PROCESSOR_INTERFACE_H_
