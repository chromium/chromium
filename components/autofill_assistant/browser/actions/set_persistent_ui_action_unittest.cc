// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_persistent_ui_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;

class SetPersistentUiActionTest : public testing::Test {
 public:
  SetPersistentUiActionTest() {}

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_set_persistent_ui() = proto_;
    auto action = std::make_unique<SetPersistentUiAction>(
        &mock_action_delegate_, action_proto);
    action->ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  SetPersistentUiProto proto_;
};

TEST_F(SetPersistentUiActionTest, SetUi) {
  EXPECT_CALL(mock_action_delegate_, SetPersistentGenericUi(_, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED)));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  proto_.mutable_generic_user_interface();
  Run();
}

TEST_F(SetPersistentUiActionTest, FailedViewInflationEndsWithError) {
  EXPECT_CALL(mock_action_delegate_, SetPersistentGenericUi(_, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(INVALID_ACTION)));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  proto_.mutable_generic_user_interface();
  Run();
}

TEST_F(SetPersistentUiActionTest, UnsupportedCallbackEndsActionWithError) {
  InSequence sequence;

  EXPECT_CALL(mock_action_delegate_, SetPersistentGenericUi).Times(0);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))))
      .Times(3);

  proto_.mutable_generic_user_interface()
      ->mutable_interactions()
      ->add_interactions()
      ->add_callbacks()
      ->mutable_end_action();
  Run();

  proto_.mutable_generic_user_interface()->clear_interactions();
  proto_.mutable_generic_user_interface()
      ->mutable_interactions()
      ->add_interactions()
      ->add_callbacks()
      ->mutable_toggle_user_action();
  Run();

  proto_.mutable_generic_user_interface()->clear_interactions();
  proto_.mutable_generic_user_interface()
      ->mutable_interactions()
      ->add_interactions()
      ->add_callbacks()
      ->mutable_set_user_actions();
  Run();

  // Other callbacks don't cause an error.
  EXPECT_CALL(mock_action_delegate_, SetPersistentGenericUi(_, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ACTION_APPLIED)));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  proto_.mutable_generic_user_interface()->clear_interactions();
  auto* interaction = proto_.mutable_generic_user_interface()
                          ->mutable_interactions()
                          ->add_interactions();

  interaction->add_callbacks()->mutable_set_value();
  interaction->add_callbacks()->mutable_show_info_popup();
  interaction->add_callbacks()->mutable_show_list_popup();
  interaction->add_callbacks()->mutable_show_calendar_popup();
  interaction->add_callbacks()->mutable_compute_value();
  interaction->add_callbacks()->mutable_set_text();
  interaction->add_callbacks()->mutable_set_view_visibility();
  interaction->add_callbacks()->mutable_set_view_enabled();
  interaction->add_callbacks()->mutable_show_generic_popup();
  interaction->add_callbacks()->mutable_create_nested_ui();
  interaction->add_callbacks()->mutable_clear_view_container();
  interaction->add_callbacks()->mutable_for_each();
  Run();
}

}  // namespace
}  // namespace autofill_assistant
