// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/tell_action.h"

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

TellAction::TellAction(ActionDelegate* delegate, const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_tell());
}

TellAction::~TellAction() {}

void TellAction::InternalProcessAction(ProcessActionCallback callback) {
  if (proto_.tell().has_message()) {
    delegate_->SetStatusMessage(proto_.tell().message());
  }

  if (proto_.tell().needs_ui())
    delegate_->RequireUI();

  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
