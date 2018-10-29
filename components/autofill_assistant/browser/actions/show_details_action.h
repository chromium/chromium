// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_DETAILS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_DETAILS_ACTION_H_

#include "base/macros.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to show contextual information.
class ShowDetailsAction : public Action {
 public:
  explicit ShowDetailsAction(const ActionProto& proto);
  ~ShowDetailsAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(ShowDetailsAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_DETAILS_ACTION_H_
