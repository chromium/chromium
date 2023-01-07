// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element.h"

namespace autofill_assistant {
class BatchElementChecker;

// An action to ask Chrome to wait for a DOM element to process next action.
class WaitForDomAction : public Action {
 public:
  explicit WaitForDomAction(ActionDelegate* delegate, const ActionProto& proto);

  WaitForDomAction(const WaitForDomAction&) = delete;
  WaitForDomAction& operator=(const WaitForDomAction&) = delete;

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
      const std::vector<std::string>& tags,
      const base::flat_map<std::string, DomObjectFrameStack>& elements);
  void ReportActionResult(ProcessActionCallback callback,
                          const ClientStatus& status);
  void UpdateElementStore();

  base::flat_map<std::string, DomObjectFrameStack> elements_;

  base::WeakPtrFactory<WaitForDomAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
