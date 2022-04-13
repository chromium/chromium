// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CHECK_OPTION_ELEMENT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CHECK_OPTION_ELEMENT_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {

// Action to check an <option> element inside a <select> element.
class CheckOptionElementAction : public Action {
 public:
  explicit CheckOptionElementAction(ActionDelegate* delegate,
                                    const ActionProto& proto);
  ~CheckOptionElementAction() override;

  CheckOptionElementAction(const CheckOptionElementAction&) = delete;
  CheckOptionElementAction& operator=(const CheckOptionElementAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnCheckSelectedOptionElement(const ClientStatus& status);

  void EndAction(const ClientStatus& status);

  ElementFinderResult select_;
  ElementFinderResult option_;
  ProcessActionCallback callback_;

  base::WeakPtrFactory<CheckOptionElementAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CHECK_OPTION_ELEMENT_ACTION_H_
