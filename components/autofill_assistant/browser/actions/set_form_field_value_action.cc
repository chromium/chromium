// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

SetFormFieldValueAction::FieldInput::FieldInput(
    std::unique_ptr<std::vector<UChar32>> _keyboard_input)
    : keyboard_input(std::move(_keyboard_input)) {}

SetFormFieldValueAction::FieldInput::FieldInput(std::string _value)
    : value(_value) {}

SetFormFieldValueAction::FieldInput::FieldInput(bool _use_password)
    : use_password(_use_password) {}

SetFormFieldValueAction::FieldInput::FieldInput(FieldInput&& other) = default;

SetFormFieldValueAction::FieldInput::~FieldInput() {}

SetFormFieldValueAction::SetFormFieldValueAction(ActionDelegate* delegate,
                                                 const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_set_form_value());
  DCHECK_GT(proto_.set_form_value().element().selectors_size(), 0);
  DCHECK_GT(proto_.set_form_value().value_size(), 0);
}

SetFormFieldValueAction::~SetFormFieldValueAction() {}

void SetFormFieldValueAction::InternalProcessAction(
    ProcessActionCallback callback) {
  process_action_callback_ = std::move(callback);
  selector_ = Selector(proto_.set_form_value().element()).MustBeVisible();
  if (selector_.empty()) {
    DVLOG(1) << __func__ << ": empty selector";
    EndAction(ClientStatus(INVALID_SELECTOR));
    return;
  }

  // Check proto fields.
  for (const auto& keypress : proto_.set_form_value().value()) {
    switch (keypress.keypress_case()) {
      case SetFormFieldValueProto_KeyPress::kKeycode:
        // DEPRECATED: the field `keycode' used to contain a single character to
        // input as text. Since there is no easy way to convert keycodes to
        // text, this field is now deprecated and only works for US-ASCII
        // characters. You should use the `keyboard_input' field instead.
        if (keypress.keycode() >= 128) {
          DVLOG(1) << "SetFormFieldValueAction: field `keycode' is deprecated "
                   << "and only supports US-ASCII values (encountered "
                   << keypress.keycode() << "). Use field `key' instead.";
          EndAction(ClientStatus(INVALID_ACTION));
          return;
        }
        field_inputs_.emplace_back(
            /* keyboard_input = */ std::make_unique<std::vector<UChar32>>(
                1, keypress.keycode()));
        break;
      case SetFormFieldValueProto_KeyPress::kKeyboardInput:
        if (keypress.keyboard_input().empty()) {
          DVLOG(1) << "SetFormFieldValueAction: field 'keyboard_input' must be "
                      "non-empty if set.";
          EndAction(ClientStatus(INVALID_ACTION));
          return;
        }
        field_inputs_.emplace_back(
            /* keyboard_input = */ std::make_unique<std::vector<UChar32>>(
                UTF8ToUnicode(keypress.keyboard_input())));
        break;
      case SetFormFieldValueProto_KeyPress::kUseUsername:
        FALLTHROUGH;
      case SetFormFieldValueProto_KeyPress::kUsePassword:
        // Login information must have been stored by a previous action.
        if (!delegate_->GetClientMemory()->has_selected_login()) {
          DVLOG(1) << "SetFormFieldValueAction: requested login details not "
                      "available in client memory.";
          EndAction(ClientStatus(PRECONDITION_FAILED));
          return;
        }
        if (keypress.keypress_case() ==
            SetFormFieldValueProto_KeyPress::kUseUsername) {
          field_inputs_.emplace_back(/* value = */ delegate_->GetClientMemory()
                                         ->selected_login()
                                         ->username);
        } else {
          field_inputs_.emplace_back(/* use_password = */ true);
        }
        break;
      case SetFormFieldValueProto_KeyPress::kText:
        // Currently no check required.
        field_inputs_.emplace_back(/* value = */ keypress.text());
        break;
      case SetFormFieldValueProto_KeyPress::kClientMemoryKey:
        if (keypress.client_memory_key().empty()) {
          DVLOG(1) << "SetFormFieldValueAction: empty |client_memory_key|";
          EndAction(ClientStatus(INVALID_ACTION));
          return;
        }
        if (!delegate_->GetClientMemory()->has_additional_value(
                keypress.client_memory_key())) {
          DVLOG(1) << "SetFormFieldValueAction: requested key '"
                   << keypress.client_memory_key()
                   << "' not available in client memory";
          EndAction(ClientStatus(PRECONDITION_FAILED));
          return;
        }
        field_inputs_.emplace_back(
            /* value = */ *delegate_->GetClientMemory()->additional_value(
                keypress.client_memory_key()));
        break;
      default:
        DVLOG(1) << "Unrecognized field for SetFormFieldValueProto_KeyPress";
        EndAction(ClientStatus(INVALID_ACTION));
        return;
    }
  }

  delegate_->ShortWaitForElement(
      selector_, base::BindOnce(&SetFormFieldValueAction::OnWaitForElement,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SetFormFieldValueAction::OnWaitForElement(
    const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(ClientStatus(element_status.proto_status()));
    return;
  }
  // Start with first value, then call OnSetFieldValue() recursively until done.
  OnSetFieldValue(/* next = */ 0, OkClientStatus());
}

void SetFormFieldValueAction::OnSetFieldValue(int next,
                                              const ClientStatus& status) {
  // If something went wrong or we are out of values: finish
  if (!status.ok() || next >= proto_.set_form_value().value_size()) {
    EndAction(status);
    return;
  }

  bool simulate_key_presses = proto_.set_form_value().simulate_key_presses();
  int delay_in_millisecond = proto_.set_form_value().delay_in_millisecond();
  auto next_field_callback = base::BindOnce(
      &SetFormFieldValueAction::OnSetFieldValue, weak_ptr_factory_.GetWeakPtr(),
      /* next = */ next + 1);
  for (const auto& field_input : field_inputs_) {
    if (field_input.keyboard_input) {
      delegate_->SendKeyboardInput(selector_, *field_input.keyboard_input,
                                   delay_in_millisecond,
                                   std::move(next_field_callback));
    } else if (field_input.use_password) {
      delegate_->GetWebsiteLoginFetcher()->GetPasswordForLogin(
          *delegate_->GetClientMemory()->selected_login(),
          base::BindOnce(&SetFormFieldValueAction::OnGetPassword,
                         weak_ptr_factory_.GetWeakPtr(),
                         /* field_index = */ next));
    } else {
      if (simulate_key_presses || field_input.value.empty()) {
        delegate_->SetFieldValue(selector_, field_input.value,
                                 simulate_key_presses, delay_in_millisecond,
                                 std::move(next_field_callback));
      } else {
        delegate_->SetFieldValue(
            selector_, field_input.value, simulate_key_presses,
            delay_in_millisecond,
            base::BindOnce(
                &SetFormFieldValueAction::OnSetFieldValueAndCheckFallback,
                weak_ptr_factory_.GetWeakPtr(),
                /* field_index = */ next,
                /* requested_value = */ field_input.value));
      }
    }
  }
}

void SetFormFieldValueAction::OnSetFieldValueAndCheckFallback(
    int field_index,
    const std::string& requested_value,
    const ClientStatus& status) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }
  delegate_->GetFieldValue(
      selector_, base::BindOnce(&SetFormFieldValueAction::OnGetFieldValue,
                                weak_ptr_factory_.GetWeakPtr(), field_index,
                                requested_value));
}

void SetFormFieldValueAction::OnGetFieldValue(
    int field_index,
    const std::string& requested_value,
    const ClientStatus& element_status,
    const std::string& actual_value) {
  // Move to next value if |GetFieldValue| failed.
  if (!element_status.ok()) {
    OnSetFieldValue(field_index + 1, OkClientStatus());
    return;
  }

  // If value is still empty while it is not supposed to be, trigger keyboard
  // simulation fallback.
  if (!requested_value.empty() && actual_value.empty()) {
    // Report a key press simulation fallback has happened.
    auto result = SetFormFieldValueProto::Result();
    result.set_fallback_to_simulate_key_presses(true);
    *processed_action_proto_->mutable_set_form_field_value_result() = result;

    // Run |SetFieldValue| with keyboard simulation on and move on to next value
    // afterwards.
    delegate_->SetFieldValue(
        selector_, requested_value, /*simulate_key_presses = */ true,
        proto_.set_form_value().delay_in_millisecond(),
        base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                       weak_ptr_factory_.GetWeakPtr(),
                       /* next = */ field_index + 1));
    return;
  }

  // Move to next value in all other cases.
  OnSetFieldValue(field_index + 1, OkClientStatus());
}

void SetFormFieldValueAction::OnGetPassword(int field_index,
                                            bool success,
                                            std::string password) {
  if (!success) {
    EndAction(ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE));
    return;
  }
  bool simulate_key_presses = proto_.set_form_value().simulate_key_presses();
  int delay_in_millisecond = proto_.set_form_value().delay_in_millisecond();
  if (simulate_key_presses) {
    delegate_->SetFieldValue(
        selector_, password, simulate_key_presses, delay_in_millisecond,
        base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                       weak_ptr_factory_.GetWeakPtr(),
                       /* next = */ field_index + 1));
  } else {
    delegate_->SetFieldValue(
        selector_, password, simulate_key_presses, delay_in_millisecond,
        base::BindOnce(
            &SetFormFieldValueAction::OnSetFieldValueAndCheckFallback,
            weak_ptr_factory_.GetWeakPtr(),
            /* next = */ field_index, /* requested_value = */ password));
  }
}

void SetFormFieldValueAction::EndAction(const ClientStatus& status) {
  // Clear immediately, to prevent sensitive information from staying in memory.
  field_inputs_.clear();
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
