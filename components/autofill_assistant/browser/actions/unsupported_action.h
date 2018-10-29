// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_UNSUPPORTED_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_UNSUPPORTED_ACTION_H_

#include "base/macros.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An unsupported action that always fails.
class UnsupportedAction : public Action {
 public:
  explicit UnsupportedAction(const ActionProto& proto);
  ~UnsupportedAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) override;

  void OnUnsupported(ProcessActionCallback callback, bool status);

  DISALLOW_COPY_AND_ASSIGN(UnsupportedAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_UNSUPPORTED_ACTION_H_
