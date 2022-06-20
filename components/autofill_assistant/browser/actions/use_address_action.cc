// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_address_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/field_formatter.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "components/autofill_assistant/core/public/autofill_assistant_intent.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

UseAddressAction::UseAddressAction(ActionDelegate* delegate,
                                   const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto.has_use_address());
  selector_ = Selector(proto.use_address().form_field_element());
}

UseAddressAction::~UseAddressAction() = default;

void UseAddressAction::InternalProcessAction(
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);

  if (selector_.empty() && !proto_.use_address().skip_autofill()) {
    VLOG(1) << "UseAddress failed: |selector| empty";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  if (proto_.use_address().skip_autofill() &&
      proto_.use_address().required_fields().empty()) {
    VLOG(1) << "UseAddress failed: |skip_autofill| without required fields";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  // Ensure data already selected in a previous action.
  switch (proto_.use_address().address_source_case()) {
    case UseAddressProto::kName: {
      if (proto_.use_address().name().empty()) {
        VLOG(1) << "UseAddress failed: |name| specified but empty";
        EndAction(ClientStatus(INVALID_ACTION));
        return;
      }
      auto* profile = delegate_->GetUserData()->selected_address(
          proto_.use_address().name());
      if (profile == nullptr) {
        auto* error_info = processed_action_proto_->mutable_status_details()
                               ->mutable_autofill_error_info();
        error_info->set_address_key_requested(proto_.use_address().name());
        error_info->set_client_memory_address_key_names(
            delegate_->GetUserData()->GetAllAddressKeyNames());
        error_info->set_address_pointee_was_null(true);
        VLOG(1) << "UseAddress failed: no profile found under "
                << proto_.use_address().name();
        EndAction(ClientStatus(PRECONDITION_FAILED));
        return;
      }
      profile_ = user_data::MakeUniqueFromProfile(*profile);
      break;
    }
    case UseAddressProto::kModelIdentifier: {
      if (proto_.use_address().model_identifier().empty()) {
        VLOG(1) << "UseAddress failed: |model_identifier| set but empty";
        EndAction(ClientStatus(INVALID_ACTION));
        return;
      }
      auto profile_value = delegate_->GetUserModel()->GetValue(
          proto_.use_address().model_identifier());
      if (!profile_value.has_value()) {
        VLOG(1) << "UseAddress failed: "
                << proto_.use_address().model_identifier()
                << " not found in user model";
        EndAction(ClientStatus(PRECONDITION_FAILED));
        return;
      }
      if (profile_value->profiles().values().size() != 1) {
        VLOG(1) << "UseAddress failed: expected single profile for "
                << proto_.use_address().model_identifier() << ", but got "
                << *profile_value;
        EndAction(ClientStatus(PRECONDITION_FAILED));
        return;
      }
      auto* profile = delegate_->GetUserModel()->GetProfile(
          profile_value->profiles().values(0));
      if (profile == nullptr) {
        VLOG(1) << "UseAddress failed: profile not found for: "
                << *profile_value;
        EndAction(ClientStatus(PRECONDITION_FAILED));
        return;
      }
      profile_ = user_data::MakeUniqueFromProfile(*profile);
      break;
    }
    case UseAddressProto::ADDRESS_SOURCE_NOT_SET:
      EndAction(ClientStatus(INVALID_ACTION));
      return;
  }
  DCHECK(profile_ != nullptr);

  FillFormWithData();
}

void UseAddressAction::EndAction(const ClientStatus& status) {
  if (fallback_handler_)
    action_stopwatch_.TransferToWaitTime(fallback_handler_->TotalWaitTime());

  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void UseAddressAction::FillFormWithData() {
  if (selector_.empty()) {
    DCHECK(proto_.use_address().skip_autofill());
    OnWaitForElement(OkClientStatus());
    return;
  }

  delegate_->ShortWaitForElementWithSlowWarning(
      selector_,
      base::BindOnce(&UseAddressAction::OnWaitForElementTimed,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&UseAddressAction::OnWaitForElement,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void UseAddressAction::OnWaitForElement(const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  DCHECK(profile_);
  InitFallbackHandler(*profile_);

  if (proto_.use_address().skip_autofill()) {
    ExecuteFallback(OkClientStatus());
    return;
  }

  DCHECK(!selector_.empty());
  delegate_->FindElement(
      selector_,
      base::BindOnce(
          &element_action_util::TakeElementAndPerform,
          base::BindOnce(&WebController::FillAddressForm,
                         delegate_->GetWebController()->GetWeakPtr(),
                         std::move(profile_),
                         ExtractIntentFromString(delegate_->GetIntent())),
          base::BindOnce(&UseAddressAction::ExecuteFallback,
                         weak_ptr_factory_.GetWeakPtr())));
}

void UseAddressAction::InitFallbackHandler(
    const autofill::AutofillProfile& profile) {
  std::vector<RequiredField> required_fields;
  for (const auto& required_field_proto :
       proto_.use_address().required_fields()) {
    if (!required_field_proto.has_value_expression()) {
      continue;
    }

    RequiredField required_field;
    required_field.FromProto(required_field_proto);
    required_fields.emplace_back(required_field);
  }

  DCHECK(fallback_handler_ == nullptr);
  fallback_handler_ = std::make_unique<RequiredFieldsFallbackHandler>(
      required_fields,
      field_formatter::CreateAutofillMappings(profile,
                                              /* locale = */ "en-US"),
      delegate_);
}

void UseAddressAction::ExecuteFallback(const ClientStatus& status) {
  DCHECK(fallback_handler_ != nullptr);
  fallback_handler_->CheckAndFallbackRequiredFields(
      status, base::BindOnce(&UseAddressAction::EndAction,
                             weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace autofill_assistant
