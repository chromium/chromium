// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_DISPATCH_JS_EVENT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_DISPATCH_JS_EVENT_ACTION_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
class ClientStatus;

class DispatchJsEventAction : public Action {
 public:
  explicit DispatchJsEventAction(ActionDelegate* delegate,
                                 const ActionProto& proto);
  ~DispatchJsEventAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnDispatchJsEvent(ProcessActionCallback callback,
                         const ClientStatus& status);

  base::WeakPtrFactory<DispatchJsEventAction> weak_ptr_factory_{this};

  DispatchJsEventAction(const DispatchJsEventAction&) = delete;
  DispatchJsEventAction& operator=(const DispatchJsEventAction&) = delete;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_DISPATCH_JS_EVENT_ACTION_H_
