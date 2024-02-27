// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/test/test_event_storage.h"

#include "base/functional/callback_forward.h"
#include "base/task/current_thread.h"
#include "components/metrics/structured/histogram_util.h"

namespace metrics::structured {

namespace {
using ::google::protobuf::RepeatedPtrField;
}

TestEventStorage::TestEventStorage() = default;

TestEventStorage::~TestEventStorage() = default;

void TestEventStorage::AddEvent(StructuredEventProto event) {
  events()->mutable_non_uma_events()->Add(std::move(event));
}

RepeatedPtrField<StructuredEventProto> TestEventStorage::TakeEvents() {
  return std::move(*events_.mutable_non_uma_events());
}

int TestEventStorage::RecordedEventsCount() const {
  return events_.non_uma_events_size();
}

void TestEventStorage::Purge() {
  events_.clear_uma_events();
  events_.clear_non_uma_events();
}

void TestEventStorage::AddBatchEvents(
    const RepeatedPtrField<StructuredEventProto>& events) {
  events_.mutable_non_uma_events()->MergeFrom(events);
}

void TestEventStorage::CopyEvents(EventsProto* proto) const {
  proto->mutable_non_uma_events()->MergeFrom(events_.non_uma_events());
}

}  // namespace metrics::structured
