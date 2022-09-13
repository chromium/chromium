// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_form_action.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/display_strings_util.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

ShowFormAction::ShowFormAction(ActionDelegate* delegate,
                               const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_show_form() && proto_.show_form().has_form());
}

ShowFormAction::~ShowFormAction() {}

void ShowFormAction::InternalProcessAction(ProcessActionCallback callback) {
  callback_ = std::move(callback);

  // Show the form. This will call OnFormValuesChanged with the initial result,
  // which will in turn show the "Continue" chip.
  if (!delegate_->SetForm(
          std::make_unique<FormProto>(proto_.show_form().form()),
          base::BindRepeating(&ShowFormAction::OnFormValuesChanged,
                              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&ShowFormAction::OnCancelForm,
                         weak_ptr_factory_.GetWeakPtr()))) {
    // The form contains unsupported or invalid inputs.
    EndAction(ClientStatus(UNSUPPORTED));
    return;
  }
}

void ShowFormAction::OnFormValuesChanged(const FormProto::Result* form_result) {
  action_stopwatch_.StartActiveTime();
  // Copy the current values to the action result.
  *processed_action_proto_->mutable_form_result() = *form_result;

  // Show "Continue" chip.
  UserAction user_action =
      UserAction(proto_.show_form().chip(), /* enabled = */ true,
                 /* identifier = */ std::string());
  if (user_action.chip().empty()) {
    user_action.chip().text = GetDisplayStringUTF8(
        ClientSettingsProto::PAYMENT_INFO_CONFIRM, delegate_->GetSettings());
    user_action.chip().type = HIGHLIGHTED_ACTION;
  }
  user_action.SetEnabled(IsFormValid(proto_.show_form().form(), *form_result));
  user_action.SetCallback(base::BindOnce(&ShowFormAction::OnButtonClicked,
                                         weak_ptr_factory_.GetWeakPtr()));

  auto user_actions = std::make_unique<std::vector<UserAction>>();
  user_actions->emplace_back(std::move(user_action));
  delegate_->Prompt(std::move(user_actions),
                    /* disable_force_expand_sheet = */ false);
  action_stopwatch_.StartWaitTime();
}

void ShowFormAction::OnCancelForm(const ClientStatus& status) {
  EndAction(status);
}

bool ShowFormAction::IsFormValid(const FormProto& form,
                                 const FormProto::Result& result) {
  // TODO(crbug.com/806868): Only check validity of inputs whose value changed
  // instead of all inputs.
  DCHECK_EQ(form.inputs_size(), result.input_results_size());
  for (int i = 0; i < form.inputs_size(); i++) {
    const FormInputProto& input = form.inputs(i);
    const FormInputProto::Result& input_result = result.input_results(i);

    switch (input.input_type_case()) {
      case FormInputProto::InputTypeCase::kCounter:
        DCHECK(input_result.has_counter());
        if (!IsCounterInputValid(input.counter(), input_result.counter())) {
          return false;
        }
        break;
      case FormInputProto::InputTypeCase::kSelection:
        DCHECK(input_result.has_selection());
        if (!IsSelectionInputValid(input.selection(),
                                   input_result.selection())) {
          return false;
        }
        break;
      case FormInputProto::InputTypeCase::INPUT_TYPE_NOT_SET:
        NOTREACHED();
        break;
        // Intentionally no default case to make compilation fail if a new value
        // was added to the enum but not to this list.
    }
  }

  return true;
}

bool ShowFormAction::IsCounterInputValid(
    const CounterInputProto& input,
    const CounterInputProto::Result& result) {
  DCHECK_EQ(input.counters_size(), result.values_size());

  if (!input.has_validation_rule())
    return true;

  return IsCounterValidationRuleSatisfied(input.validation_rule(), input,
                                          result);
}

bool ShowFormAction::IsCounterValidationRuleSatisfied(
    const CounterInputProto::ValidationRule& rule,
    const CounterInputProto& input,
    const CounterInputProto::Result& result) {
  switch (rule.rule_type_case()) {
    case CounterInputProto::ValidationRule::RuleTypeCase::kBoolean: {
      // Satisfied if the number of satisfied sub rules is within
      // [min_satisfied_rules; max_satisfied_rules].
      auto boolean_rule = rule.boolean();
      int n = 0;
      for (const CounterInputProto::ValidationRule& sub_rule :
           boolean_rule.sub_rules()) {
        if (IsCounterValidationRuleSatisfied(sub_rule, input, result)) {
          n++;
        }
      }
      return n >= boolean_rule.min_satisfied_rules() &&
             n <= boolean_rule.max_satisfied_rules();
    }
    case CounterInputProto::ValidationRule::RuleTypeCase::kCounter: {
      // Satisfied if the value of |counters[counter_index]| is within
      // [min_value; max_value].
      auto counter_rule = rule.counter();
      int index = counter_rule.counter_index();
      DCHECK_GE(index, 0);
      DCHECK_LT(index, result.values_size());
      int value = result.values(index);
      return value >= counter_rule.min_value() &&
             value <= counter_rule.max_value();
    }
    case CounterInputProto::ValidationRule::RuleTypeCase::kCountersSum: {
      // Satisfied if the sum of all counters values is within [min_value;
      // max_value].
      auto counters_sum_rule = rule.counters_sum();
      long sum = 0;
      for (int i = 0; i < result.values_size(); ++i) {
        DCHECK_LT(i, input.counters_size());
        sum += result.values(i) * input.counters(i).size();
      }
      return sum >= counters_sum_rule.min_value() &&
             sum <= counters_sum_rule.max_value();
    }
    case CounterInputProto::ValidationRule::RuleTypeCase::RULE_TYPE_NOT_SET:
      // Unknown validation rule: suppose it is satisfied.
      return true;
  }
}

bool ShowFormAction::IsSelectionInputValid(
    const SelectionInputProto& input,
    const SelectionInputProto::Result& result) {
  DCHECK_EQ(input.choices_size(), result.selected_size());

  // A selection input is valid if the number of selected choices is
  // greater or equal than |min_selected_choices|.
  int min_selected = input.min_selected_choices();
  if (min_selected == 0)
    return true;

  int n = 0;
  for (bool selected : result.selected()) {
    if (selected && ++n >= min_selected) {
      return true;
    }
  }

  return n >= min_selected;
}

void ShowFormAction::OnButtonClicked() {
  EndAction(ClientStatus(ACTION_APPLIED));
}

void ShowFormAction::EndAction(const ClientStatus& status) {
  action_stopwatch_.StartActiveTime();
  delegate_->CleanUpAfterPrompt();
  delegate_->SetForm(nullptr, base::DoNothing(), base::DoNothing());
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
