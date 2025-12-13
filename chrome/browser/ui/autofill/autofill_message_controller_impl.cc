// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_message_controller_impl.h"

#include <memory>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "components/messages/android/message_dispatcher_bridge.h"

namespace autofill {

AutofillMessageControllerImpl::AutofillMessageControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(*web_contents) {}

AutofillMessageControllerImpl::~AutofillMessageControllerImpl() {
  Dismiss();
}

void AutofillMessageControllerImpl::Show(
    std::unique_ptr<AutofillMessageModel> message_model) {
  AutofillMessageModel* message_model_ptr = message_model.get();
  message_models_.insert(std::move(message_model));

  message_model_ptr->GetMessage(/*pass_key=*/{})
      .SetActionClick(
          base::BindOnce(&AutofillMessageControllerImpl::OnActionClicked,
                         weak_ptr_factory_.GetWeakPtr(), message_model_ptr));
  message_model_ptr->GetMessage(/*pass_key=*/{})
      .SetDismissCallback(
          base::BindOnce(&AutofillMessageControllerImpl::OnDismissed,
                         weak_ptr_factory_.GetWeakPtr(), message_model_ptr));

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      &message_model_ptr->GetMessage(/*pass_key=*/{}), &web_contents_.get(),
      messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);

  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Message.", message_model_ptr->GetTypeAsString(),
                    ".Shown"}),
      true);
}

void AutofillMessageControllerImpl::OnActionClicked(
    AutofillMessageModel* message_model_ptr) {
  auto message_model_it = message_models_.find(message_model_ptr);
  CHECK(message_model_it != message_models_.end());
  (*message_model_it)->OnActionClicked();

  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Message.", (*message_model_it)->GetTypeAsString(),
                    ".ActionClicked"}),
      true);
}

void AutofillMessageControllerImpl::OnDismissed(
    AutofillMessageModel* message_model_ptr,
    messages::DismissReason reason) {
  auto message_model_it = message_models_.find(message_model_ptr);
  CHECK(message_model_it != message_models_.end());
  (*message_model_it)->OnDismissed(reason);

  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.Message.", (*message_model_it)->GetTypeAsString(),
                    ".Dismissed"}),
      reason);

  message_models_.erase(message_model_it);
}

void AutofillMessageControllerImpl::Dismiss() {
  for (auto it = message_models_.begin(); it != message_models_.end();) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        &(*it++)->GetMessage(/*pass_key=*/{}),
        messages::DismissReason::UNKNOWN);
  }
}

}  // namespace autofill
