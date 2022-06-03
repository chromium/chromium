// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_NAVIGATE_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_NAVIGATE_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to navigate to a given web page.
class NavigateAction : public Action {
 public:
  explicit NavigateAction(ActionDelegate* delegate, const ActionProto& proto);

  NavigateAction(const NavigateAction&) = delete;
  NavigateAction& operator=(const NavigateAction&) = delete;

  ~NavigateAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_NAVIGATE_ACTION_H_
