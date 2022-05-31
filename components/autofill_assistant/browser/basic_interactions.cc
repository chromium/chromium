// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/basic_interactions.h"
#include <algorithm>
#include "base/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {

namespace {

bool BooleanAnd(UserModel* user_model,
                const std::string& result_model_identifier,
                const BooleanAndProto& proto) {
  auto values = user_model->GetValues(proto.values());
  if (!values.has_value()) {
    DVLOG(2) << "Failed to find values in user model";
    return false;
  }

  if (!AreAllValuesOfType(*values, ValueProto::kBooleans) ||
      !AreAllValuesOfSize(*values, 1)) {
    DVLOG(2) << "All values must be 'boolean' and contain exactly 1 value each";
    return false;
  }

  bool result = true;
  for (const auto& value : *values) {
    result &= value.booleans().values(0);
  }
  user_model->SetValue(result_model_identifier,
                       SimpleValue(result, ContainsClientOnlyValue(*values)));
  return true;
}

bool BooleanOr(UserModel* user_model,
               const std::string& result_model_identifier,
               const BooleanOrProto& proto) {
  auto values = user_model->GetValues(proto.values());
  if (!values.has_value()) {
    DVLOG(2) << "Failed to find values in user model";
    return false;
  }

  if (!AreAllValuesOfType(*values, ValueProto::kBooleans) ||
      !AreAllValuesOfSize(*values, 1)) {
    DVLOG(2) << "All values must be 'boolean' and contain exactly 1 value each";
    return false;
  }

  bool result = false;
  for (const auto& value : *values) {
    result |= value.booleans().values(0);
  }
  user_model->SetValue(result_model_identifier,
                       SimpleValue(result, ContainsClientOnlyValue(*values)));
  return true;
}

bool BooleanNot(UserModel* user_model,
                const std::string& result_model_identifier,
                const BooleanNotProto& proto) {
  auto value = user_model->GetValue(proto.value());
  if (!value.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": " << proto.value()
             << " not found in model";
    return false;
  }
  if (value->booleans().values().size() != 1) {
    DVLOG(2) << "Error evaluating " << __func__
             << ": expected single boolean, but got " << *value;
    return false;
  }

  user_model->SetValue(
      result_model_identifier,
      SimpleValue(!value->booleans().values(0), value->is_client_side_only()));
  return true;
}

bool ValueToString(UserModel* user_model,
                   const std::string& result_model_identifier,
                   const ToStringProto& proto) {
  auto value = user_model->GetValue(proto.value());
  if (!value.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": " << proto.value()
             << " not found in model";
    return false;
  }
  if (GetValueSize(*value) == 0) {
    DVLOG(2) << "Error evaluating " << __func__ << ": input value empty";
    return false;
  }

  switch (value->kind_case()) {
    case ValueProto::kUserActions:
    case ValueProto::kLoginOptions:
    case ValueProto::kCreditCardResponse:
    case ValueProto::kServerPayload:
      DVLOG(2) << "Error evaluating " << __func__
               << ": does not support values of type " << value->kind_case();
      return false;
    default:
      break;
  }

  ValueProto result;
  result.set_is_client_side_only(value->is_client_side_only());
  for (int i = 0; i < GetValueSize(*value); ++i) {
    switch (value->kind_case()) {
      case ValueProto::kStrings:
        result.mutable_strings()->add_values(value->strings().values(i));
        break;
      case ValueProto::kBooleans:
        result.mutable_strings()->add_values(
            value->booleans().values(i) ? "true" : "false");
        break;
      case ValueProto::kInts:
        result.mutable_strings()->add_values(
            base::NumberToString(value->ints().values(i)));
        break;
      case ValueProto::kDates: {
        if (proto.date_format().date_format().empty()) {
          DVLOG(2) << "Error evaluating " << __func__
                   << ": date_format not set";
          return false;
        }
        auto date = value->dates().values(i);

        // Technically we are setting the wrong |day_of_week|, but it's ignored
        // in practice and the formatted string will have the correct day for
        // the date. Setting an invalid value here (e.g. -1) causes issues on
        // Windows.
        base::Time::Exploded exploded_time = {static_cast<int>(date.year()),
                                              date.month(),
                                              /* day_of_week = */ 0,
                                              date.day(),
                                              /* hour = */ 0,
                                              /* minute = */ 0,
                                              /* second = */ 0,
                                              /* millisecond = */ 0};

        base::Time time;
        if (!base::Time::FromLocalExploded(exploded_time, &time)) {
          DVLOG(2) << "Error evaluating " << __func__ << ": invalid date "
                   << *value;
          return false;
        }

        result.mutable_strings()->add_values(
            base::UTF16ToUTF8(base::TimeFormatWithPattern(
                time, proto.date_format().date_format().c_str())));
        break;
      }
      case ValueProto::kCreditCards: {
        if (proto.autofill_format().value_expression().chunk().empty()) {
          DVLOG(2) << "Error evaluating " << __func__ << ": pattern not set";
          return false;
        }
        auto* credit_card =
            user_model->GetCreditCard(value->credit_cards().values(i));
        if (!credit_card) {
          DVLOG(2) << "Error evaluating " << __func__
                   << ": credit card not found";
          return false;
        }
        std::string formatted_string;
        auto format_status = field_formatter::FormatExpression(
            proto.autofill_format().value_expression(),
            field_formatter::CreateAutofillMappings(
                *credit_card, proto.autofill_format().locale()),
            /* quote_meta= */ false, &formatted_string);
        if (!format_status.ok()) {
          DVLOG(2) << "Error evaluating " << __func__
                   << ": error formatting pattern '"
                   << proto.autofill_format().value_expression() << "'";
          return false;
        }
        result.mutable_strings()->add_values(formatted_string);
        break;
      }
      case ValueProto::kProfiles: {
        if (proto.autofill_format().value_expression().chunk().empty()) {
          DVLOG(2) << "Error evaluating " << __func__ << ": pattern not set";
          return false;
        }
        auto* profile = user_model->GetProfile(value->profiles().values(i));
        if (!profile) {
          DVLOG(2) << "Error evaluating " << __func__ << ": profile not found";
          return false;
        }
        std::string formatted_string;
        auto format_status = field_formatter::FormatExpression(
            proto.autofill_format().value_expression(),
            field_formatter::CreateAutofillMappings(
                *profile, proto.autofill_format().locale()),
            /* quote_meta= */ false, &formatted_string);
        if (!format_status.ok()) {
          DVLOG(2) << "Error evaluating " << __func__
                   << ": error formatting pattern '"
                   << proto.autofill_format().value_expression() << "'";
          return false;
        }
        result.mutable_strings()->add_values(formatted_string);
        break;
      }
      case ValueProto::kUserActions:
      case ValueProto::kLoginOptions:
      case ValueProto::kCreditCardResponse:
      case ValueProto::kServerPayload:
      case ValueProto::KIND_NOT_SET:
        NOTREACHED();
        return false;
    }
  }

  user_model->SetValue(result_model_identifier, result);
  return true;
}

bool Compare(UserModel* user_model,
             const std::string& result_model_identifier,
             const ValueComparisonProto& proto) {
  auto value_a = user_model->GetValue(proto.value_a());
  if (!value_a.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": " << proto.value_a()
             << " not found in model";
    return false;
  }
  auto value_b = user_model->GetValue(proto.value_b());
  if (!value_b.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": " << proto.value_b()
             << " not found in model";
    return false;
  }
  if (proto.mode() == ValueComparisonProto::UNDEFINED) {
    DVLOG(2) << "Error evaluating " << __func__ << ": mode not set";
    return false;
  }

  if (proto.mode() == ValueComparisonProto::EQUAL) {
    user_model->SetValue(
        result_model_identifier,
        SimpleValue(*value_a == *value_b,
                    ContainsClientOnlyValue({*value_a, *value_b})));
    return true;
  }
  if (proto.mode() == ValueComparisonProto::NOT_EQUAL) {
    user_model->SetValue(
        result_model_identifier,
        SimpleValue(*value_a != *value_b,
                    ContainsClientOnlyValue({*value_a, *value_b})));
    return true;
  }

  // All modes except EQUAL require a size of 1 and a common value type and
  // are only supported for a subset of value types.
  if (!AreAllValuesOfSize({*value_a, *value_b}, 1)) {
    DVLOG(2) << "Error evaluating " << __func__ << ": comparison mode "
             << proto.mode() << "requires all input values to have size 1";
    return false;
  }

  if (!AreAllValuesOfType({*value_a, *value_b}, value_a->kind_case())) {
    DVLOG(2) << "Error evaluating " << __func__ << ": comparison mode "
             << proto.mode()
             << "requires all input values to share the same type, but got "
             << value_a->kind_case() << " and " << value_b->kind_case();
    return false;
  }

  if (value_a->kind_case() != ValueProto::kInts &&
      value_a->kind_case() != ValueProto::kDates &&
      value_a->kind_case() != ValueProto::kStrings) {
    DVLOG(2) << "Error evaluating " << __func__
             << ": the selected comparison mode is only supported for "
                "integers, strings, and dates";
    return false;
  }

  bool result = false;
  switch (proto.mode()) {
    case ValueComparisonProto::LESS:
      result = *value_a < *value_b;
      break;
    case ValueComparisonProto::LESS_OR_EQUAL:
      result = *value_a < *value_b || value_a == value_b;
      break;
    case ValueComparisonProto::GREATER_OR_EQUAL:
      result = *value_a > *value_b || value_a == value_b;
      break;
    case ValueComparisonProto::GREATER:
      result = *value_a > *value_b;
      break;
    case ValueComparisonProto::EQUAL:
    case ValueComparisonProto::NOT_EQUAL:
    case ValueComparisonProto::UNDEFINED:
      NOTREACHED();
      return false;
  }
  user_model->SetValue(
      result_model_identifier,
      SimpleValue(result, ContainsClientOnlyValue({*value_a, *value_b})));
  return true;
}

bool IntegerSum(UserModel* user_model,
                const std::string& result_model_identifier,
                const IntegerSumProto& proto) {
  auto values = user_model->GetValues(proto.values());
  if (!values.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": "
             << "Failed to find values in user model";
    return false;
  }

  if (!AreAllValuesOfSize(*values, 1) ||
      !AreAllValuesOfType(*values, ValueProto::kInts)) {
    DVLOG(2) << "Error evaluating " << __func__ << ": "
             << "all input values must be single integers";
    return false;
  }

  int sum = 0;
  for (const auto& value : *values) {
    sum += value.ints().values(0);
  }

  user_model->SetValue(result_model_identifier,
                       SimpleValue(sum, ContainsClientOnlyValue(*values)));
  return true;
}

bool CreateCreditCardResponse(UserModel* user_model,
                              const std::string& result_model_identifier,
                              const CreateCreditCardResponseProto& proto) {
  auto value = user_model->GetValue(proto.value());
  if (!value.has_value()) {
    DVLOG(2) << "Failed to find value in user model";
    return false;
  }

  if (value->credit_cards().values().size() != 1) {
    DVLOG(2) << "Error evaluating " << __func__
             << ": expected single CreditCardProto, but got " << *value;
    return false;
  }

  auto* credit_card =
      user_model->GetCreditCard(value->credit_cards().values(0));
  if (!credit_card) {
    DVLOG(2) << "Error evaluating " << __func__ << ": card not found for guid "
             << value->credit_cards().values(0).guid();
    return false;
  }

  // The result is intentionally not client_side_only, irrespective of input.
  ValueProto result;
  result.mutable_credit_card_response()->set_network(
      autofill::data_util::GetPaymentRequestData(credit_card->network())
          .basic_card_issuer_network);
  user_model->SetValue(result_model_identifier, result);
  return true;
}

bool CreateLoginOptionResponse(UserModel* user_model,
                               const std::string& result_model_identifier,
                               const CreateLoginOptionResponseProto& proto) {
  auto value = user_model->GetValue(proto.value());
  if (!value.has_value()) {
    DVLOG(2) << "Failed to find value in user model";
    return false;
  }

  if (value->login_options().values().size() != 1) {
    DVLOG(2) << "Error evaluating " << __func__
             << ": expected single LoginOptionProto, but got " << *value;
    return false;
  }

  // The result is intentionally not client_side_only, irrespective of input.
  const LoginOptionProto& login_option = value->login_options().values(0);
  ValueProto result;
  switch (login_option.payload_or_tag_case()) {
    case LoginOptionProto::kPayload:
    case LoginOptionProto::PAYLOAD_OR_TAG_NOT_SET:
      result.set_server_payload(login_option.payload());
      break;

    case LoginOptionProto::kTag:
      result.mutable_strings()->add_values(login_option.tag());
      break;
  }
  user_model->SetValue(result_model_identifier, result);
  return true;
}

bool StringEmpty(UserModel* user_model,
                 const std::string& result_model_identifier,
                 const StringEmptyProto& proto) {
  auto value = user_model->GetValue(proto.value());
  if (!value.has_value()) {
    DVLOG(2) << "Failed to find value in user model";
    return false;
  }

  if (value->strings().values().size() != 1) {
    DVLOG(2) << "Error evaluating " << __func__
             << ": expected single string, but got " << *value;
    return false;
  }

  user_model->SetValue(result_model_identifier,
                       SimpleValue(value->strings().values(0).empty(),
                                   ContainsClientOnlyValue({*value})));
  return true;
}

}  // namespace

base::WeakPtr<BasicInteractions> BasicInteractions::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BasicInteractions::BasicInteractions(ScriptExecutorUiDelegate* ui_delegate,
                                     ExecutionDelegate* execution_delegate)
    : ui_delegate_(ui_delegate), execution_delegate_(execution_delegate) {}

BasicInteractions::~BasicInteractions() {}

const ClientSettings& BasicInteractions::GetClientSettings() {
  return execution_delegate_->GetClientSettings();
}
bool BasicInteractions::SetValue(const SetModelValueProto& proto) {
  if (proto.model_identifier().empty()) {
    DVLOG(2) << "Error setting value: model_identifier empty";
    return false;
  }
  auto value = execution_delegate_->GetUserModel()->GetValue(proto.value());
  if (!value.has_value()) {
    DVLOG(2) << "Error setting value: " << proto.value() << " not found";
    return false;
  }
  execution_delegate_->GetUserModel()->SetValue(proto.model_identifier(),
                                                *value);
  return true;
}

bool BasicInteractions::ComputeValue(const ComputeValueProto& proto) {
  if (proto.result_model_identifier().empty()) {
    DVLOG(2) << "Error computing value: result_model_identifier empty";
    return false;
  }

  switch (proto.kind_case()) {
    case ComputeValueProto::kBooleanAnd:
      if (proto.boolean_and().values().size() == 0) {
        DVLOG(2) << "Error computing ComputeValue::BooleanAnd: no "
                    "values specified";
        return false;
      }
      return BooleanAnd(execution_delegate_->GetUserModel(),
                        proto.result_model_identifier(), proto.boolean_and());
    case ComputeValueProto::kBooleanOr:
      if (proto.boolean_or().values().size() == 0) {
        DVLOG(2) << "Error computing ComputeValue::BooleanOr: no "
                    "values specified";
        return false;
      }
      return BooleanOr(execution_delegate_->GetUserModel(),
                       proto.result_model_identifier(), proto.boolean_or());
    case ComputeValueProto::kBooleanNot:
      if (!proto.boolean_not().has_value()) {
        DVLOG(2) << "Error computing ComputeValue::BooleanNot: "
                    "value not specified";
        return false;
      }
      return BooleanNot(execution_delegate_->GetUserModel(),
                        proto.result_model_identifier(), proto.boolean_not());
    case ComputeValueProto::kToString:
      if (!proto.to_string().has_value()) {
        DVLOG(2) << "Error computing ComputeValue::ToString: "
                    "value not specified";
        return false;
      }
      return ValueToString(execution_delegate_->GetUserModel(),
                           proto.result_model_identifier(), proto.to_string());
    case ComputeValueProto::kComparison:
      return Compare(execution_delegate_->GetUserModel(),
                     proto.result_model_identifier(), proto.comparison());
    case ComputeValueProto::kIntegerSum:
      if (proto.integer_sum().values().size() == 0) {
        DVLOG(2) << "Error computing ComputeValue::IntegerSum: "
                    "no values specified";
        return false;
      }
      return IntegerSum(execution_delegate_->GetUserModel(),
                        proto.result_model_identifier(), proto.integer_sum());
    case ComputeValueProto::kCreateCreditCardResponse:
      if (!proto.create_credit_card_response().has_value()) {
        DVLOG(2) << "Error computing ComputeValue::CreateCreditCardResponse: "
                    "no value specified";
        return false;
      }
      return CreateCreditCardResponse(execution_delegate_->GetUserModel(),
                                      proto.result_model_identifier(),
                                      proto.create_credit_card_response());
    case ComputeValueProto::kCreateLoginOptionResponse:
      if (!proto.create_login_option_response().has_value()) {
        DVLOG(2) << "Error computing ComputeValue::CreateLoginOptionResponse: "
                    "no value specified";
        return false;
      }
      return CreateLoginOptionResponse(execution_delegate_->GetUserModel(),
                                       proto.result_model_identifier(),
                                       proto.create_login_option_response());
    case ComputeValueProto::kStringEmpty:
      if (!proto.string_empty().has_value()) {
        DVLOG(2)
            << "Error computing ComputeValue::StringEmpty: no value specified";
        return false;
      }
      return StringEmpty(execution_delegate_->GetUserModel(),
                         proto.result_model_identifier(), proto.string_empty());
    case ComputeValueProto::KIND_NOT_SET:
      DVLOG(2) << "Error computing value: kind not set";
      return false;
  }
}

bool BasicInteractions::SetUserActions(const SetUserActionsProto& proto) {
  if (!proto.has_user_actions()) {
    DVLOG(2) << "Error setting user actions: user_actions not set";
    return false;
  }
  auto user_actions_value =
      execution_delegate_->GetUserModel()->GetValue(proto.user_actions());
  if (!user_actions_value.has_value()) {
    DVLOG(2) << "Error setting user actions: " << proto.user_actions()
             << " not found in model";
    return false;
  }
  if (!user_actions_value->has_user_actions()) {
    DVLOG(2) << "Error setting user actions: Expected " << proto.user_actions()
             << " to hold UserActions, but found "
             << user_actions_value->kind_case() << " instead";
    return false;
  }

  auto user_actions = std::make_unique<std::vector<UserAction>>();
  for (const auto& user_action : user_actions_value->user_actions().values()) {
    user_actions->push_back({user_action});
    // No callback needed, the framework relies on generic events which will
    // be fired automatically when user actions are called.
    user_actions->back().SetCallback(base::DoNothing());
  }

  ui_delegate_->SetUserActions(std::move(user_actions));
  return true;
}

bool BasicInteractions::ToggleUserAction(const ToggleUserActionProto& proto) {
  auto user_actions_value = execution_delegate_->GetUserModel()->GetValue(
      proto.user_actions_model_identifier());
  if (!user_actions_value.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": "
             << proto.user_actions_model_identifier() << " not found in model";
    return false;
  }
  if (!user_actions_value->has_user_actions()) {
    DVLOG(2) << "Error evaluating " << __func__
             << ": expected user_actions_model_identifier to contain user "
                "actions, but was "
             << *user_actions_value;
    return false;
  }

  auto enabled_value =
      execution_delegate_->GetUserModel()->GetValue(proto.enabled());
  if (!enabled_value.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": " << proto.enabled()
             << " not found in model";
    return false;
  }
  if (enabled_value->booleans().values().size() != 1) {
    DVLOG(2) << "Error evaluating " << __func__
             << ": expected enabled to contain a single bool, but was "
             << *enabled_value;
    return false;
  }

  auto user_action_it = std::find_if(
      user_actions_value->user_actions().values().cbegin(),
      user_actions_value->user_actions().values().cend(),
      [&](const UserActionProto& user_action) {
        return user_action.identifier() == proto.user_action_identifier();
      });
  if (user_action_it == user_actions_value->user_actions().values().cend()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": "
             << proto.user_action_identifier() << " not found in "
             << *user_actions_value;
    return false;
  }
  auto user_action_index =
      user_action_it - user_actions_value->user_actions().values().cbegin();
  user_actions_value->mutable_user_actions()
      ->mutable_values(user_action_index)
      ->set_enabled(enabled_value->booleans().values(0));
  execution_delegate_->GetUserModel()->SetValue(
      proto.user_actions_model_identifier(), *user_actions_value);
  return true;
}

bool BasicInteractions::EndAction(const ClientStatus& status) {
  if (!end_action_callback_) {
    DVLOG(2) << "Failed to EndAction: no callback set";
    return false;
  }

  // It is possible for the action to end before view inflation was finished.
  // In that case, the action can end directly and does not need to receive this
  // callback.
  view_inflation_finished_callback_.Reset();
  std::move(end_action_callback_).Run(status);
  return true;
}

bool BasicInteractions::NotifyViewInflationFinished(
    const ClientStatus& status) {
  if (!view_inflation_finished_callback_) {
    return false;
  }
  std::move(view_inflation_finished_callback_).Run(status);
  return true;
}

bool BasicInteractions::NotifyPersistentViewInflationFinished(
    const ClientStatus& status) {
  if (!persistent_view_inflation_finished_callback_) {
    return false;
  }
  std::move(persistent_view_inflation_finished_callback_).Run(status);
  return true;
}

void BasicInteractions::ClearCallbacks() {
  end_action_callback_.Reset();
  view_inflation_finished_callback_.Reset();
}

void BasicInteractions::ClearPersistentUiCallbacks() {
  persistent_view_inflation_finished_callback_.Reset();
}

void BasicInteractions::SetEndActionCallback(
    base::OnceCallback<void(const ClientStatus&)> end_action_callback) {
  end_action_callback_ = std::move(end_action_callback);
}

void BasicInteractions::SetViewInflationFinishedCallback(
    base::OnceCallback<void(const ClientStatus&)>
        view_inflation_finished_callback) {
  view_inflation_finished_callback_ =
      std::move(view_inflation_finished_callback);
}

void BasicInteractions::SetPersistentViewInflationFinishedCallback(
    base::OnceCallback<void(const ClientStatus&)>
        persistent_view_inflation_finished_callback) {
  persistent_view_inflation_finished_callback_ =
      std::move(persistent_view_inflation_finished_callback);
}

bool BasicInteractions::RunConditionalCallback(
    const std::string& condition_identifier,
    base::RepeatingCallback<void()> callback) {
  auto condition_value =
      execution_delegate_->GetUserModel()->GetValue(condition_identifier);
  if (!condition_value.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": " << condition_identifier
             << " not found in model";
    return false;
  }
  if (condition_value->booleans().values().size() != 1) {
    DVLOG(2) << "Error evaluating " << __func__ << ": expected "
             << condition_identifier << " to contain a single bool, but was "
             << *condition_value;
    return false;
  }
  if (condition_value->booleans().values(0)) {
    callback.Run();
  }
  return true;
}

}  // namespace autofill_assistant
