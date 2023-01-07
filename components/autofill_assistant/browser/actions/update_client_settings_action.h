// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_UPDATE_CLIENT_SETTINGS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_UPDATE_CLIENT_SETTINGS_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to update Client Settings.
class UpdateClientSettingsAction : public Action {
 public:
  explicit UpdateClientSettingsAction(ActionDelegate* delegate,
                                      const ActionProto& proto);
  ~UpdateClientSettingsAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  ProcessActionCallback process_action_callback_;
  base::WeakPtrFactory<UpdateClientSettingsAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_UPDATE_CLIENT_SETTINGS_ACTION_H_
