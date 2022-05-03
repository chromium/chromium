// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXTERNAL_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXTERNAL_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "components/autofill_assistant/browser/public/external_script_controller.h"
#include "components/autofill_assistant/browser/wait_for_dom_observer.h"

namespace autofill_assistant {

class ExternalAction : public Action {
 public:
  explicit ExternalAction(ActionDelegate* delegate, const ActionProto& proto);

  ExternalAction(const ExternalAction&) = delete;
  ExternalAction& operator=(const ExternalAction&) = delete;

  ~ExternalAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;
  void OnExternalActionFinished(ExternalActionDelegate::ActionResult success);
  void SendActionInfo();
  void EndAction(const ClientStatus& status);

  ProcessActionCallback callback_;

  base::WeakPtrFactory<ExternalAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXTERNAL_ACTION_H_
