// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PRESAVE_GENERATED_PASSWORD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PRESAVE_GENERATED_PASSWORD_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/user_data.h"

namespace autofill_assistant {

// Action to presave a generated password. It is only possible if password
// has been already generated using |GeneratePasswordAction|.
// Presaving stores a generated password with empty username for the cases
// when Chrome misses or misclassifies a successful submission. Thus, even if
// a site saves/updates the password and Chrome doesn't, the generated
// password will be in the password store.
// A generated password is presaved after form filling.
class PresaveGeneratedPasswordAction : public Action {
 public:
  explicit PresaveGeneratedPasswordAction(ActionDelegate* delegate,
                                          const ActionProto& proto);
  ~PresaveGeneratedPasswordAction() override;

  PresaveGeneratedPasswordAction(const PresaveGeneratedPasswordAction&) =
      delete;
  PresaveGeneratedPasswordAction& operator=(
      const PresaveGeneratedPasswordAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  Selector selector_;
  ProcessActionCallback callback_;
  base::WeakPtrFactory<PresaveGeneratedPasswordAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PRESAVE_GENERATED_PASSWORD_ACTION_H_
