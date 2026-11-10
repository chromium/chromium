// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_POPULATIONS_PENDING_EVENTS_BATCH_H_
#define COMPONENTS_PRIVATE_INSIGHTS_POPULATIONS_PENDING_EVENTS_BATCH_H_

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "components/private_insights/event_queue.h"
#include "third_party/federated_compute/src/fcp/client/example_query_result.pb.h"

namespace private_insights {

// A snapshot of unsent events for a single population, detached from the
// queue they were logged to. The batch is created on the main sequence, moved
// to the thread pool where it serializes itself, and moved back to the main
// sequence where it can be requeued if the upload failed. It is owned by
// exactly one sequence at a time, so no synchronization is needed.
//
// The interface is type-erased so that the upload machinery in
// `PrivateInsightsService` can be shared across event proto types /
// populations.
class PendingEventsBatch {
 public:
  virtual ~PendingEventsBatch() = default;

  // Number of events in the batch.
  virtual size_t size() const = 0;

  // Serializes the events into `query_result`. Runs on the thread pool.
  virtual void SerializeToQueryResult(
      fcp::client::ExampleQueryResult* query_result) const = 0;

  // Puts the events back into the queue they came from. Consumes the events;
  // must be called at most once. Runs on the main sequence.
  virtual void Requeue() = 0;
};

// Serializes `events` into `query_result` in the format expected by the
// private logger example query.
template <typename EventProto>
void SerializeEventsToQueryResult(
    const typename EventQueue<EventProto>::Entries& events,
    fcp::client::ExampleQueryResult* query_result) {
  CHECK(query_result);

  query_result->set_result_source(
      fcp::client::ExampleQueryResult::PRIVATE_LOGGER);
  query_result->mutable_stats()->set_output_rows_count(
      static_cast<int32_t>(events.size()));

  auto* proto_values =
      (*query_result->mutable_vector_data()->mutable_vectors())["entry"]
          .mutable_bytes_values();
  auto* time_values =
      (*query_result->mutable_vector_data()
            ->mutable_vectors())["confidential_compute_event_time"]
          .mutable_string_values();
  for (const auto& entry : events) {
    proto_values->add_value(entry.event.SerializeAsString());
    time_values->add_value(base::TimeFormatAsIso8601(entry.timestamp));
  }
}

// `PendingEventsBatch` for a concrete event proto.
template <typename EventProto>
class TypedPendingEventsBatch : public PendingEventsBatch {
 public:
  using Events = typename EventQueue<EventProto>::Entries;
  using RequeueCallback = base::OnceCallback<void(Events)>;

  TypedPendingEventsBatch(Events events, RequeueCallback requeue)
      : events_(std::move(events)), requeue_(std::move(requeue)) {}
  ~TypedPendingEventsBatch() override = default;

  TypedPendingEventsBatch(const TypedPendingEventsBatch&) = delete;
  TypedPendingEventsBatch& operator=(const TypedPendingEventsBatch&) = delete;

  // PendingEventsBatch:
  size_t size() const override { return events_.size(); }
  void SerializeToQueryResult(
      fcp::client::ExampleQueryResult* query_result) const override {
    SerializeEventsToQueryResult<EventProto>(events_, query_result);
  }
  void Requeue() override {
    CHECK(requeue_);
    std::move(requeue_).Run(std::move(events_));
  }

 private:
  Events events_;
  RequeueCallback requeue_;
};

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_POPULATIONS_PENDING_EVENTS_BATCH_H_
