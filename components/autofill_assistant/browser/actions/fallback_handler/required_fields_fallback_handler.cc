// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
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

ClientStatus& UpdateClientStatusForIncomplete(ClientStatus& status) {
  if (status.ok()) {
    status.set_proto_status(AUTOFILL_INCOMPLETE);
  }
  return status;
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
    std::move(status_update_callback_).Run(client_status_);
    return;
  }

  if (!apply_fallback) {
    // Validation failed and we don't want to try the fallback.
    std::move(status_update_callback_)
        .Run(UpdateClientStatusForIncomplete(client_status_));
    return;
  }

  // If there are any fallbacks for the empty fields, set them, otherwise fail
  // immediately.
  bool has_fallbacks = false;
  bool has_empty_value = false;
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
      has_empty_value = true;
    }
  }
  if (!has_fallbacks || has_empty_value) {
    std::move(status_update_callback_)
        .Run(UpdateClientStatusForIncomplete(client_status_));
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
  base::OnceCallback<void()> set_next_field = base::BindOnce(
      &RequiredFieldsFallbackHandler::SetFallbackFieldValuesSequentially,
      weak_ptr_factory_.GetWeakPtr(), required_fields_index + 1);

  if (required_field.value_expression.empty()) {
    action_delegate_util::SetFieldValue(
        action_delegate_, required_field.selector, "",
        required_field.fill_strategy, required_field.delay_in_millisecond,
        base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), required_field,
                       std::move(set_next_field),
                       /* element= */ nullptr));
    return;
  }

  auto fallback_value = field_formatter::FormatString(
      required_field.value_expression, fallback_values_);
  DCHECK(fallback_value.has_value());

  if (required_field.fallback_click_element.has_value()) {
    ClickType click_type = required_field.click_type;
    if (click_type == ClickType::NOT_SET) {
      // default: TAP
      click_type = ClickType::TAP;
    }
    action_delegate_util::ClickOrTapElement(
        action_delegate_, required_field.selector, click_type,
        /* on_top= */ SKIP_STEP,
        base::BindOnce(
            &RequiredFieldsFallbackHandler::OnClickOrTapFallbackElement,
            weak_ptr_factory_.GetWeakPtr(), *fallback_value, required_field,
            std::move(set_next_field)));
    return;
  }

  action_delegate_->FindElement(
      required_field.selector,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnFindElement,
                     weak_ptr_factory_.GetWeakPtr(), *fallback_value,
                     required_field, std::move(set_next_field)));
}

void RequiredFieldsFallbackHandler::OnFindElement(
    const std::string& value,
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!element_status.ok()) {
    FillStatusDetailsWithError(required_field, element_status.proto_status(),
                               &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(UpdateClientStatusForIncomplete(client_status_));
    return;
  }

  const ElementFinder::Result* element_result_ptr = element_result.get();
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
    std::unique_ptr<ElementFinder::Result> element,
    const ClientStatus& element_tag_status,
    const std::string& element_tag) {
  if (!element_tag_status.ok()) {
    DVLOG(3) << "Status for element tag was "
             << element_tag_status.proto_status();
  }

  VLOG(3) << "Setting fallback value for " << required_field.selector << " ("
          << element_tag << ")";
  if (element_tag == kSelectElementTag) {
    SelectOptionProto::OptionComparisonAttribute option_comparison_attribute;
    std::string re2;
    switch (required_field.select_strategy) {
      case UNSPECIFIED_SELECT_STRATEGY:
      case LABEL_STARTS_WITH:
        // This is the legacy default.
        option_comparison_attribute = SelectOptionProto::LABEL;
        re2 = base::StrCat({"^", re2::RE2::QuoteMeta(value)});
        break;
      case LABEL_MATCH:
        option_comparison_attribute = SelectOptionProto::LABEL;
        re2 = base::StrCat({"^", re2::RE2::QuoteMeta(value), "$"});
        break;
      case VALUE_MATCH:
        option_comparison_attribute = SelectOptionProto::VALUE;
        re2 = base::StrCat({"^", re2::RE2::QuoteMeta(value), "$"});
        break;
    }

    const ElementFinder::Result* element_ptr = element.get();
    action_delegate_->GetWebController()->SelectOption(
        re2, /* case_sensitive= */ false, option_comparison_attribute,
        *element_ptr,
        base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), required_field,
                       std::move(set_next_field), std::move(element)));
    return;
  }

  const ElementFinder::Result* element_ptr = element.get();
  action_delegate_util::PerformSetFieldValue(
      action_delegate_, value, required_field.fill_strategy,
      required_field.delay_in_millisecond, *element_ptr,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_field,
                     std::move(set_next_field), std::move(element)));
}

void RequiredFieldsFallbackHandler::OnClickOrTapFallbackElement(
    const std::string& value,
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field,
    const ClientStatus& element_click_status) {
  if (!element_click_status.ok()) {
    FillStatusDetailsWithError(
        required_field, element_click_status.proto_status(), &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(UpdateClientStatusForIncomplete(client_status_));
    return;
  }

  DCHECK(required_field.fallback_click_element.has_value());
  Selector value_selector = required_field.fallback_click_element.value();
  value_selector.MatchingInnerText(re2::RE2::QuoteMeta(value));

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
    FillStatusDetailsWithError(
        required_field, find_element_status.proto_status(), &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(UpdateClientStatusForIncomplete(client_status_));
    return;
  }

  ClickType click_type = required_field.click_type;
  if (click_type == ClickType::NOT_SET) {
    // default: TAP
    click_type = ClickType::TAP;
  }
  action_delegate_util::ClickOrTapElement(
      action_delegate_, selector_to_click, click_type, /* on_top= */ SKIP_STEP,
      base::BindOnce(&RequiredFieldsFallbackHandler::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), required_field,
                     std::move(set_next_field),
                     /* element= */ nullptr));
}

void RequiredFieldsFallbackHandler::OnSetFallbackFieldValue(
    const RequiredField& required_field,
    base::OnceCallback<void()> set_next_field,
    std::unique_ptr<ElementFinder::Result> element,
    const ClientStatus& set_field_status) {
  if (!set_field_status.ok()) {
    VLOG(1) << "Error setting value for required_field: "
            << required_field.selector << " " << set_field_status;
    FillStatusDetailsWithError(required_field, set_field_status.proto_status(),
                               &client_status_);

    // Fallback failed: we stop the script without checking the other fields.
    std::move(status_update_callback_)
        .Run(UpdateClientStatusForIncomplete(client_status_));
    return;
  }

  std::move(set_next_field).Run();
}

}  // namespace autofill_assistant
