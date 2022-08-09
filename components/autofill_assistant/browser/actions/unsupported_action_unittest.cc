// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/unsupported_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Property;

class UnsupportedActionTest : public testing::Test {
 public:
  UnsupportedActionTest() = default;

  void SetUp() override {}

 protected:
  void Run() {
    ActionProto action_proto;
    UnsupportedAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
};

TEST_F(UnsupportedActionTest, SetsStatusUnsupportedAction) {
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              UNSUPPORTED_ACTION))));

  Run();
}

}  // namespace
}  // namespace autofill_assistant
