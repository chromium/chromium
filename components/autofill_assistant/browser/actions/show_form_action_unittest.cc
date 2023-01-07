// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_form_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_action.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class ShowFormActionTest : public testing::Test {
 public:
  ShowFormActionTest() = default;

  void SetUp() override {
    ON_CALL(mock_action_delegate_, SetForm(_, _, _))
        .WillByDefault(DoAll(RunCallback<1>(&result_), Return(true)));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_show_form() = proto_;
    ShowFormAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ShowFormProto proto_;
  FormProto::Result result_;
};

TEST_F(ShowFormActionTest, SucceedsWithoutValidation) {
  EXPECT_CALL(mock_action_delegate_,
              Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, true))),
                     _, _, _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* counter = input->mutable_counter()->add_counters();
  counter->set_min_value(0);
  counter->set_max_value(1);
  counter->set_label("Counter");

  auto* input_result = result_.add_input_results();
  input_result->mutable_counter()->add_values(1);

  Run();
}

TEST_F(ShowFormActionTest, SucceedsWithValidForm) {
  EXPECT_CALL(mock_action_delegate_,
              Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, true))),
                     _, _, _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* counter = input->mutable_counter()->add_counters();
  counter->set_min_value(0);
  counter->set_max_value(1);
  counter->set_label("Counter");
  auto* rule = input->mutable_counter()
                   ->mutable_validation_rule()
                   ->mutable_counters_sum();
  rule->set_min_value(0);
  rule->set_max_value(2);

  auto* input_result = result_.add_input_results();
  input_result->mutable_counter()->add_values(1);

  Run();
}

TEST_F(ShowFormActionTest, FailsWithInvalidForm) {
  EXPECT_CALL(
      mock_action_delegate_,
      Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, false))), _, _,
             _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* counter = input->mutable_counter()->add_counters();
  counter->set_min_value(0);
  counter->set_max_value(5);
  counter->set_label("Counter");
  auto* rule = input->mutable_counter()
                   ->mutable_validation_rule()
                   ->mutable_counters_sum();
  rule->set_min_value(0);
  rule->set_max_value(2);

  auto* input_result = result_.add_input_results();
  input_result->mutable_counter()->add_values(3);

  Run();
}

TEST_F(ShowFormActionTest, SucceedsWithValidFormWithWeight) {
  EXPECT_CALL(mock_action_delegate_,
              Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, true))),
                     _, _, _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* counter = input->mutable_counter()->add_counters();
  counter->set_min_value(0);
  counter->set_max_value(1);
  counter->set_label("Counter");
  counter->set_size(4);
  auto* rule = input->mutable_counter()
                   ->mutable_validation_rule()
                   ->mutable_counters_sum();
  rule->set_min_value(0);
  rule->set_max_value(10);

  auto* input_result = result_.add_input_results();
  input_result->mutable_counter()->add_values(1);

  Run();
}

TEST_F(ShowFormActionTest, FailsWithInvalidFormWithWeight) {
  EXPECT_CALL(
      mock_action_delegate_,
      Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, false))), _, _,
             _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* counter = input->mutable_counter()->add_counters();
  counter->set_min_value(0);
  counter->set_max_value(2);
  counter->set_label("Counter");
  counter->set_size(4);
  auto* rule = input->mutable_counter()
                   ->mutable_validation_rule()
                   ->mutable_counters_sum();
  rule->set_min_value(0);
  rule->set_max_value(5);

  auto* input_result = result_.add_input_results();
  input_result->mutable_counter()->add_values(2);

  Run();
}

TEST_F(ShowFormActionTest, SetFormCallFails) {
  ON_CALL(mock_action_delegate_, SetForm(_, _, _)).WillByDefault(Return(false));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, UNSUPPORTED))));

  proto_.mutable_form();
  Run();
}

TEST_F(ShowFormActionTest, FailsWithTooManyBooleanRulesSatisfied) {
  EXPECT_CALL(
      mock_action_delegate_,
      Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, false))), _, _,
             _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* counter = input->mutable_counter()->add_counters();
  counter->set_min_value(0);
  counter->set_max_value(1);
  counter->set_label("Counter");
  auto* rule =
      input->mutable_counter()->mutable_validation_rule()->mutable_boolean();
  rule->set_min_satisfied_rules(1);
  rule->set_max_satisfied_rules(2);

  // All three rules are satisfied.
  for (int i = 1; i <= 3; ++i) {
    auto* sub_rule = rule->add_sub_rules()->mutable_counter();
    sub_rule->set_counter_index(0);
    sub_rule->set_min_value(i);
    sub_rule->set_max_value(3);
  }

  auto* input_result = result_.add_input_results();
  input_result->mutable_counter()->add_values(3);

  Run();
}

TEST_F(ShowFormActionTest, FailsWithTooLittleBooleanRulesSatisfied) {
  EXPECT_CALL(
      mock_action_delegate_,
      Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, false))), _, _,
             _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* counter = input->mutable_counter()->add_counters();
  counter->set_min_value(0);
  counter->set_max_value(1);
  counter->set_label("Counter");
  auto* rule =
      input->mutable_counter()->mutable_validation_rule()->mutable_boolean();
  rule->set_min_satisfied_rules(1);
  rule->set_max_satisfied_rules(2);

  // None of the rules are satisfied.
  for (int i = 1; i <= 3; ++i) {
    auto* sub_rule = rule->add_sub_rules()->mutable_counter();
    sub_rule->set_counter_index(0);
    sub_rule->set_min_value(i);
    sub_rule->set_max_value(3);
  }

  auto* input_result = result_.add_input_results();
  input_result->mutable_counter()->add_values(0);

  Run();
}

TEST_F(ShowFormActionTest, SucceedsWithEnoughBooleanRulesSatisfied) {
  EXPECT_CALL(mock_action_delegate_,
              Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, true))),
                     _, _, _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* counter = input->mutable_counter()->add_counters();
  counter->set_min_value(0);
  counter->set_max_value(1);
  counter->set_label("Counter");
  auto* rule =
      input->mutable_counter()->mutable_validation_rule()->mutable_boolean();
  rule->set_min_satisfied_rules(1);
  rule->set_max_satisfied_rules(2);

  // Only the first rule is satisfied.
  for (int i = 1; i <= 3; ++i) {
    auto* sub_rule = rule->add_sub_rules()->mutable_counter();
    sub_rule->set_counter_index(0);
    sub_rule->set_min_value(i);
    sub_rule->set_max_value(3);
  }

  auto* input_result = result_.add_input_results();
  input_result->mutable_counter()->add_values(1);

  Run();
}

TEST_F(ShowFormActionTest, SucceedsInputSelectionValidation) {
  EXPECT_CALL(mock_action_delegate_,
              Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, true))),
                     _, _, _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* selection = input->mutable_selection();
  selection->set_min_selected_choices(1);
  selection->add_choices();
  selection->add_choices();

  auto* input_result = result_.add_input_results();
  input_result->mutable_selection()->add_selected(true);
  input_result->mutable_selection()->add_selected(true);

  Run();
}

TEST_F(ShowFormActionTest, FailsInputSelectionValidation) {
  EXPECT_CALL(
      mock_action_delegate_,
      Prompt(Pointee(ElementsAre(Property(&UserAction::enabled, false))), _, _,
             _, _));

  auto* input = proto_.mutable_form()->add_inputs();
  auto* selection = input->mutable_selection();
  selection->set_min_selected_choices(2);
  selection->add_choices();
  selection->add_choices();

  auto* input_result = result_.add_input_results();
  input_result->mutable_selection()->add_selected(false);
  input_result->mutable_selection()->add_selected(true);

  Run();
}

}  // namespace
}  // namespace autofill_assistant
