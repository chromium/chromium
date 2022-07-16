// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_STOP_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_STOP_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to stop Autofill Assistant.
class StopAction : public Action {
 public:
  explicit StopAction(ActionDelegate* delegate, const ActionProto& proto);

  StopAction(const StopAction&) = delete;
  StopAction& operator=(const StopAction&) = delete;

  ~StopAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_STOP_ACTION_H_
