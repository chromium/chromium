// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_details_action.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

ShowDetailsAction::ShowDetailsAction(ActionDelegate* delegate,
                                     const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_show_details());
}

ShowDetailsAction::~ShowDetailsAction() {}

void ShowDetailsAction::InternalProcessAction(ProcessActionCallback callback) {
  std::unique_ptr<Details> details = nullptr;
  bool details_valid = true;

  switch (proto_.show_details().data_to_show_case()) {
    case ShowDetailsProto::DataToShowCase::kDetails:
      details = std::make_unique<Details>();
      details_valid =
          Details::UpdateFromProto(proto_.show_details(), details.get());
      break;
    case ShowDetailsProto::DataToShowCase::kContactDetails:
      details = std::make_unique<Details>();
      details_valid = Details::UpdateFromContactDetails(
          proto_.show_details(), delegate_->GetClientMemory(), details.get());
      break;
    case ShowDetailsProto::DataToShowCase::kShippingAddress:
      details = std::make_unique<Details>();
      details_valid = Details::UpdateFromShippingAddress(
          proto_.show_details(), delegate_->GetClientMemory(), details.get());
      break;
    case ShowDetailsProto::DataToShowCase::kCreditCard:
      details = std::make_unique<Details>();
      details_valid = Details::UpdateFromSelectedCreditCard(
          proto_.show_details(), delegate_->GetClientMemory(), details.get());
      break;
    case ShowDetailsProto::DataToShowCase::DATA_TO_SHOW_NOT_SET:
      // Clear Details. Calling SetDetails with nullptr clears the details.
      break;
  }

  if (!details_valid) {
    DVLOG(1) << "Failed to fill the details";
    UpdateProcessedAction(INVALID_ACTION);
  } else {
    delegate_->SetDetails(std::move(details));
    UpdateProcessedAction(ACTION_APPLIED);
  }

  std::move(callback).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
