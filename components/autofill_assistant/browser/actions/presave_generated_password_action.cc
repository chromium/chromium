// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/presave_generated_password_action.h"

#include <utility>
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/value_util.h"

namespace autofill_assistant {

PresaveGeneratedPasswordAction::PresaveGeneratedPasswordAction(
    ActionDelegate* delegate,
    const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_presave_generated_password());
}

PresaveGeneratedPasswordAction::~PresaveGeneratedPasswordAction() {}

void PresaveGeneratedPasswordAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  auto presave_password = proto_.presave_generated_password();

  if (presave_password.memory_key().empty()) {
    VLOG(1) << "PresaveGeneratedPasswordAction: empty |memory_key|";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  if (!delegate_->GetUserData()->HasAdditionalValue(
          presave_password.memory_key()) ||
      delegate_->GetUserData()
              ->GetAdditionalValue(presave_password.memory_key())
              ->strings()
              .values()
              .size() != 1) {
    VLOG(1) << "PresaveGeneratedPasswordAction: requested key '"
            << presave_password.memory_key()
            << "' not available in client memory";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  if (!delegate_->GetUserData()->selected_login_.has_value()) {
    VLOG(1) << "PresaveGeneratedPasswordAction: requested login details "
               "not available in client memory.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  if (!delegate_->GetUserData()->password_form_data_.has_value()) {
    VLOG(1) << "PresaveGeneratedPasswordAction: requested form data details "
               "not available in client memory.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  std::string password = delegate_->GetUserData()
                             ->GetAdditionalValue(presave_password.memory_key())
                             ->strings()
                             .values(0);

  delegate_->GetWebsiteLoginManager()->PresaveGeneratedPassword(
      *delegate_->GetUserData()->selected_login_, password,
      *delegate_->GetUserData()->password_form_data_,
      base::BindOnce(&PresaveGeneratedPasswordAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr(),
                     ClientStatus(ACTION_APPLIED)));
}

void PresaveGeneratedPasswordAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
