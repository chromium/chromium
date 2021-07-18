// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_FACTORY_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "base/strings/string_piece_forward.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/util/statusor.h"

namespace net {
class BackoffEntry;
}  // namespace net

namespace reporting {

// Report queue factory simplify the creation of ReportQueues by abstracting
// away both the ReportQueueProvider and the ReportQueueConfiguration. It also
// allows automatic retries under the hood if the creation of the ReportQueue
// fails. Usage would be ReportQueueFactory::Create(dm_token, destination,
// success_callback) which represent the bare minimum requirements to create a
// ReportQueue. dm_token is the DMToken value (as StringPiece) under which
// identity the ReportQueue will be created. destination is a requirement to
// define where the event is coming from. success_callback is the callback that
// will pass the ReportQueue back to the model.
class ReportQueueFactory {
 public:
  using SuccessCallback =
      base::OnceCallback<void(std::unique_ptr<ReportQueue>)>;
  using TrySetReportQueueCallback =
      base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>;

  static void Create(base::StringPiece dm_token_value,
                     const Destination destination,
                     SuccessCallback done_cb);

 private:
  static void TrySetReportQueue(
      SuccessCallback success_cb,
      StatusOr<std::unique_ptr<reporting::ReportQueue>> report_queue_result);

  static TrySetReportQueueCallback CreateTrySetCallback(
      base::StringPiece dm_token_value,
      const Destination destination,
      SuccessCallback success_cb,
      std::unique_ptr<net::BackoffEntry> backoff_entry);
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_FACTORY_H_
