// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_RELEASE_ELEMENTS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_RELEASE_ELEMENTS_ACTION_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

// An action to release elements from the store.
class ReleaseElementsAction : public Action {
 public:
  explicit ReleaseElementsAction(ActionDelegate* delegate,
                                 const ActionProto& proto);
  ~ReleaseElementsAction() override;

  ReleaseElementsAction(const ReleaseElementsAction&) = delete;
  ReleaseElementsAction& operator=(const ReleaseElementsAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  ProcessActionCallback process_action_callback_;

  base::WeakPtrFactory<ReleaseElementsAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_RELEASE_ELEMENTS_ACTION_H_
