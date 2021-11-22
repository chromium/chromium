// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/browser/content_autofill_assistant_driver.h"

#include "base/files/file.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace autofill_assistant {

DOCUMENT_USER_DATA_KEY_IMPL(ContentAutofillAssistantDriver);

ContentAutofillAssistantDriver::ContentAutofillAssistantDriver(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<ContentAutofillAssistantDriver>(
          render_frame_host) {}
ContentAutofillAssistantDriver::~ContentAutofillAssistantDriver() = default;

void ContentAutofillAssistantDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAssistantDriver>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

const mojo::AssociatedRemote<mojom::AutofillAssistantAgent>&
ContentAutofillAssistantDriver::GetAutofillAssistantAgent() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!autofill_assistant_agent_) {
    render_frame_host().GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_assistant_agent_);
  }

  return autofill_assistant_agent_;
}

void ContentAutofillAssistantDriver::SetAnnotateDomModelService(
    AnnotateDomModelService* annotate_dom_model_service) {
  DCHECK(annotate_dom_model_service);
  annotate_dom_model_service_ = annotate_dom_model_service;
}

void ContentAutofillAssistantDriver::GetAnnotateDomModel(
    GetAnnotateDomModelCallback callback) {
  DCHECK(annotate_dom_model_service_);
  if (!annotate_dom_model_service_) {
    std::move(callback).Run(base::File());
  }

  absl::optional<base::File> file = annotate_dom_model_service_->GetModelFile();
  if (file) {
    std::move(callback).Run(file->Duplicate());
    return;
  }

  annotate_dom_model_service_->NotifyOnModelFileAvailable(base::BindOnce(
      &ContentAutofillAssistantDriver::OnModelAvailabilityChanged,
      weak_pointer_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentAutofillAssistantDriver::OnModelAvailabilityChanged(
    GetAnnotateDomModelCallback callback,
    bool is_available) {
  if (!is_available) {
    std::move(callback).Run(base::File());
    return;
  }

  auto file_opt = annotate_dom_model_service_->GetModelFile();
  DCHECK(file_opt);
  if (!file_opt) {
    std::move(callback).Run(base::File());
    return;
  }
  std::move(callback).Run(file_opt->Duplicate());
}

}  // namespace autofill_assistant
