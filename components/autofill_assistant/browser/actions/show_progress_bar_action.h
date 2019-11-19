// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_PROGRESS_BAR_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_PROGRESS_BAR_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace autofill_assistant {
// An action to show the current progress.
class ShowProgressBarAction : public Action {
 public:
  explicit ShowProgressBarAction(ActionDelegate* delegate,
                                 const ActionProto& proto);
  ~ShowProgressBarAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(ShowProgressBarAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_PROGRESS_BAR_ACTION_H_
