// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/clear_persistent_ui_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Pointee;
using ::testing::Property;

class ClearPersistentUiActionTest : public testing::Test {
 public:
  ClearPersistentUiActionTest() {}

 protected:
  void Run() {
    ActionProto action_proto;
    action_proto.mutable_clear_persistent_ui();
    auto action = std::make_unique<ClearPersistentUiAction>(
        &mock_action_delegate_, action_proto);
    action->ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
};

TEST_F(ClearPersistentUiActionTest, ClearUi) {
  EXPECT_CALL(mock_action_delegate_, ClearPersistentGenericUi);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();
}
}  // namespace
}  // namespace autofill_assistant
