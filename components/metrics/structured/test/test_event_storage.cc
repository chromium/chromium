// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/test/test_event_storage.h"

#include "base/functional/callback_forward.h"
#include "base/task/current_thread.h"
#include "components/metrics/structured/histogram_util.h"

namespace metrics::structured {

TestEventStorage::TestEventStorage() = default;

TestEventStorage::~TestEventStorage() = default;

void TestEventStorage::AddEvent(StructuredEventProto&& event) {
  *events()->add_non_uma_events() = event;
}

void TestEventStorage::MoveEvents(ChromeUserMetricsExtension& uma_proto) {
  StructuredDataProto* proto = uma_proto.mutable_structured_data();
  proto->mutable_events()->Swap(events_.mutable_non_uma_events());

  events_.clear_uma_events();
  events_.clear_non_uma_events();
}

int TestEventStorage::RecordedEventsCount() const {
  return events_.non_uma_events_size();
}

void TestEventStorage::Purge() {
  events_.clear_uma_events();
  events_.clear_non_uma_events();
}

void TestEventStorage::AddBatchEvents(
    const google::protobuf::RepeatedPtrField<StructuredEventProto>& events) {
  events_.mutable_non_uma_events()->MergeFrom(events);
}

}  // namespace metrics::structured
