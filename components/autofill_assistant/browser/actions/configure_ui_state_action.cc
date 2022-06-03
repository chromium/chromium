// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/configure_ui_state_action.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/viewport_mode.h"

namespace autofill_assistant {

ConfigureUiStateAction::ConfigureUiStateAction(ActionDelegate* delegate,
                                               const ActionProto& proto)
    : Action(delegate, proto) {}

ConfigureUiStateAction::~ConfigureUiStateAction() {}

void ConfigureUiStateAction::InternalProcessAction(
    ProcessActionCallback callback) {
  const ConfigureUiStateProto& proto = proto_.configure_ui_state();

  if (proto.has_overlay_behavior()) {
    delegate_->SetOverlayBehavior(proto.overlay_behavior());
  }

  UpdateProcessedAction(OkClientStatus());
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
