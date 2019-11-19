// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_INFO_BOX_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_INFO_BOX_ACTION_H_

#include "base/macros.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Action to show informational content in the sheet.
class ShowInfoBoxAction : public Action {
 public:
  explicit ShowInfoBoxAction(ActionDelegate* delegate,
                             const ActionProto& proto);
  ~ShowInfoBoxAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(ShowInfoBoxAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_INFO_BOX_ACTION_H_
