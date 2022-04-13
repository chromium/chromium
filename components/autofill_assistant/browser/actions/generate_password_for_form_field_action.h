// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GENERATE_PASSWORD_FOR_FORM_FIELD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GENERATE_PASSWORD_FOR_FORM_FIELD_ACTION_H_

#include <memory>
#include <string>
#include <utility>
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/user_data.h"

namespace autofill {
struct FormData;
struct FormFieldData;
}  // namespace autofill

namespace autofill_assistant {

// Action to generate a password for a form field. Sets up the necessary state
// for subsequent calls to SetFormFieldValueAction.
class GeneratePasswordForFormFieldAction : public Action {
 public:
  explicit GeneratePasswordForFormFieldAction(ActionDelegate* delegate,
                                              const ActionProto& proto);
  ~GeneratePasswordForFormFieldAction() override;

  GeneratePasswordForFormFieldAction(
      const GeneratePasswordForFormFieldAction&) = delete;
  GeneratePasswordForFormFieldAction& operator=(
      const GeneratePasswordForFormFieldAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  void OnGetFormAndFieldDataForGeneration(
      const std::string& memory_key,
      const ClientStatus& status,
      const autofill::FormData& form_data,
      const autofill::FormFieldData& field_data);

  void StoreGeneratedPasswordToUserData(const std::string& memory_key,
                                        const std::string& generated_password,
                                        const autofill::FormData& form_data,
                                        UserData* user_data,
                                        UserDataFieldChange* field_change);

  Selector selector_;
  ProcessActionCallback callback_;
  base::WeakPtrFactory<GeneratePasswordForFormFieldAction> weak_ptr_factory_{
      this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_GENERATE_PASSWORD_FOR_FORM_FIELD_ACTION_H_
