// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
// An action to ask Chrome to wait for a DOM element to process next action.
class WaitForDomAction : public Action {
 public:
  explicit WaitForDomAction(const ActionProto& proto);
  ~WaitForDomAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) override;

  void OnCheckDone(ProcessActionCallback callback);

  std::unique_ptr<BatchElementChecker> batch_element_checker_;
  DISALLOW_COPY_AND_ASSIGN(WaitForDomAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_DOM_ACTION_H_
