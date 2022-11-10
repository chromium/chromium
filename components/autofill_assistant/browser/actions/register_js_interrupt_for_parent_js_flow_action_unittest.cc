// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/register_js_interrupt_for_parent_js_flow_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

class RegisterJsInterruptForParentJsFlowActionTest : public testing::Test {
 public:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_register_js_interrupt_for_flow() = proto_;
    RegisterJsInterruptForParentJsFlowAction action(&mock_action_delegate_,
                                                    action_proto);
    action.ProcessAction(callback_.Get());
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  RegisterJsInterruptForParentJsFlow proto_;
};

TEST_F(RegisterJsInterruptForParentJsFlowActionTest, SmokeTest) {
  Run();
}

}  // namespace
}  // namespace autofill_assistant
