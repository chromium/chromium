// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/register_self_contained_interrupt_scripts_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

class RegisterSelfContainedInterruptScriptsActionTest : public testing::Test {
 public:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_register_interrupt_scripts() = proto_;
    RegisterSelfContainedInterruptScriptsAction action(&mock_action_delegate_,
                                                       action_proto);
    action.ProcessAction(callback_.Get());
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  RegisterSelfContainedInterruptScripts proto_;
};

TEST_F(RegisterSelfContainedInterruptScriptsActionTest, SmokeTest) {
  Run();
}

}  // namespace
}  // namespace autofill_assistant
