// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_H_
#define COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_H_

#include <memory>

#include "base/callback.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

// A mock of ReportQueue for use in testing.
class MockReportQueue : public ReportQueue {
 public:
  MockReportQueue();
  ~MockReportQueue() override;

  MOCK_METHOD(void,
              AddRecord,
              (base::StringPiece, Priority, ReportQueue::EnqueueCallback),
              (const override));

  MOCK_METHOD(void, Flush, (Priority, ReportQueue::FlushCallback), (override));

  MOCK_METHOD(
      (base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>),
      PrepareToAttachActualQueue,
      (),
      (const override));
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_H_
