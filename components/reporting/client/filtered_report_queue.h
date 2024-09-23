// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_FILTERED_REPORT_QUEUE_H_
#define COMPONENTS_REPORTING_CLIENT_FILTERED_REPORT_QUEUE_H_

#include <memory>
#include <queue>
#include <string>
#include <type_traits>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

namespace internal {
// A concept that determines whether a type U is implicitly convertible from a
// type V.
template <class U, typename V>
concept IsImplicitlyConstructible =
    std::is_constructible_v<U, V> && std::is_convertible_v<V, U>;
}  // namespace internal

// `FilteredReportQueue` is a wrapper template using actual `ReportQueue`
// instance. It will make a call to a provided filtering interface `Filter`
// whenever `Enqueue` API is called.
// As opposed to `ReportQueue`, filtering only accepts one form of `Enqueue` -
// string, dictionary or proto - others would not compile. And since the
// filtering is called before the event is serialized, it can be analyzed or
// deduplicated in its original form, without the need to deserialize it back.
//
// Usage sketch (note that Filter thread-safety is expected to be ensured by the
// caller code here):
//
// proto EventMessage { optional string tag; optional string data; }
//
// class EventFilter : public FilteredReportQueue<EventMessage>::Filter {
//  private:
//   Status is_accepted(const EventMessage& new_event) override {
//     if (!last_event_tag_.empty() && new_event.tag() == last_event_tag_) {
//       return Status(error::ALREADY_EXISTS, "Duplicated event");
//     }
//     last_event_tag_ = new_event.tag();
//     return Status::StatusOK();
//   }
//
//   std::string last_event_tag_;
// };
//
// auto filtered_queue = std::make_unique<FilteredReportQueue<EventMessage>>(
//     std::make_unique<EventFilter>(filter),
//     std::move(actual_report_queue));
//
// ...
//
// EventMessage event;
// event.set_tag(...);
// event.set_data(...);
// filtered_queue->Enqueue(std::move(event), priority, callback);
//
// The event will only be posted, if its tag differs from the tag in the last
// message before it.
//
template <typename T>
class FilteredReportQueue {
 public:
  // `Filter` interface, to be subclassed and owned by `FilteredReportQueue`.
  // When `Enqueue` is called, the `record` parameter is first passed to
  // `is_accepted` and only if the latter returns `Status::OK`, it is actually
  // enqueued. Otherwise returned `Status` is reported with `EnqueueCallback`.
  class Filter {
   public:
    virtual ~Filter() = default;
    // Returns `Status::OK` if `record` is to be actually enqueued
    virtual Status is_accepted(const T& record) = 0;

   protected:
    Filter() = default;
  };

  // Events filtering UMA metric name.
  static constexpr char kFilteredOutEventsUma[] =
      "Browser.ERP.FilteredOutEvents";

  FilteredReportQueue<T>(std::unique_ptr<Filter> filter,
                         std::unique_ptr<ReportQueue> report_queue)
      : filter_(std::move(filter)), report_queue_(std::move(report_queue)) {}

  // String and Dict forms of `Enqueue` are forwarded to `report_queue_`
  template <typename U>
    requires(internal::IsImplicitlyConstructible<std::string, U> ||
             internal::IsImplicitlyConstructible<base::Value::Dict, U>)
  void Enqueue(U record,
               Priority priority,
               ReportQueue::EnqueueCallback callback) const {
    const auto status = filter_->is_accepted(record);
    if (!status.ok()) {
      base::UmaHistogramBoolean(kFilteredOutEventsUma, true);
      std::move(callback).Run(status);
      return;
    }
    base::UmaHistogramBoolean(kFilteredOutEventsUma, false);
    report_queue_->Enqueue(std::move(record), priority, std::move(callback));
  }

  // Proto form of `Enqueue` is forwarded to `report_queue_` wrapping the
  // argument in unique_ptr.
  template <typename U>
    requires(internal::IsImplicitlyConstructible<
             std::unique_ptr<google::protobuf::MessageLite>,
             std::unique_ptr<U>>)
  void Enqueue(U record,
               Priority priority,
               ReportQueue::EnqueueCallback callback) const {
    const auto status = filter_->is_accepted(record);
    if (!status.ok()) {
      base::UmaHistogramBoolean(kFilteredOutEventsUma, true);
      std::move(callback).Run(status);
      return;
    }
    base::UmaHistogramBoolean(kFilteredOutEventsUma, false);
    report_queue_->Enqueue(std::make_unique<U>(std::move(record)), priority,
                           std::move(callback));
  }

  // `Flush` is forwarded to `report_queue_`.
  void Flush(Priority priority, ReportQueue::FlushCallback callback) {
    report_queue_->Flush(priority, std::move(callback));
  }

 private:
  std::unique_ptr<Filter> filter_;
  std::unique_ptr<ReportQueue> report_queue_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_FILTERED_REPORT_QUEUE_H_
