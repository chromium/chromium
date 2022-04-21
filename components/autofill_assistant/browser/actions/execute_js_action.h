// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXECUTE_JS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXECUTE_JS_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"

namespace autofill_assistant {

// An action that runs the JS snippet on a single previously stored element.
// No additional parameters are provided when running the snippet.
class ExecuteJsAction : public Action {
 public:
  ExecuteJsAction(ActionDelegate* delegate, const ActionProto& proto);
  ~ExecuteJsAction() override;

  ExecuteJsAction(const ExecuteJsAction&) = delete;
  ExecuteJsAction& operator=(const ExecuteJsAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  ElementFinderResult element_;
  ProcessActionCallback callback_;
  base::OneShotTimer timer_;

  base::WeakPtrFactory<ExecuteJsAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_EXECUTE_JS_ACTION_H_
