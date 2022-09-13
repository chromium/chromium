// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EDIT_PASSWORD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EDIT_PASSWORD_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Action to edit password. It is only possible if Userdata contains selected
// login.
class EditPasswordAction : public Action {
 public:
  explicit EditPasswordAction(ActionDelegate* delegate,
                              const ActionProto& proto);
  ~EditPasswordAction() override;

  EditPasswordAction(const EditPasswordAction&) = delete;
  EditPasswordAction& operator=(const EditPasswordAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnPasswordEdited(bool success);
  void EndAction(const ClientStatus& status);

  ProcessActionCallback callback_;
  base::WeakPtrFactory<EditPasswordAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EDIT_PASSWORD_ACTION_H_
