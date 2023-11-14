// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_TEST_TEST_EVENT_STORAGE_H_
#define COMPONENTS_METRICS_STRUCTURED_TEST_TEST_EVENT_STORAGE_H_

#include "components/metrics/structured/event_storage.h"
#include "components/metrics/structured/storage.pb.h"

namespace metrics::structured {

// Simple in-memory event storage for unit and some browser tests.
class TestEventStorage final : public EventStorage {
 public:
  TestEventStorage();

  ~TestEventStorage() override;

  // EventStorage:
  void AddEvent(StructuredEventProto&& event) override;
  void MoveEvents(ChromeUserMetricsExtension& uma_proto) override;
  int RecordedEventsCount() const override;
  void Purge() override;
  void AddBatchEvents(
      const google::protobuf::RepeatedPtrField<StructuredEventProto>& events)
      override;

  EventsProto* events() { return &events_; }
  const EventsProto* events() const { return &events_; }

 private:
  EventsProto events_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_TEST_TEST_EVENT_STORAGE_H_
