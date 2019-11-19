// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
class BatchElementChecker;

// An action to ask Chrome to wait for a DOM element to process next action.
class WaitForDomAction : public Action {
 public:
  explicit WaitForDomAction(ActionDelegate* delegate, const ActionProto& proto);
  ~WaitForDomAction() override;

 private:
  enum class SelectorPredicate {
    // The selector matches elements
    kMatch,

    // The selector doesn't match any elements
    kNoMatch
  };

  struct Condition {
    // Whether the selector should match or not.
    SelectorPredicate predicate = SelectorPredicate::kMatch;

    // The selector to look for.
    Selector selector;

    // True if the condition matched.
    bool match = false;

    // Status proto result associated with this condition.
    ProcessedActionStatusProto status_proto;

    // A payload to report to the server when this condition match. Empty
    // payloads are not reported.
    std::string server_payload;
  };

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  // Initializes |require_all_| and |conditions_| from |proto_|.
  void AddConditionsFromProto();

  // Adds a single condition to |conditions_|.
  void AddCondition(const WaitForDomProto::ElementCondition& condition);

  // Adds a single condition to |conditions_|.
  void AddCondition(SelectorPredicate predicate,
                    const ElementReferenceProto& selector_proto,
                    const std::string& server_payload);

  // Check all elements using the given BatchElementChecker and reports the
  // result to |callback|. In case of failure, the last failed status is
  // returned.
  void CheckElements(BatchElementChecker* checker,
                     base::OnceCallback<void(const ClientStatus&)> callback);
  void OnSingleElementCheckDone(size_t condition_index,
                                const ClientStatus& element_status);
  void OnAllElementChecksDone(
      base::OnceCallback<void(const ClientStatus&)> callback);

  void OnCheckDone(ProcessActionCallback callback, const ClientStatus& status);

  bool require_all_ = false;
  std::vector<Condition> conditions_;

  base::WeakPtrFactory<WaitForDomAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WaitForDomAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
