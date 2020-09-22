// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {
namespace {

const char kSelectElementTag[] = "SELECT";

AutofillErrorInfoProto::AutofillFieldError* AddAutofillError(
    const RequiredField& required_field,
    ClientStatus* client_status) {
  client_status->set_proto_status(
      ProcessedActionStatusProto::AUTOFILL_INCOMPLETE);
  auto* field_error = client_status->mutable_details()
                          ->mutable_autofill_error_info()
                          ->add_autofill_field_error();
  *field_error->mutable_field() = required_field.selector.proto;
  field_error->set_value_expression(required_field.value_expression);
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

}  // namespace

RequiredFieldsFallbackHandler::~RequiredFieldsFallbackHandler() = default;

RequiredFieldsFallbackHandler::RequiredFieldsFallbackHandler(
    const std::vector<RequiredField>& required_fields,
    const std::map<std::string, std::string>& fallback_values,
    ActionDelegate* delegate)
    : required_fields_(required_fields),
      fallback_values_(fallback_values),
      action_delegate_(delegate) {}

void RequiredFieldsFallbackHandler::CheckAndFallbackRequiredFields(
    const ClientStatus& initial_autofill_status,
    base::OnceCallback<void(const ClientStatus&,
                            const base::Optional<ClientStatus>&)>
        status_update_callback) {
  client_status_ = initial_autofill_status;
  status_update_callback_ = std::move(status_update_callback);

  if (required_fields_.empty()) {
    if (!initial_autofill_status.ok()) {
      VLOG(1) << __func__ << " Autofill failed and no fallback provided "
              << initial_autofill_status.proto_status();
    }

    std::move(status_update_callback_)
        .Run(initial_autofill_status, base::nullopt);
    return;
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
    if (required_fields_[i].forced && apply_fallback) {
      continue;
    }

    // We cannot check the value of elements with custom fallback clicks. Those
    // elements are JS driven structures that in most cases lack a "value"
    // attribute. We define a successful click on the element as successfully
    // filling the form field.
    if (required_fields_[i].fallback_click_element.has_value()) {
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
        if (required_field.value_expression.empty()) {
          VLOG(1) << "Field was filled after attempting to clear it: "
                  << required_field.selector;
          FillStatusDetailsWithNotClearedField(required_field, &client_status_);
        } else {
          VLOG(1) << "Field was empty after applying fallback: "
                  << required_field.selector;
          FillStatusDetailsWithEmptyField(required_field, &client_status_);
        }
      }
      break;
    }
  }

  if (!should_fallback) {
    std::move(status_update_callback_)
        .Run(ClientStatus(ACTION_APPLIED), client_status_);
    return;
  }

  if (!apply_fallback) {
    // Validation failed and we don't want to try the fallback.
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  // If there are any fallbacks for the empty fields, set them, otherwise fail
  // immediately.
  bool has_fallbacks = false;
  for (const RequiredField& required_field : required_fields_) {
    if (!required_field.ShouldFallback(/* apply_fallback= */ true)) {
      continue;
    }

    if (required_field.value_expression.empty()) {
      has_fallbacks = true;
    } else if (field_formatter::FormatString(required_field.value_expression,
                                             fallback_values_)
                   .has_value()) {
      has_fallbacks = true;
    } else {
      VLOG(3) << "Field has no fallback data: " << required_field.selector
              << " " << required_field.value_expression;
      FillStatusDetailsWithMissingFallbackData(required_field, &client_status_);
    }
  }
  if (!has_fallbacks) {
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
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

  if (required_field.value_expression.empty()) {
    ActionDelegateUtil::SetFieldValue(
        action_delegate_, required_field.selector, "",
        required_field.fill_strategy, required_field.delay_in_millisecond,
        base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                       /* element= */ nullptr));
    return;
  }

  auto fallback_value = field_formatter::FormatString(
      required_field.value_expression, fallback_values_);
  if (!fallback_value.has_value()) {
    VLOG(3) << "No fallback for " << required_field.selector;
    // If there is no fallback value, we skip this failed field.
    SetFallbackFieldValuesSequentially(++required_fields_index);
    return;
  }

  if (required_field.fallback_click_element.has_value()) {
    ClickType click_type = required_field.click_type;
    if (click_type == ClickType::NOT_SET) {
      // default: TAP
      click_type = ClickType::TAP;
    }
    ActionDelegateUtil::ClickOrTapElement(
        action_delegate_, required_field.selector, click_type,
        base::BindOnce(
            &RequiredFieldsFallbackHandler::OnClickOrTapFallbackElement,
            weak_ptr_factory_.GetWeakPtr(), fallback_value.value(),
            required_fields_index));
    return;
  }

  action_delegate_->FindElement(
      required_field.selector,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnFindElement,
                     weak_ptr_factory_.GetWeakPtr(), fallback_value.value(),
                     required_fields_index));
}

void RequiredFieldsFallbackHandler::OnFindElement(
    const std::string& value,
    size_t required_fields_index,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!element_status.ok()) {
    FillStatusDetailsWithError(required_fields_[required_fields_index],
                               element_status.proto_status(), &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  action_delegate_->GetElementTag(
      *element_result,
      base::BindOnce(
          &RequiredFieldsFallbackHandler::OnGetFallbackFieldElementTag,
          weak_ptr_factory_.GetWeakPtr(), value, required_fields_index,
          std::move(element_result)));
}

void RequiredFieldsFallbackHandler::OnGetFallbackFieldElementTag(
    const std::string& value,
    size_t required_fields_index,
    std::unique_ptr<ElementFinder::Result> element,
    const ClientStatus& element_tag_status,
    const std::string& element_tag) {
  if (!element_tag_status.ok()) {
    DVLOG(3) << "Status for element tag was "
             << element_tag_status.proto_status();
  }

  const RequiredField& required_field = required_fields_[required_fields_index];
  VLOG(3) << "Setting fallback value for " << required_field.selector << " ("
          << element_tag << ")";
  if (element_tag == kSelectElementTag) {
    DropdownSelectStrategy select_strategy;
    if (required_field.select_strategy != UNSPECIFIED_SELECT_STRATEGY) {
      select_strategy = required_field.select_strategy;
    } else {
      // This is the legacy default.
      select_strategy = LABEL_STARTS_WITH;
    }

    action_delegate_->SelectOption(
        value, select_strategy, *element,
        base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                       std::move(element)));
    return;
  }

  action_delegate_->SetFieldValue(
      value, required_field.fill_strategy, required_field.delay_in_millisecond,
      *element,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                     std::move(element)));
}

void RequiredFieldsFallbackHandler::OnClickOrTapFallbackElement(
    const std::string& value,
    size_t required_fields_index,
    const ClientStatus& element_click_status) {
  const RequiredField& required_field = required_fields_[required_fields_index];
  if (!element_click_status.ok()) {
    FillStatusDetailsWithError(
        required_field, element_click_status.proto_status(), &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  DCHECK(required_field.fallback_click_element.has_value());
  Selector value_selector = required_field.fallback_click_element.value();
  value_selector.MatchingInnerText(value).MustBeVisible();

  action_delegate_->ShortWaitForElement(
      value_selector,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnShortWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), value_selector,
                     required_fields_index));
}

void RequiredFieldsFallbackHandler::OnShortWaitForElement(
    const Selector& selector_to_click,
    size_t required_fields_index,

    const ClientStatus& find_element_status) {
  const RequiredField& required_field = required_fields_[required_fields_index];
  if (!find_element_status.ok()) {
    FillStatusDetailsWithError(
        required_field, find_element_status.proto_status(), &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  ClickType click_type = required_field.click_type;
  if (click_type == ClickType::NOT_SET) {
    // default: TAP
    click_type = ClickType::TAP;
  }
  ActionDelegateUtil::ClickOrTapElement(
      action_delegate_, selector_to_click, click_type,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_fields_index,
                     /* element= */ nullptr));
}

void RequiredFieldsFallbackHandler::OnSetFallbackFieldValue(
    size_t required_fields_index,
    std::unique_ptr<ElementFinder::Result> element,
    const ClientStatus& set_field_status) {
  if (!set_field_status.ok()) {
    VLOG(1) << "Error setting value for required_field: "
            << required_fields_[required_fields_index].selector << " "
            << set_field_status;
    FillStatusDetailsWithError(required_fields_[required_fields_index],
                               set_field_status.proto_status(),
                               &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(ClientStatus(AUTOFILL_INCOMPLETE), client_status_);
    return;
  }

  SetFallbackFieldValuesSequentially(++required_fields_index);
}

}  // namespace autofill_assistant
