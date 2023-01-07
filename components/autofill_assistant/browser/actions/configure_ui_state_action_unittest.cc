// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/configure_ui_state_action.h"

#include "action_delegate.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Property;

class ConfigureUiStateActionTest : public testing::Test {
 public:
  ConfigureUiStateActionTest() = default;

 protected:
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
};

TEST_F(ConfigureUiStateActionTest, SetDefault) {
  ActionProto action_proto;
  action_proto.mutable_configure_ui_state()->set_overlay_behavior(
      ConfigureUiStateProto_OverlayBehavior_DEFAULT);
  ConfigureUiStateAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(mock_action_delegate_,
              SetOverlayBehavior(ConfigureUiStateProto_OverlayBehavior_DEFAULT))
      .Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  action.ProcessAction(callback_.Get());
}

TEST_F(ConfigureUiStateActionTest, SetHidden) {
  ActionProto action_proto;
  action_proto.mutable_configure_ui_state()->set_overlay_behavior(
      ConfigureUiStateProto_OverlayBehavior_HIDDEN);
  ConfigureUiStateAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(mock_action_delegate_,
              SetOverlayBehavior(ConfigureUiStateProto_OverlayBehavior_HIDDEN))
      .Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  action.ProcessAction(callback_.Get());
}

TEST_F(ConfigureUiStateActionTest, OverlayBehaviorNotSet) {
  ActionProto action_proto;
  ConfigureUiStateAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(mock_action_delegate_, SetOverlayBehavior).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  action.ProcessAction(callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
