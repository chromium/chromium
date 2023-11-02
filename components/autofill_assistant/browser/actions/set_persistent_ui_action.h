// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_PERSISTENT_UI_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_PERSISTENT_UI_ACTION_H_

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Action to show generic UI in the sheet until dismissed.
class SetPersistentUiAction : public Action {
 public:
  explicit SetPersistentUiAction(ActionDelegate* delegate,
                                 const ActionProto& proto);
  ~SetPersistentUiAction() override;

  SetPersistentUiAction(const SetPersistentUiAction&) = delete;
  SetPersistentUiAction& operator=(const SetPersistentUiAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  ProcessActionCallback callback_;
  base::WeakPtrFactory<SetPersistentUiAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_PERSISTENT_UI_ACTION_H_
