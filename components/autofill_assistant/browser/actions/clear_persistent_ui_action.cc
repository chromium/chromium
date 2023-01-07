// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/clear_persistent_ui_action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

ClearPersistentUiAction::ClearPersistentUiAction(ActionDelegate* delegate,
                                                 const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_clear_persistent_ui());
}

ClearPersistentUiAction::~ClearPersistentUiAction() = default;

void ClearPersistentUiAction::InternalProcessAction(
    ProcessActionCallback callback) {
  delegate_->ClearPersistentGenericUi();
  UpdateProcessedAction(OkClientStatus());
  std::move(callback).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
