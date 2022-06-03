// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_DELETE_PASSWORD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_DELETE_PASSWORD_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Action to delete password. It is only possible if Userdata contains selected
// login.
class DeletePasswordAction : public Action {
 public:
  explicit DeletePasswordAction(ActionDelegate* delegate,
                                const ActionProto& proto);
  ~DeletePasswordAction() override;

  DeletePasswordAction(const DeletePasswordAction&) = delete;
  DeletePasswordAction& operator=(const DeletePasswordAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnPasswordDeleted(bool success);
  void EndAction(const ClientStatus& status);

  ProcessActionCallback callback_;
  base::WeakPtrFactory<DeletePasswordAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_DELETE_PASSWORD_ACTION_H_
