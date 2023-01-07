// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/reset_pending_credentials_action.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/password_manager/core/browser/password_manager.h"

namespace autofill_assistant {

ResetPendingCredentialsAction::ResetPendingCredentialsAction(
    ActionDelegate* delegate,
    const ActionProto& proto)
    : Action(delegate, proto) {}

ResetPendingCredentialsAction::~ResetPendingCredentialsAction() {}

void ResetPendingCredentialsAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  delegate_->GetWebsiteLoginManager()->ResetPendingCredentials();

  EndAction(ClientStatus(ACTION_APPLIED));
}

void ResetPendingCredentialsAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
