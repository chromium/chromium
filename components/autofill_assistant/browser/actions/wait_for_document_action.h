// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOCUMENT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOCUMENT_ACTION_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

class WaitForDocumentAction : public Action {
 public:
  explicit WaitForDocumentAction(ActionDelegate* delegate,
                                 const ActionProto& proto);
  ~WaitForDocumentAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnShortWaitForElement(const ClientStatus& status);

  void OnGetStartState(const ClientStatus& status,
                       DocumentReadyState start_state);
  void OnWaitForStartState(const ClientStatus& status,
                           DocumentReadyState end_state);
  void OnTimeout();
  void OnTimeoutInState(const ClientStatus& status,
                        DocumentReadyState end_state);
  void SendResult(const ClientStatus& status, DocumentReadyState end_state);

  ProcessActionCallback callback_;
  base::OneShotTimer timer_;
  base::WeakPtrFactory<WaitForDocumentAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WaitForDocumentAction);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOCUMENT_ACTION_H_
