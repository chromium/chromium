// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_FORM_FIELD_VALUE_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_FORM_FIELD_VALUE_ACTION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {

// An action to set the value of a form input element.
class SetFormFieldValueAction : public Action {
 public:
  explicit SetFormFieldValueAction(ActionDelegate* delegate,
                                   const ActionProto& proto);
  ~SetFormFieldValueAction() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SetFormFieldValueActionTest,
                           PasswordIsClearedFromMemory);

  // A field input as extracted from the proto, but already checked for
  // validity.
  struct FieldInput {
    explicit FieldInput(std::unique_ptr<std::vector<UChar32>> keyboard_input);
    explicit FieldInput(std::string value);
    explicit FieldInput(PasswordManagerValue password_manager_value);
    FieldInput(FieldInput&& other);
    ~FieldInput();

    // The keys to press if either |keycode| or |keyboard_input| is set, else
    // nullptr.
    std::unique_ptr<std::vector<UChar32>> keyboard_input;
    // If the action is about passwords, the field describes whether to use
    // password or username.
    PasswordManagerValue password_manager_value;
    // The string to input (for all other cases).
    std::string value;
  };

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnWaitForElement(const ClientStatus& element_status);
  void OnFindElement(const ClientStatus& element_status,
                     std::unique_ptr<ElementFinder::Result> element_result);
  void SetFieldValueSequentially(int field_index, const ClientStatus& status);
  void OnGetPasswordManagerValue(
      base::OnceCallback<void(const ClientStatus&)> next_field_callback,
      const ClientStatus& status,
      const std::string& value);
  void SetFieldValueAndCheckFallback(
      const std::string& value,
      base::OnceCallback<void(const ClientStatus&)> next_field_callback);
  void OnSetFieldValueAndCheckFallback(
      base::OnceCallback<void(const ClientStatus&)> next_field_callback,
      const std::string& requested_value,
      const ClientStatus& status);
  void OnGetFieldValue(
      base::OnceCallback<void(const ClientStatus&)> next_field_callback,
      const std::string& requested_value,
      const ClientStatus& element_status,
      const std::string& actual_value);

  void FailAction(const ClientStatus& status, int keypress_index);
  void EndAction(const ClientStatus& status);

  Selector selector_;
  std::unique_ptr<ElementFinder::Result> element_;
  std::vector<FieldInput> field_inputs_;
  ProcessActionCallback process_action_callback_;
  base::WeakPtrFactory<SetFormFieldValueAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SetFormFieldValueAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_FORM_FIELD_VALUE_ACTION_H_
