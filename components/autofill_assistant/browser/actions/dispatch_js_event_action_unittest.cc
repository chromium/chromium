// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/dispatch_js_event_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Property;
using ::testing::Return;

class DispatchJsEventActionTest : public testing::Test {
 public:
  DispatchJsEventActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebController())
        .WillByDefault(Return(&mock_web_controller_));
    ON_CALL(mock_web_controller_, DispatchJsEvent(_))
        .WillByDefault(RunOnceCallback<0>(OkClientStatus()));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_dispatch_js_event() = proto_;
    DispatchJsEventAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  DispatchJsEventProto proto_;
};

TEST_F(DispatchJsEventActionTest, EmptyProtoSendsEvent) {
  EXPECT_CALL(mock_web_controller_, DispatchJsEvent(_));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
