// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_TELL_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_TELL_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to display a message.
class TellAction : public Action {
 public:
  explicit TellAction(ActionDelegate* delegate, const ActionProto& proto);

  TellAction(const TellAction&) = delete;
  TellAction& operator=(const TellAction&) = delete;

  ~TellAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_TELL_ACTION_H_
