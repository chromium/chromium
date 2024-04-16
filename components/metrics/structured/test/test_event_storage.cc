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
  events()->mutable_events()->Add(std::move(event));
}

RepeatedPtrField<StructuredEventProto> TestEventStorage::TakeEvents() {
  return std::move(*events_.mutable_events());
}

int TestEventStorage::RecordedEventsCount() const {
  return events_.events_size();
}

void TestEventStorage::Purge() {
  events_.clear_uma_events();
  events_.clear_events();
}

void TestEventStorage::AddBatchEvents(
    const RepeatedPtrField<StructuredEventProto>& events) {
  events_.mutable_events()->MergeFrom(events);
}

void TestEventStorage::CopyEvents(EventsProto* proto) const {
  proto->mutable_events()->MergeFrom(events_.events());
}

}  // namespace metrics::structured
