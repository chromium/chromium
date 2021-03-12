// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_H_
#define COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_H_

#include "base/callback.h"
#include "base/values.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
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
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_H_
