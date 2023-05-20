// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_FACTORY_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/strings/string_piece_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/statusor.h"
#include "net/base/backoff_entry.h"

namespace reporting {

// Report queue factory simplifies the creation of ReportQueues by abstracting
// away both the ReportQueueProvider and the ReportQueueConfiguration. It also
// allows automatic retries under the hood if the creation of the ReportQueue
// fails.
//
// To synchronously create `SpeculativeReportQueue`:
//    ... = ReportQueueFactory::CreateSpeculativeReportQueue(dm_token,
//    destination[, reserved_space]);
//
// To asynchronously create `ReportQueue` (currently used in tests only):
//    ReportQueueFactory::Create(event_type, destination, success_callback[,
//    reserved_space]);
//
class ReportQueueFactory {
 public:
  using SuccessCallback =
      base::OnceCallback<void(std::unique_ptr<ReportQueue>)>;
  using TrySetReportQueueCallback =
      base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>;

  // Instantiates regular ReportQueue (asynchronous operation).
  // `event_type` describes the type of events being reported so the provider
  // can determine what DM token needs to be used for reporting purposes.
  // `destination` is a requirement to define where the event is coming from.
  // `reserved_space` is optional. If it is > 0, respective ReportQueue will be
  // "opportunistic" - underlying Storage would only accept an enqueue request
  // if after adding the new record remaining amount of disk space will not drop
  // below `reserved_space`.
  // `success_callback` is the callback that will pass the ReportQueue back to
  // the model.
  static void Create(
      EventType event_type,
      Destination destination,
      SuccessCallback done_cb,
      std::unique_ptr<RateLimiterInterface> rate_limiter = nullptr,
      int64_t reserved_space = 0L);

  // Instantiates and returns SpeculativeReportQueue.
  // `event_type`, `destination` and `reserved_space` have the same meaining as
  // in `Create` factory method above.
  static std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>
  CreateSpeculativeReportQueue(
      EventType event_type,
      Destination destination,
      std::unique_ptr<RateLimiterInterface> rate_limiter = nullptr,
      int64_t reserved_space = 0L);

 private:
  static void TrySetReportQueue(
      SuccessCallback success_cb,
      StatusOr<std::unique_ptr<ReportQueue>> report_queue_result);

  static TrySetReportQueueCallback CreateTrySetCallback(
      Destination destination,
      SuccessCallback success_cb,
      std::unique_ptr<::net::BackoffEntry> backoff_entry);
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_FACTORY_H_
