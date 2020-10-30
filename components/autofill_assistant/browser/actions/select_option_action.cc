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
#include "components/autofill_assistant/browser/user_data_util.h"

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
      ClientStatus autofill_status = GetFormattedAutofillValue(
          select_option.autofill_value(), delegate_->GetUserData(), &value_);
      if (!autofill_status.ok()) {
        EndAction(autofill_status);
        return;
      }
      break;
    }
    default:
      VLOG(1) << "Unrecognized field for SelectOptionAction";
      EndAction(ClientStatus(INVALID_ACTION));
      return;
  }

  delegate_->ShortWaitForElement(
      selector,
      base::BindOnce(&SelectOptionAction::OnWaitForElementTimed,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&SelectOptionAction::OnWaitForElement,
                                    weak_ptr_factory_.GetWeakPtr(), selector)));
}

void SelectOptionAction::OnWaitForElement(const Selector& selector,
                                          const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  action_delegate_util::FindElementAndPerform(
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
