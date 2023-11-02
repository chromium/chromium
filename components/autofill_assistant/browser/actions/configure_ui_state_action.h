// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CONFIGURE_UI_STATE_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CONFIGURE_UI_STATE_ACTION_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Action to configure the ui.
class ConfigureUiStateAction : public Action {
 public:
  explicit ConfigureUiStateAction(ActionDelegate* delegate,
                                  const ActionProto& proto);

  ConfigureUiStateAction(const ConfigureUiStateAction&) = delete;
  ConfigureUiStateAction& operator=(const ConfigureUiStateAction&) = delete;

  ~ConfigureUiStateAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  base::WeakPtrFactory<ConfigureUiStateAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CONFIGURE_UI_STATE_ACTION_H_
