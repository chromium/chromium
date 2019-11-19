// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CLICK_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CLICK_ACTION_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
class ClientStatus;

// This action performs a click on a given element.
class ClickAction : public Action {
 public:
  enum ClickType { TAP = 0, JAVASCRIPT = 1, CLICK = 2 };

  explicit ClickAction(ActionDelegate* delegate, const ActionProto& proto);
  ~ClickAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnWaitForElement(ProcessActionCallback callback,
                        const Selector& selector,
                        const ClientStatus& element_status);
  void OnClick(ProcessActionCallback callback, const ClientStatus& status);

  ClickType click_type_;
  base::WeakPtrFactory<ClickAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClickAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CLICK_ACTION_H_
