// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/delete_password_action.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/user_data.h"

namespace autofill_assistant {

DeletePasswordAction::DeletePasswordAction(ActionDelegate* delegate,
                                           const ActionProto& proto)
    : Action(delegate, proto) {}

DeletePasswordAction::~DeletePasswordAction() {}

void DeletePasswordAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  if (!delegate_->GetUserData()->selected_login_.has_value()) {
    VLOG(1) << "DeletePasswordAction: requested login details "
               "not available in client memory.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  delegate_->GetWebsiteLoginManager()->DeletePasswordForLogin(
      *delegate_->GetUserData()->selected_login_,
      base::BindOnce(&DeletePasswordAction::OnPasswordDeleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeletePasswordAction::OnPasswordDeleted(bool success) {
  auto status = success ? ClientStatus(ACTION_APPLIED)
                        : ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE);
  EndAction(status);
}

void DeletePasswordAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
