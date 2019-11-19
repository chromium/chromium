// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class PromptActionTest : public testing::Test {
 public:
  PromptActionTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ON_CALL(mock_web_controller_, OnElementCheck(_, _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus()));
    ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus(), ""));

    ON_CALL(mock_action_delegate_, RunElementChecks)
        .WillByDefault(Invoke([this](BatchElementChecker* checker) {
          checker->Run(&mock_web_controller_);
        }));
    ON_CALL(mock_action_delegate_, Prompt(_))
        .WillByDefault(Invoke(
            [this](std::unique_ptr<std::vector<UserAction>> user_actions) {
              user_actions_ = std::move(user_actions);
            }));
    prompt_proto_ = proto_.mutable_prompt();
  }

 protected:
  // task_env_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_env_;

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ActionProto proto_;
  PromptProto* prompt_proto_;
  std::unique_ptr<std::vector<UserAction>> user_actions_;
};

TEST_F(PromptActionTest, ChoicesMissing) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, SelectButtons) {
  auto* ok_proto = prompt_proto_->add_choices();
  auto* chip = ok_proto->mutable_chip();
  chip->set_text("Ok");
  chip->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  auto* cancel_proto = prompt_proto_->add_choices();
  cancel_proto->mutable_chip()->set_text("Cancel");
  cancel_proto->mutable_chip()->set_type(NORMAL_ACTION);
  cancel_proto->set_server_payload("cancel");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  ASSERT_THAT(user_actions_, Pointee(SizeIs(2)));

  EXPECT_EQ("Ok", (*user_actions_)[0].chip().text);
  EXPECT_EQ(HIGHLIGHTED_ACTION, (*user_actions_)[0].chip().type);

  EXPECT_EQ("Cancel", (*user_actions_)[1].chip().text);
  EXPECT_EQ(NORMAL_ACTION, (*user_actions_)[1].chip().type);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::prompt_choice,
                   Property(&PromptProto::Choice::server_payload, "ok"))))));
  EXPECT_TRUE((*user_actions_)[0].HasCallback());
  (*user_actions_)[0].Call(TriggerContext::CreateEmpty());
}

TEST_F(PromptActionTest, ReportDirectAction) {
  // Ok has a chip and a direct action.
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_direct_action()->add_names("ok");
  ok_proto->set_server_payload("ok");

  // Maybe only has a mappings to direct actions.
  auto* maybe_proto = prompt_proto_->add_choices();
  maybe_proto->mutable_direct_action()->add_names("maybe");
  maybe_proto->mutable_direct_action()->add_names("I_guess");
  maybe_proto->set_server_payload("maybe");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  ASSERT_THAT(user_actions_, Pointee(SizeIs(2)));

  EXPECT_THAT((*user_actions_)[0].direct_action().names, ElementsAre("ok"));
  EXPECT_FALSE((*user_actions_)[0].chip().empty());
  EXPECT_THAT((*user_actions_)[1].direct_action().names,
              UnorderedElementsAre("maybe", "I_guess"));
  EXPECT_TRUE((*user_actions_)[1].chip().empty());
}

TEST_F(PromptActionTest, ShowOnlyIfElementExists) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  ok_proto->add_show_only_if_element_exists()->add_selectors("element");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  ASSERT_THAT(user_actions_, Pointee(IsEmpty()));

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(ClientStatus()));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_THAT(user_actions_, Pointee(IsEmpty()));
}

TEST_F(PromptActionTest, DisabledUnlessElementExists) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  ok_proto->set_allow_disabling(true);
  ok_proto->add_show_only_if_element_exists()->add_selectors("element");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));
  EXPECT_TRUE((*user_actions_)[0].enabled());

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(ClientStatus()));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));
  EXPECT_FALSE((*user_actions_)[0].enabled());
  EXPECT_TRUE((*user_actions_)[0].HasCallback());
}

TEST_F(PromptActionTest, AutoSelect) {
  auto* choice_proto = prompt_proto_->add_choices();
  choice_proto->set_server_payload("auto-select");
  choice_proto->mutable_auto_select_if_element_exists()->add_selectors(
      "element");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
  EXPECT_THAT(user_actions_, Pointee(SizeIs(0)));

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));

  EXPECT_CALL(mock_action_delegate_, CancelPrompt());
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                        Property(&ProcessedActionProto::prompt_choice,
                                 Property(&PromptProto::Choice::server_payload,
                                          "auto-select"))))));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(PromptActionTest, AutoSelectWithButton) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  auto* choice_proto = prompt_proto_->add_choices();
  choice_proto->set_server_payload("auto-select");
  choice_proto->mutable_auto_select_if_element_exists()->add_selectors(
      "element");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                        Property(&ProcessedActionProto::prompt_choice,
                                 Property(&PromptProto::Choice::server_payload,
                                          "auto-select"))))));
  task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(PromptActionTest, Terminate) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  {
    PromptAction action(&mock_action_delegate_, proto_);
    action.ProcessAction(callback_.Get());
  }

  // Chips pointing to a deleted action do nothing.
  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));
  EXPECT_TRUE((*user_actions_)[0].HasCallback());
  (*user_actions_)[0].Call(TriggerContext::CreateEmpty());
}

TEST_F(PromptActionTest, NoMessageSet) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(_)).Times(0);

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, EmptyMessage) {
  prompt_proto_->set_message("");
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq("")));

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, NormalMessageSet) {
  prompt_proto_->set_message(" test message ");
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq(" test message ")));

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
