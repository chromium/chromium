// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/report_progress_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrEq;

class ReportProgressActionTest : public testing::Test {
 public:
  ReportProgressActionTest() = default;

  void SetUp() override {
    ON_CALL(mock_action_delegate_, ReportProgress(_, _))
        .WillByDefault(Return());
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ReportProgressProto proto_;
};

TEST_F(ReportProgressActionTest, ReportProgress) {
  std::string payload = "payload";
  ActionProto action_proto;
  action_proto.mutable_report_progress()->set_payload("payload");
  ReportProgressAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(mock_action_delegate_, ReportProgress(payload, _)).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  action.ProcessAction(callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
