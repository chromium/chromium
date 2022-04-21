// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOCUMENT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOCUMENT_ACTION_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
class ElementFinderResult;

class WaitForDocumentAction : public Action {
 public:
  explicit WaitForDocumentAction(ActionDelegate* delegate,
                                 const ActionProto& proto);

  WaitForDocumentAction(const WaitForDocumentAction&) = delete;
  WaitForDocumentAction& operator=(const WaitForDocumentAction&) = delete;

  ~WaitForDocumentAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnShortWaitForElement(const Selector& frame_selector,
                             const ClientStatus& status);
  void OnFindElement(const ClientStatus& status,
                     std::unique_ptr<ElementFinderResult> element);
  void WaitForReadyState();

  void OnGetStartState(const ClientStatus& status,
                       DocumentReadyState start_state);
  void OnWaitForStartState(const ClientStatus& status,
                           DocumentReadyState current_state,
                           base::TimeDelta wait_time);
  void OnTimeoutInState(const ClientStatus& original_status,
                        const ClientStatus& status,
                        DocumentReadyState end_state);
  void SendResult(const ClientStatus& status, DocumentReadyState end_state);

  ProcessActionCallback callback_;
  std::unique_ptr<ElementFinderResult> optional_frame_element_;
  base::WeakPtrFactory<WaitForDocumentAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOCUMENT_ACTION_H_
