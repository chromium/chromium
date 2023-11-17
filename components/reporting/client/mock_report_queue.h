// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_H_
#define COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace reporting {

// A mock of ReportQueue for use in testing.
class MockReportQueueStrict : public ReportQueue {
 public:
  MockReportQueueStrict();
  ~MockReportQueueStrict() override;

  // Mock AddRecord with record producer.
  // Rarely used, by default calls plain-text AddRecord.
  MOCK_METHOD(void,
              AddProducedRecord,
              (RecordProducer, Priority, EnqueueCallback),
              (const override));

  MOCK_METHOD(void,
              AddRecord,
              (std::string, Priority, EnqueueCallback),
              (const));

  MOCK_METHOD(void, Flush, (Priority, FlushCallback), (override));

  MOCK_METHOD(
      (base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>),
      PrepareToAttachActualQueue,
      (),
      (const override));

  MOCK_METHOD(Destination, GetDestination, (), (const override));

 private:
  // Helper method that executes |record_producer| and in case of success
  // forwards the result to |AddRecord|. In case of failure passes Status to
  // |callback|.
  void ForwardProducedRecord(RecordProducer record_producer,
                             Priority priority,
                             EnqueueCallback callback);
};

// Most of the time no need to log uninterested calls.
typedef ::testing::NiceMock<MockReportQueueStrict> MockReportQueue;

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_H_
