// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REGISTER_PASSWORD_RESET_REQUEST_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REGISTER_PASSWORD_RESET_REQUEST_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Action to notify the password change success tracker that a password reset
// has been requested.
class RegisterPasswordResetRequestAction : public Action {
 public:
  explicit RegisterPasswordResetRequestAction(ActionDelegate* delegate,
                                              const ActionProto& proto);
  ~RegisterPasswordResetRequestAction() override;

  RegisterPasswordResetRequestAction(
      const RegisterPasswordResetRequestAction&) = delete;
  RegisterPasswordResetRequestAction& operator=(
      const RegisterPasswordResetRequestAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  ProcessActionCallback callback_;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REGISTER_PASSWORD_RESET_REQUEST_ACTION_H_
