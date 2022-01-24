// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_TOUCHABLE_AREA_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_TOUCHABLE_AREA_ACTION_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Sets the touchable and restricted area of the overlay.
class SetTouchableAreaAction : public Action {
 public:
  explicit SetTouchableAreaAction(ActionDelegate* delegate,
                                  const ActionProto& proto);
  ~SetTouchableAreaAction() override;

  SetTouchableAreaAction(const SetTouchableAreaAction&) = delete;
  SetTouchableAreaAction& operator=(const SetTouchableAreaAction&) = delete;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  base::WeakPtrFactory<SetTouchableAreaAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SET_TOUCHABLE_AREA_ACTION_H_
