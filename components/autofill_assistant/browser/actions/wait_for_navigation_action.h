// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_NAVIGATION_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_NAVIGATION_ACTION_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

class WaitForNavigationAction : public Action {
 public:
  explicit WaitForNavigationAction(ActionDelegate* delegate,
                                   const ActionProto& proto);
  ~WaitForNavigationAction() override;

 private:
  static constexpr base::TimeDelta kDefaultTimeout =
      base::TimeDelta::FromSeconds(20);

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnWaitForNavigation(bool has_navigation_error);
  void OnTimeout();
  void SendResult(ProcessedActionStatusProto status);

  ProcessActionCallback callback_;
  base::OneShotTimer timer_;
  base::WeakPtrFactory<WaitForNavigationAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WaitForNavigationAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_WAIT_FOR_NAVIGATION_ACTION_H_
