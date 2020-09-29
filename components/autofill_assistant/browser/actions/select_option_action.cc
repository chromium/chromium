// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/select_option_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/field_formatter.h"

namespace autofill_assistant {

SelectOptionAction::SelectOptionAction(ActionDelegate* delegate,
                                       const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_select_option());
}

SelectOptionAction::~SelectOptionAction() {}

void SelectOptionAction::InternalProcessAction(ProcessActionCallback callback) {
  process_action_callback_ = std::move(callback);
  const SelectOptionProto& select_option = proto_.select_option();

  Selector selector = Selector(select_option.element());
  if (selector.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    EndAction(ClientStatus(INVALID_SELECTOR));
    return;
  }

  switch (select_option.value_case()) {
    case SelectOptionProto::kSelectedOption:
      if (select_option.selected_option().empty()) {
        VLOG(1) << __func__ << ": empty |selected_option|";
        EndAction(ClientStatus(INVALID_ACTION));
        return;
      }

      value_ = select_option.selected_option();
      break;
    case SelectOptionProto::kAutofillValue: {
      if (select_option.autofill_value().profile().identifier().empty() ||
          select_option.autofill_value().value_expression().empty()) {
        VLOG(1) << "SelectOptionAction: |autofill_value| with empty "
                   "|profile.identifier| or |value_expression|";
        EndAction(ClientStatus(INVALID_ACTION));
        return;
      }

      const autofill::AutofillProfile* address =
          delegate_->GetUserData()->selected_address(
              select_option.autofill_value().profile().identifier());
      if (address == nullptr) {
        VLOG(1) << "SelectOptionAction: requested unknown address '"
                << select_option.autofill_value().profile().identifier() << "'";
        EndAction(ClientStatus(PRECONDITION_FAILED));
        return;
      }

      auto value = field_formatter::FormatString(
          select_option.autofill_value().value_expression(),
          field_formatter::CreateAutofillMappings(*address,
                                                  /* locale= */ "en-US"));
      if (!value.has_value()) {
        EndAction(ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE));
        return;
      }

      value_ = *value;
      break;
    }
    default:
      VLOG(1) << "Unrecognized field for SelectOptionAction";
      EndAction(ClientStatus(INVALID_ACTION));
      return;
  }

  delegate_->ShortWaitForElement(
      selector, base::BindOnce(&SelectOptionAction::OnWaitForElement,
                               weak_ptr_factory_.GetWeakPtr(), selector));
}

void SelectOptionAction::OnWaitForElement(const Selector& selector,
                                          const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  ActionDelegateUtil::FindElementAndPerform(
      delegate_, selector,
      base::BindOnce(&ActionDelegate::SelectOption, delegate_->GetWeakPtr(),
                     value_, proto_.select_option().select_strategy()),
      base::BindOnce(&SelectOptionAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SelectOptionAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
