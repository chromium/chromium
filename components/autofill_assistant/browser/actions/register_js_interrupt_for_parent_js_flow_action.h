// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REGISTER_JS_INTERRUPT_FOR_PARENT_JS_FLOW_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REGISTER_JS_INTERRUPT_FOR_PARENT_JS_FLOW_ACTION_H_

#include <memory>
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Convenience wrapper around RegisterSelfContainedInterruptScriptsAction,
// specifically for use in JS flows. Will internally configure and run a nested
// RegisterSelfContainedInterruptScriptsAction using this action's parent's JS
// flow.
class RegisterJsInterruptForParentJsFlowAction : public Action {
 public:
  explicit RegisterJsInterruptForParentJsFlowAction(ActionDelegate* delegate,
                                                    const ActionProto& proto);
  ~RegisterJsInterruptForParentJsFlowAction() override;

  RegisterJsInterruptForParentJsFlowAction(
      const RegisterJsInterruptForParentJsFlowAction&) = delete;
  RegisterJsInterruptForParentJsFlowAction& operator=(
      const RegisterJsInterruptForParentJsFlowAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnInterruptRegistered(
      std::unique_ptr<ProcessedActionProto> processed_action);
  void EndAction(const ClientStatus& status);

  // Only set while executing a nested action.
  std::unique_ptr<Action> current_nested_action_;

  ProcessActionCallback callback_;
  base::WeakPtrFactory<RegisterJsInterruptForParentJsFlowAction>
      weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REGISTER_JS_INTERRUPT_FOR_PARENT_JS_FLOW_ACTION_H_
