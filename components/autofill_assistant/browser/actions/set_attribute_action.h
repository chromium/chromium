// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_ATTRIBUTE_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_ATTRIBUTE_ACTION_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// An action to set the attribute of an element.
class SetAttributeAction : public Action {
 public:
  explicit SetAttributeAction(ActionDelegate* delegate,
                              const ActionProto& proto);
  ~SetAttributeAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnWaitForElement(ProcessActionCallback callback,
                        const Selector& selector,
                        const ClientStatus& element_status);
  void OnSetAttribute(ProcessActionCallback callback,
                      const ClientStatus& status);

  base::WeakPtrFactory<SetAttributeAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SetAttributeAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_ATTRIBUTE_ACTION_H_
