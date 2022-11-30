// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SAVE_SUBMITTED_PASSWORD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SAVE_SUBMITTED_PASSWORD_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/user_data.h"

namespace autofill_assistant {

// Action to save the current submitted password to the password store.
class SaveSubmittedPasswordAction : public Action {
 public:
  explicit SaveSubmittedPasswordAction(ActionDelegate* delegate,
                                       const ActionProto& proto);
  ~SaveSubmittedPasswordAction() override;

  SaveSubmittedPasswordAction(const SaveSubmittedPasswordAction&) = delete;
  SaveSubmittedPasswordAction& operator=(const SaveSubmittedPasswordAction&) =
      delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  // Called with the results of a password leak check.
  void OnLeakCheckComplete(LeakDetectionStatus status, bool is_leaked);

  void EndAction(const ClientStatus& status);

  ProcessActionCallback callback_;
  base::WeakPtrFactory<SaveSubmittedPasswordAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SAVE_SUBMITTED_PASSWORD_ACTION_H_
