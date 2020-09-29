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
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/user_data.h"

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

  // TODO(b/154067717): Remove PasswordValueType enum.
  // Helper enum for |FieldInput| to describe the passwords-related actions.
  enum class PasswordValueType { NOT_SET, STORED_PASSWORD };

  // A field input as extracted from the proto, but already checked for
  // validity.
  struct FieldInput {
    explicit FieldInput(std::unique_ptr<std::vector<UChar32>> keyboard_input);
    explicit FieldInput(std::string value);
    explicit FieldInput(PasswordValueType password_type);
    FieldInput(FieldInput&& other);
    ~FieldInput();

    // The keys to press if either |keycode| or |keyboard_input| is set, else
    // nullptr.
    std::unique_ptr<std::vector<UChar32>> keyboard_input = nullptr;
    // If the action is about passwords, the field describes whether the
    // password should be retrieved from storage or generated.
    PasswordValueType password_type = PasswordValueType::NOT_SET;
    // The string to input (for all other cases).
    std::string value;
  };

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnWaitForElement(const ClientStatus& element_status);

  void OnGetFieldValue(int field_index,
                       const std::string& requested_value,
                       const ClientStatus& element_status,
                       const std::string& actual_value);

  void OnSetFieldValue(int next, const ClientStatus& status);

  void OnSetFieldValueAndCheckFallback(int field_index,
                                       const std::string& requested_value,
                                       const ClientStatus& status);

  void OnGetStoredPassword(int field_index, bool success, std::string password);

  void FailAction(const ClientStatus& status, int keypress_index);
  void EndAction(const ClientStatus& status);

  Selector selector_;
  std::vector<FieldInput> field_inputs_;
  ProcessActionCallback process_action_callback_;
  base::WeakPtrFactory<SetFormFieldValueAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SetFormFieldValueAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_FORM_FIELD_VALUE_ACTION_H_
