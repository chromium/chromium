// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/dispatch_js_event_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Property;

class DispatchJsEventActionTest : public testing::Test {
 public:
  DispatchJsEventActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, OnDispatchJsEvent(_))
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
  base::MockCallback<Action::ProcessActionCallback> callback_;
  DispatchJsEventProto proto_;
};

TEST_F(DispatchJsEventActionTest, EmptyProtoSetsMessageDoesNothing) {
  EXPECT_CALL(mock_action_delegate_, OnDispatchJsEvent(_));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
