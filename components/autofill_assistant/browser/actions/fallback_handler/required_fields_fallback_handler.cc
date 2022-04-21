// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {
namespace {

const char kSelectElementTag[] = "SELECT";

AutofillErrorInfoProto::AutofillFieldError* AddAutofillError(
    const RequiredField& required_field,
    ClientStatus* client_status) {
  auto* field_error = client_status->mutable_details()
                          ->mutable_autofill_error_info()
                          ->add_autofill_field_error();
  *field_error->mutable_field() = required_field.selector.proto;
  field_error->set_value_expression(
      field_formatter::GetHumanReadableValueExpression(
          required_field.proto.value_expression()));
  return field_error;
}

void FillStatusDetailsWithMissingFallbackData(
    const RequiredField& required_field,
    ClientStatus* client_status) {
  auto* field_error = AddAutofillError(required_field, client_status);
  field_error->set_no_fallback_value(true);
}

void FillStatusDetailsWithError(const RequiredField& required_field,
                                ProcessedActionStatusProto error_status,
                                ClientStatus* client_status) {
  auto* field_error = AddAutofillError(required_field, client_status);
  field_error->set_status(error_status);
}

void FillStatusDetailsWithEmptyField(const RequiredField& required_field,
                                     ClientStatus* client_status) {
  auto* field_error = AddAutofillError(required_field, client_status);
  field_error->set_empty_after_fallback(true);
}

void FillStatusDetailsWithNotClearedField(const RequiredField& required_field,
                                          ClientStatus* client_status) {
  auto* field_error = AddAutofillError(required_field, client_status);
  field_error->set_filled_after_clear(true);
}

ClientStatus& ErrorStatusWithDefault(ClientStatus& status) {
  if (status.ok()) {
    status.set_proto_status(AUTOFILL_INCOMPLETE);
  }
  return status;
}

ClientStatus GetRe2Value(
    const RequiredField& required_field,
    const base::flat_map<field_formatter::Key, std::string>& mappings,
    bool use_contains,
    std::string* re2_value,
    bool* case_sensitive) {
  if (required_field.proto.has_option_comparison_value_expression_re2()) {
    ClientStatus status = field_formatter::FormatExpression(
        required_field.proto.option_comparison_value_expression_re2()
            .value_expression(),
        mappings,
        /* quote_meta= */ true, re2_value);
    if (!status.ok()) {
      return status;
    }
    *case_sensitive =
        required_field.proto.option_comparison_value_expression_re2()
            .case_sensitive();
    return OkClientStatus();
  }

  std::string re2;
  ClientStatus status =
      field_formatter::FormatExpression(required_field.proto.value_expression(),
                                        mappings, /* quote_meta= */ true, &re2);
  if (!status.ok()) {
    return status;
  }

  if (use_contains) {
    re2_value->assign(re2);
  } else {
    switch (required_field.proto.select_strategy()) {
      case UNSPECIFIED_SELECT_STRATEGY:
      case LABEL_STARTS_WITH:
        // This is the legacy default.
        re2_value->assign(base::StrCat({"^", re2}));
        break;
      case VALUE_MATCH:
      case LABEL_MATCH:
        re2_value->assign(base::StrCat({"^", re2, "$"}));
        break;
    }
  }
  *case_sensitive = false;
  return OkClientStatus();
}

}  // namespace

RequiredFieldsFallbackHandler::~RequiredFieldsFallbackHandler() = default;

RequiredFieldsFallbackHandler::RequiredFieldsFallbackHandler(
    const std::vector<RequiredField>& required_fields,
    const base::flat_map<field_formatter::Key, std::string>& fallback_values,
    ActionDelegate* delegate)
    : required_fields_(required_fields),
      fallback_values_(fallback_values),
      action_delegate_(delegate) {}

void RequiredFieldsFallbackHandler::CheckAndFallbackRequiredFields(
    const ClientStatus& initial_autofill_status,
    base::OnceCallback<void(const ClientStatus&)> status_update_callback) {
  client_status_ = initial_autofill_status;
  status_update_callback_ = std::move(status_update_callback);

  if (required_fields_.empty()) {
    if (!initial_autofill_status.ok()) {
      VLOG(1) << __func__ << " Autofill failed and no fallback provided "
              << initial_autofill_status.proto_status();
    }

    std::move(status_update_callback_).Run(initial_autofill_status);
    return;
  }

  if (!client_status_.ok()) {
    client_status_.mutable_details()
        ->mutable_autofill_error_info()
        ->set_autofill_error_status(client_status_.proto_status());
  }

  CheckAllRequiredFields(/* apply_fallback = */ true);
}

void RequiredFieldsFallbackHandler::CheckAllRequiredFields(
    bool apply_fallback) {
  DCHECK(!batch_element_checker_);
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  for (size_t i = 0; i < required_fields_.size(); i++) {
    // First run (with fallback) we skip checking forced fields, since we
    // overwrite them anyway. Second run (without fallback) forced fields should
    // be checked.
    if (required_fields_[i].proto.forced() && apply_fallback) {
      continue;
    }

    // We cannot check the value of elements with custom fallback clicks. Those
    // elements are JS driven structures that in most cases lack a "value"
    // attribute. We define a successful click on the element as successfully
    // filling the form field.
    if (required_fields_[i].proto.has_option_element_to_click()) {
      continue;
    }

    batch_element_checker_->AddFieldValueCheck(
        required_fields_[i].selector,
        base::BindOnce(&RequiredFieldsFallbackHandler::OnGetRequiredFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }

  batch_element_checker_->AddAllDoneCallback(
      base::BindOnce(&RequiredFieldsFallbackHandler::OnCheckRequiredFieldsDone,
                     weak_ptr_factory_.GetWeakPtr(), apply_fallback));
  action_delegate_->RunElementChecks(batch_element_checker_.get());
}

void RequiredFieldsFallbackHandler::OnGetRequiredFieldValue(
    size_t required_fields_index,
    const ClientStatus& element_status,
    const std::string& value) {
  required_fields_[required_fields_index].status =
      value.empty() ? RequiredField::EMPTY : RequiredField::NOT_EMPTY;
}

void RequiredFieldsFallbackHandler::OnCheckRequiredFieldsDone(
    bool apply_fallback) {
  batch_element_checker_.reset();

  // We process all fields with an empty value in order to perform the fallback
  // on all those fields, if any.
  bool should_fallback = false;
  for (const RequiredField& required_field : required_fields_) {
    if (required_field.ShouldFallback(apply_fallback)) {
      should_fallback = true;
      if (!apply_fallback) {
        if (required_field.proto.value_expression().chunk().empty()) {
          VLOG(1) << "Field was filled after attempting to clear it: "
                  << required_field.selector;
          FillStatusDetailsWithNotClearedField(required_field, &client_status_);
        } else {
          VLOG(1) << "Field was empty after applying fallback: "
                  << required_field.selector;
          FillStatusDetailsWithEmptyField(required_field, &client_status_);
        }
      }
    }
  }

  if (!should_fallback) {
    std::move(status_update_callback_).Run(client_status_);
    return;
  }

  if (!apply_fallback) {
    // Validation failed and we don't want to try the fallback.
    std::move(status_update_callback_)
        .Run(ErrorStatusWithDefault(client_status_));
    return;
  }

  for (const RequiredField& required_field : required_fields_) {
    if (required_field.proto.value_expression().chunk().empty() ||
        !required_field.ShouldFallback(/* apply_fallback= */ true)) {
      continue;
    }

    std::string tmp;
    if (!field_formatter::FormatExpression(
             required_field.proto.value_expression(), fallback_values_,
             /* quote_meta= */ false, &tmp)
             .ok()) {
      DVLOG(3) << "Field has no fallback data: " << required_field.selector
               << " " << required_field.proto.value_expression();
      FillStatusDetailsWithMissingFallbackData(required_field, &client_status_);
    }
  }

  // Set the fallback values and check again.
  SetFallbackFieldValuesSequentially(0);
}

void RequiredFieldsFallbackHandler::SetFallbackFieldValuesSequentially(
    size_t required_fields_index) {
  // Skip non-empty fields.
  while (required_fields_index < required_fields_.size() &&
         !required_fields_[required_fields_index].ShouldFallback(
             /* apply_fallback= */ true)) {
    required_fields_index++;
  }

  // If there are no more fields to set, check the required fields again,
  // but this time we don't want to try the fallback in case of failure.
  if (required_fields_index >= required_fields_.size()) {
    DCHECK_EQ(required_fields_index, required_fields_.size());

    CheckAllRequiredFields(/* apply_fallback= */ false);
    return;
  }

  // Treat the next field.
  const RequiredField& required_field = required_fields_[required_fields_index];
  base::OnceCallback<void()> set_next_field = base::BindOnce(
      &RequiredFieldsFallbackHandler::SetFallbackFieldValuesSequentially,
      weak_ptr_factory_.GetWeakPtr(), required_fields_index + 1);

  std::string fallback_value;
  ClientStatus format_status = field_formatter::FormatExpression(
      required_field.proto.value_expression(), fallback_values_,
      /* quote_meta= */ false, &fallback_value);
  if (!format_status.ok()) {
    // Skip optional field, fail otherwise.
    if (required_field.proto.is_optional()) {
      std::move(set_next_field).Run();
    } else {
      std::move(status_update_callback_)
          .Run(ErrorStatusWithDefault(client_status_));
    }
    return;
  }

  if (required_field.proto.has_option_element_to_click()) {
    FillJsDrivenDropdown(fallback_value, required_field,
                         std::move(set_next_field));
  } else {
    FillFormField(fallback_value, required_field, std::move(set_next_field));
  }
}

void RequiredFieldsFallbackHandler::FillFormField(
    const std::string& value,
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field) {
  action_delegate_->FindElement(
      required_field.selector,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnFindElement,
                     weak_ptr_factory_.GetWeakPtr(), value, required_field,
                     std::move(set_next_field)));
}

void RequiredFieldsFallbackHandler::OnFindElement(
    const std::string& value,
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinderResult> element_result) {
  if (!element_status.ok()) {
    FillStatusDetailsWithError(required_field, element_status.proto_status(),
                               &client_status_);

    // Element to operate on does not exist. We either continue or stop the
    // script without checking the other fields.
    if (required_field.proto.is_optional() &&
        element_status.proto_status() == ELEMENT_RESOLUTION_FAILED) {
      std::move(set_next_field).Run();
    } else {
      std::move(status_update_callback_)
          .Run(ErrorStatusWithDefault(client_status_));
    }
    return;
  }

  const ElementFinderResult* element_result_ptr = element_result.get();
  action_delegate_->GetWebController()->GetElementTag(
      *element_result_ptr,
      base::BindOnce(
          &RequiredFieldsFallbackHandler::OnGetFallbackFieldElementTag,
          weak_ptr_factory_.GetWeakPtr(), value, required_field,
          std::move(set_next_field), std::move(element_result)));
}

void RequiredFieldsFallbackHandler::OnGetFallbackFieldElementTag(
    const std::string& value,
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field,
    std::unique_ptr<ElementFinderResult> element,
    const ClientStatus& element_tag_status,
    const std::string& element_tag) {
  if (!element_tag_status.ok()) {
    DVLOG(3) << "Status for element tag was "
             << element_tag_status.proto_status();
  }
  const ElementFinderResult* element_ptr = element.get();
  base::OnceCallback<void(const ClientStatus&)> on_set_field_value =
      base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_field,
                     std::move(set_next_field), std::move(element));

  DVLOG(3) << "Setting fallback value for " << required_field.selector << " ("
           << element_tag << ")";
  if (element_tag == kSelectElementTag) {
    std::string re2_value;
    bool case_sensitive = false;
    ClientStatus re2_status =
        GetRe2Value(required_field, fallback_values_, /* use_contains= */ false,
                    &re2_value, &case_sensitive);
    if (!re2_status.ok() || re2_value.empty()) {
      // TODO(b/184814284): Selecting an empty value of a dropdown is somewhat
      // undefined behaviour. Set the value attribute as a best guess.
      action_delegate_->GetWebController()->SetValueAttribute(
          std::string(), *element_ptr, std::move(on_set_field_value));
      return;
    }

    SelectOptionProto::OptionComparisonAttribute option_comparison_attribute =
        required_field.proto.option_comparison_attribute();
    if (option_comparison_attribute == SelectOptionProto::NOT_SET) {
      switch (required_field.proto.select_strategy()) {
        case UNSPECIFIED_SELECT_STRATEGY:
        case LABEL_STARTS_WITH:
        case LABEL_MATCH:
          option_comparison_attribute = SelectOptionProto::LABEL;
          break;
        case VALUE_MATCH:
          option_comparison_attribute = SelectOptionProto::VALUE;
          break;
      }
    }

    action_delegate_->GetWebController()->SelectOption(
        re2_value, case_sensitive, option_comparison_attribute,
        /* strict= */ false, *element_ptr, std::move(on_set_field_value));
    return;
  }

  action_delegate_util::PerformSetFieldValue(
      action_delegate_, value, required_field.proto.fill_strategy(),
      required_field.proto.delay_in_millisecond(), *element_ptr,
      std::move(on_set_field_value));
}

void RequiredFieldsFallbackHandler::FillJsDrivenDropdown(
    const std::string& value,
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field) {
  DCHECK(required_field.proto.has_option_element_to_click());

  std::string re2_value;
  bool case_sensitive = false;
  ClientStatus re2_status =
      GetRe2Value(required_field, fallback_values_, /* use_contains= */ true,
                  &re2_value, &case_sensitive);
  if (!re2_status.ok() || re2_value.empty()) {
    // TODO(b/184814284): Selecting an empty value in a JS driven dropdown is
    // undefined behaviour. Do nothing.
    std::move(set_next_field).Run();
    return;
  }

  ClickType click_type = required_field.proto.click_type();
  if (click_type == ClickType::NOT_SET) {
    // default: TAP
    click_type = ClickType::TAP;
  }
  action_delegate_->FindElement(
      required_field.selector,
      base::BindOnce(
          &element_action_util::TakeElementAndPerform,
          base::BindOnce(&action_delegate_util::PerformClickOrTapElement,
                         action_delegate_, click_type),
          base::BindOnce(
              &RequiredFieldsFallbackHandler::OnClickOrTapFallbackElement,
              weak_ptr_factory_.GetWeakPtr(), re2_value, case_sensitive,
              required_field, std::move(set_next_field))));
}

void RequiredFieldsFallbackHandler::OnClickOrTapFallbackElement(
    const std::string& re2_value,
    bool case_sensitive,
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field,
    const ClientStatus& element_click_status) {
  if (!element_click_status.ok()) {
    FillStatusDetailsWithError(
        required_field, element_click_status.proto_status(), &client_status_);

    // Main element to click does not exist. We either continue or stop the
    // script without checking the other fields.
    if (required_field.proto.is_optional() &&
        element_click_status.proto_status() == ELEMENT_RESOLUTION_FAILED) {
      std::move(set_next_field).Run();
    } else {
      std::move(status_update_callback_)
          .Run(ErrorStatusWithDefault(client_status_));
    }
    return;
  }

  DCHECK(required_field.proto.has_option_element_to_click());
  Selector value_selector =
      Selector(required_field.proto.option_element_to_click());
  value_selector.MatchingInnerText(re2_value, case_sensitive);

  action_delegate_->ShortWaitForElementWithSlowWarning(
      value_selector,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnShortWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), value_selector,
                     required_field, std::move(set_next_field)));
}

void RequiredFieldsFallbackHandler::OnShortWaitForElement(
    const Selector& selector_to_click,
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field,
    const ClientStatus& find_element_status,
    base::TimeDelta wait_time) {
  total_wait_time_ += wait_time;
  if (!find_element_status.ok()) {
    // We're looking for the option element to click, if it cannot be found,
    // change the error status to reflect that.
    FillStatusDetailsWithError(
        required_field,
        find_element_status.proto_status() == ELEMENT_RESOLUTION_FAILED
            ? OPTION_VALUE_NOT_FOUND
            : find_element_status.proto_status(),
        &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(ErrorStatusWithDefault(client_status_));
    return;
  }

  ClickType click_type = required_field.proto.click_type();
  if (click_type == ClickType::NOT_SET) {
    // default: TAP
    click_type = ClickType::TAP;
  }
  action_delegate_->FindElement(
      selector_to_click,
      base::BindOnce(
          &element_action_util::TakeElementAndPerform,
          base::BindOnce(&action_delegate_util::PerformClickOrTapElement,
                         action_delegate_, click_type),
          base::BindOnce(
              &RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
              weak_ptr_factory_.GetWeakPtr(), required_field,
              std::move(set_next_field), /* element= */ nullptr)));
}

void RequiredFieldsFallbackHandler::OnSetFallbackFieldValue(
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field,
    std::unique_ptr<ElementFinderResult> element,
    const ClientStatus& set_field_status) {
  if (!set_field_status.ok()) {
    VLOG(1) << "Error setting value for required_field: "
            << required_field.selector << " " << set_field_status;
    FillStatusDetailsWithError(required_field, set_field_status.proto_status(),
                               &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(ErrorStatusWithDefault(client_status_));
    return;
  }

  std::move(set_next_field).Run();
}

}  // namespace autofill_assistant
