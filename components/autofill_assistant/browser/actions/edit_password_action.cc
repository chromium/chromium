// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/edit_password_action.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/value_util.h"

namespace autofill_assistant {

EditPasswordAction::EditPasswordAction(ActionDelegate* delegate,
                                       const ActionProto& proto)
    : Action(delegate, proto) {}

EditPasswordAction::~EditPasswordAction() {}

void EditPasswordAction::InternalProcessAction(ProcessActionCallback callback) {
  callback_ = std::move(callback);
  const auto& edit_password = proto_.edit_password();

  if (edit_password.memory_key().empty()) {
    VLOG(1) << "EditPasswordAction: empty |memory_key|";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  if (!delegate_->GetUserData()->HasAdditionalValue(
          edit_password.memory_key()) ||
      delegate_->GetUserData()
              ->GetAdditionalValue(edit_password.memory_key())
              ->strings()
              .values()
              .size() != 1) {
    VLOG(1) << "EditPasswordAction: requested key '"
            << edit_password.memory_key() << "' not available in client memory";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  if (!delegate_->GetUserData()->selected_login_.has_value()) {
    VLOG(1) << "EditPasswordAction: requested login details "
               "not available in client memory.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  std::string password = delegate_->GetUserData()
                             ->GetAdditionalValue(edit_password.memory_key())
                             ->strings()
                             .values(0);

  delegate_->GetWebsiteLoginManager()->EditPasswordForLogin(
      *delegate_->GetUserData()->selected_login_, password,
      base::BindOnce(&EditPasswordAction::OnPasswordEdited,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EditPasswordAction::OnPasswordEdited(bool success) {
  auto status = success ? ClientStatus(ACTION_APPLIED)
                        : ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE);
  EndAction(status);
}

void EditPasswordAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
