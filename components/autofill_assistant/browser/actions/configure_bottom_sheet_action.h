// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CONFIGURE_BOTTOM_SHEET_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CONFIGURE_BOTTOM_SHEET_ACTION_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Action to configure the viewport and peek height of the sheet.
class ConfigureBottomSheetAction : public Action {
 public:
  explicit ConfigureBottomSheetAction(ActionDelegate* delegate,
                                      const ActionProto& proto);
  ~ConfigureBottomSheetAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnWindowHeightChange(const ClientStatus& status);
  void OnTimeout();

  ProcessActionCallback callback_;
  base::OneShotTimer timer_;
  base::WeakPtrFactory<ConfigureBottomSheetAction> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ConfigureBottomSheetAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_CONFIGURE_BOTTOM_SHEET_ACTION_H_
