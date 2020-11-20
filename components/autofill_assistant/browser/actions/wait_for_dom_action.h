// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/element_precondition.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element.h"

namespace autofill_assistant {
class BatchElementChecker;

// An action to ask Chrome to wait for a DOM element to process next action.
class WaitForDomAction : public Action {
 public:
  explicit WaitForDomAction(ActionDelegate* delegate, const ActionProto& proto);
  ~WaitForDomAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  // Check all elements using the given BatchElementChecker and reports the
  // result to |callback|.
  void CheckElements(BatchElementChecker* checker,
                     base::OnceCallback<void(const ClientStatus&)> callback);
  void OnWaitConditionDone(
      base::OnceCallback<void(const ClientStatus&)> callback,
      const ClientStatus& status,
      const std::vector<std::string>& payloads,
      const base::flat_map<std::string, DomObjectFrameStack>& elements);
  void ReportActionResult(ProcessActionCallback callback,
                          const ClientStatus& status);
  void UpdateElementStore();

  std::unique_ptr<ElementPrecondition> wait_condition_;
  base::flat_map<std::string, DomObjectFrameStack> elements_;

  base::WeakPtrFactory<WaitForDomAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WaitForDomAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
