// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/generate_password_for_form_field_action.h"

#include <utility>
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/value_util.h"

namespace autofill_assistant {

GeneratePasswordForFormFieldAction::GeneratePasswordForFormFieldAction(
    ActionDelegate* delegate,
    const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_generate_password_for_form_field());
}

GeneratePasswordForFormFieldAction::~GeneratePasswordForFormFieldAction() {}

void GeneratePasswordForFormFieldAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  selector_ = Selector(proto_.generate_password_for_form_field().element());
  if (selector_.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    EndAction(ClientStatus(INVALID_SELECTOR));
    return;
  }

  delegate_->RetrieveElementFormAndFieldData(
      selector_,
      base::BindOnce(&GeneratePasswordForFormFieldAction::
                         OnGetFormAndFieldDataForGeneration,
                     weak_ptr_factory_.GetWeakPtr(),
                     proto_.generate_password_for_form_field().memory_key()));
}

void GeneratePasswordForFormFieldAction::OnGetFormAndFieldDataForGeneration(
    const std::string& memory_key,
    const ClientStatus& status,
    content::RenderFrameHost* rfh,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& field_data) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  uint64_t max_length = field_data.max_length;
  absl::optional<std::string> password =
      delegate_->GetWebsiteLoginManager()->GeneratePassword(
          rfh, autofill::CalculateFormSignature(form_data),
          autofill::CalculateFieldSignatureForField(field_data), max_length);

  if (!password) {
    // In theory, GeneratePassword() could fail for other reasons, but in
    // practice, the only reason it can return absl::nullopt is if the
    // RenderFrameHost does not have a live RenderFrame (e.g. the renderer
    // process crashed).
    EndAction(ClientStatus(NO_RENDER_FRAME));
    return;
  }

  delegate_->WriteUserData(base::BindOnce(
      &GeneratePasswordForFormFieldAction::StoreGeneratedPasswordToUserData,
      weak_ptr_factory_.GetWeakPtr(), memory_key, *password, form_data));

  EndAction(ClientStatus(ACTION_APPLIED));
}

void GeneratePasswordForFormFieldAction::StoreGeneratedPasswordToUserData(
    const std::string& memory_key,
    const std::string& generated_password,
    const autofill::FormData& form_data,
    UserData* user_data,
    UserDataFieldChange* field_change) {
  DCHECK(user_data);
  user_data->SetAdditionalValue(
      memory_key,
      SimpleValue(generated_password, /* is_client_side_only = */ true));
  user_data->password_form_data_ = form_data;
}

void GeneratePasswordForFormFieldAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
