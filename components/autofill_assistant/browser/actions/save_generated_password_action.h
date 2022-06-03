// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SAVE_GENERATED_PASSWORD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SAVE_GENERATED_PASSWORD_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/user_data.h"

namespace autofill_assistant {

// Action to save a generated password after successful form submission.
class SaveGeneratedPasswordAction : public Action {
 public:
  explicit SaveGeneratedPasswordAction(ActionDelegate* delegate,
                                       const ActionProto& proto);
  ~SaveGeneratedPasswordAction() override;

  SaveGeneratedPasswordAction(const SaveGeneratedPasswordAction&) = delete;
  SaveGeneratedPasswordAction& operator=(const SaveGeneratedPasswordAction&) =
      delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  ProcessActionCallback callback_;
  base::WeakPtrFactory<SaveGeneratedPasswordAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SAVE_GENERATED_PASSWORD_ACTION_H_
