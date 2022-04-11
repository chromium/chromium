// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/stop_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Property;

class StopActionTest : public testing::Test {
 protected:
  StopActionTest() = default;

  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_stop() = proto_;
    action_ =
        std::make_unique<StopAction>(&mock_action_delegate_, action_proto);
    action_->ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  StopProto proto_;

 private:
  std::unique_ptr<StopAction> action_;
};

TEST_F(StopActionTest, CallsShutdownWithoutShowFeedbackChipOnDelegate) {
  EXPECT_CALL(mock_action_delegate_, Shutdown(/*show_feedback_chip=*/false));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(StopActionTest, CallsCloseWithShowFeedbackChipOnDelegate) {
  EXPECT_CALL(mock_action_delegate_, Shutdown(/*show_feedback_chip=*/true));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  proto_.set_show_feedback_chip(true);
  Run();
}

TEST_F(StopActionTest, CallsCloseOnDelegate) {
  EXPECT_CALL(mock_action_delegate_, Close());
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  proto_.set_close_cct(true);
  Run();
}

}  // namespace
}  // namespace autofill_assistant
