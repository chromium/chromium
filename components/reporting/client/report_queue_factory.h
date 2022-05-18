// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_FACTORY_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "base/strings/string_piece_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/util/statusor.h"
#include "net/base/backoff_entry.h"

namespace reporting {

// Report queue factory simplifies the creation of ReportQueues by abstracting
// away both the ReportQueueProvider and the ReportQueueConfiguration. It also
// allows automatic retries under the hood if the creation of the ReportQueue
// fails.
//
// Usage:
// 1. ReportQueueFactory::Create(dm_token, destination, success_callback)
// 2. ReportQueueFactory::Create(event_type, destination, success_callback)
//
// Option 1 is deprecated in favor of option 2 since the new version retrieves
// DM tokens autonomously without forcing the user to specify them before hand.
// dm_token is the DMToken value (as StringPiece) under which
// identity the ReportQueue will be created. event_type describes the type of
// events being reported so the provider can determine what DM token needs to be
// used for reporting purposes. destination is a requirement to
// define where the event is coming from. success_callback is the callback that
// will pass the ReportQueue back to the model.
class ReportQueueFactory {
 public:
  using SuccessCallback =
      base::OnceCallback<void(std::unique_ptr<ReportQueue>)>;
  using TrySetReportQueueCallback =
      base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>;

  static void Create(EventType event_type,
                     Destination destination,
                     SuccessCallback done_cb);

  static std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>
  CreateSpeculativeReportQueue(EventType event_type, Destination destination);

 private:
  static void TrySetReportQueue(
      SuccessCallback success_cb,
      StatusOr<std::unique_ptr<reporting::ReportQueue>> report_queue_result);

  static TrySetReportQueueCallback CreateTrySetCallback(
      Destination destination,
      SuccessCallback success_cb,
      std::unique_ptr<::net::BackoffEntry> backoff_entry);
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_FACTORY_H_
