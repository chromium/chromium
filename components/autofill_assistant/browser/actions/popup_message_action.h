// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_POPUP_MESSAGE_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_POPUP_MESSAGE_ACTION_H_

#include "base/macros.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// TODO: Action documentation.
// This action shows a popup message in the bottom bar anchored at the icon.
// The popup is just a Chrome TextBubble that is hidden when clicked. Also, a
// subsequent popup action will replace the message in the popup bubble, not add
// a second one.
class PopupMessageAction : public Action {
 public:
  explicit PopupMessageAction(ActionDelegate* delegate,
                              const ActionProto& proto);
  ~PopupMessageAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(PopupMessageAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_POPUP_MESSAGE_ACTION_H_
