// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_info_box_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Property;

class ShowInfoBoxActionTest : public testing::Test {
 public:
  ShowInfoBoxActionTest() {}

  void SetUp() override {}

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_show_info_box() = proto_;
    ShowInfoBoxAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ShowInfoBoxProto proto_;
};

TEST_F(ShowInfoBoxActionTest, EmptyActionClearsInfoBox) {
  EXPECT_CALL(mock_action_delegate_, ClearInfoBox);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowInfoBoxActionTest, SetsInfoBox) {
  EXPECT_CALL(mock_action_delegate_, SetInfoBox);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  proto_.mutable_info_box();
  Run();
}

}  // namespace
}  // namespace autofill_assistant
