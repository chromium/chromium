// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_RESET_PENDING_CREDENTIALS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_RESET_PENDING_CREDENTIALS_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Action to reset any existing pending credentials on Chrome Password Manager.
class ResetPendingCredentialsAction : public Action {
 public:
  explicit ResetPendingCredentialsAction(ActionDelegate* delegate,
                                         const ActionProto& proto);
  ~ResetPendingCredentialsAction() override;

  ResetPendingCredentialsAction(const ResetPendingCredentialsAction&) = delete;
  ResetPendingCredentialsAction& operator=(
      const ResetPendingCredentialsAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  ProcessActionCallback callback_;
  base::WeakPtrFactory<ResetPendingCredentialsAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_RESET_PENDING_CREDENTIALS_ACTION_H_
