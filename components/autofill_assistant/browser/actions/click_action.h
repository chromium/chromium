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
// An action to perform a mouse left button click on a given element on Web.
class ClickAction : public Action {
 public:
  explicit ClickAction(const ActionProto& proto);
  ~ClickAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) override;

  void OnWaitForElement(ActionDelegate* delegate,
                        ProcessActionCallback callback,
                        bool element_found);
  void OnClick(ProcessActionCallback callback, bool status);

  std::vector<std::string> target_element_selectors_;
  base::WeakPtrFactory<ClickAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClickAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CLICK_ACTION_H_
