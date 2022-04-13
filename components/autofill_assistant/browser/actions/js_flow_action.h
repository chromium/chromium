// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_JS_FLOW_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_JS_FLOW_ACTION_H_

#include <memory>
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/js_flow_executor.h"

namespace autofill_assistant {

// An action that runs a javascript flow inside an isolated JS context.
class JsFlowAction : public Action, public JsFlowExecutor::Delegate {
 public:
  explicit JsFlowAction(ActionDelegate* delegate, const ActionProto& proto);
  // Can be used to inject a different flow executor in tests.
  explicit JsFlowAction(ActionDelegate* delegate,
                        const ActionProto& proto,
                        std::unique_ptr<JsFlowExecutor> js_flow_executor);
  ~JsFlowAction() override;
  JsFlowAction(const JsFlowAction&) = delete;
  JsFlowAction& operator=(const JsFlowAction&) = delete;

  // Override Action
  ActionData& GetActionData() override;

  // Override JsFlowExecutor::Delegate
  void RunNativeAction(
      int action_id,
      const std::string& action,
      base::OnceCallback<void(const ClientStatus& result_status,
                              std::unique_ptr<base::Value> result_value)>
          finished_callback) override;

 private:
  friend class JsFlowActionTest;

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnNativeActionFinished(
      base::OnceCallback<void(const ClientStatus& result_status,
                              std::unique_ptr<base::Value> result_value)>
          finished_callback,
      std::unique_ptr<ProcessedActionProto> processed_action);
  void OnFlowFinished(ProcessActionCallback callback,
                      const ClientStatus& status,
                      std::unique_ptr<base::Value> return_value);

  // Only set while executing a native action.
  std::unique_ptr<Action> current_native_action_;

  std::unique_ptr<JsFlowExecutor> js_flow_executor_;
  base::WeakPtrFactory<JsFlowAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_JS_FLOW_ACTION_H_
