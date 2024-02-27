// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_EVENT_STORAGE_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_EVENT_STORAGE_H_

#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace metrics {
class StructuredEventProto;
}  // namespace metrics

namespace metrics::structured {

class EventsProto;

// Abstraction for how events are stored in Structured Metrics.
template <typename T,
          template <class> class Container =
              ::google::protobuf::RepeatedPtrField>
class EventStorage {
 public:
  EventStorage() = default;

  virtual ~EventStorage() = default;

  EventStorage(const EventStorage&) = delete;
  EventStorage& operator=(const EventStorage&) = delete;

  virtual bool IsReady() { return true; }

  // A callback to be run when the storage is ready.
  virtual void OnReady() {}

  // Add a new StructuredEventProto to be stored.
  virtual void AddEvent(T event) = 0;

  // External API for removing events from the storage.
  // Intended to be used with a Swap for improved performance.
  virtual Container<T> TakeEvents() = 0;

  // The number of events that have been recorded.
  virtual int RecordedEventsCount() const = 0;

  // Checks if events have been stored.
  bool HasEvents() const { return RecordedEventsCount() > 0; }

  // Delete all events.
  virtual void Purge() = 0;

  // Copies the events out of the event storage.
  virtual void CopyEvents(EventsProto* events_proto) const {}

  // Temporary API for external metrics.
  virtual void AddBatchEvents(const Container<T>& events) {}
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_EVENT_STORAGE_H_
