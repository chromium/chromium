// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/popup_message_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

PopupMessageAction::PopupMessageAction(ActionDelegate* delegate,
                                       const ActionProto& proto)
    : Action(delegate, proto) {}

PopupMessageAction::~PopupMessageAction() {}

void PopupMessageAction::InternalProcessAction(ProcessActionCallback callback) {
  delegate_->SetBubbleMessage(proto_.popup_message().message());
  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
