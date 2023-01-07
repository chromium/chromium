// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CLEAR_PERSISTENT_UI_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CLEAR_PERSISTENT_UI_ACTION_H_

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Action to show generic UI in the sheet until dismissed.
class ClearPersistentUiAction : public Action {
 public:
  explicit ClearPersistentUiAction(ActionDelegate* delegate,
                                   const ActionProto& proto);
  ~ClearPersistentUiAction() override;

  ClearPersistentUiAction(const ClearPersistentUiAction&) = delete;
  ClearPersistentUiAction& operator=(const ClearPersistentUiAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CLEAR_PERSISTENT_UI_ACTION_H_
