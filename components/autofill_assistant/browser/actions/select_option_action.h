// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SELECT_OPTION_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SELECT_OPTION_ACTION_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

// An action to select an option on a given element on Web.
class SelectOptionAction : public Action {
 public:
  explicit SelectOptionAction(ActionDelegate* delegate,
                              const ActionProto& proto);

  SelectOptionAction(const SelectOptionAction&) = delete;
  SelectOptionAction& operator=(const SelectOptionAction&) = delete;

  ~SelectOptionAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnWaitForElement(const Selector& selector,
                        const ClientStatus& element_status);

  void EndAction(const ClientStatus& status);

  std::string value_;
  bool case_sensitive_ = false;
  ProcessActionCallback process_action_callback_;

  base::WeakPtrFactory<SelectOptionAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SELECT_OPTION_ACTION_H_
