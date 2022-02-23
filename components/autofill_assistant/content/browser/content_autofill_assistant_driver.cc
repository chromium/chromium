// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/browser/content_autofill_assistant_driver.h"

#include "base/files/file.h"
#include "base/location.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace autofill_assistant {

DOCUMENT_USER_DATA_KEY_IMPL(ContentAutofillAssistantDriver);

ContentAutofillAssistantDriver::ContentAutofillAssistantDriver(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<ContentAutofillAssistantDriver>(
          render_frame_host) {}
ContentAutofillAssistantDriver::~ContentAutofillAssistantDriver() = default;

// static
void ContentAutofillAssistantDriver::BindDriver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAssistantDriver>
        pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  auto* driver = ContentAutofillAssistantDriver::GetOrCreateForCurrentDocument(
      render_frame_host);
  if (driver) {
    driver->BindPendingReceiver(std::move(pending_receiver));
  }
}

// static
ContentAutofillAssistantDriver*
ContentAutofillAssistantDriver::GetOrCreateForRenderFrameHost(
    content::RenderFrameHost* render_frame_host,
    AnnotateDomModelService* annotate_dom_model_service) {
  ContentAutofillAssistantDriver* driver =
      ContentAutofillAssistantDriver::GetOrCreateForCurrentDocument(
          render_frame_host);
  if (driver) {
    driver->SetAnnotateDomModelService(annotate_dom_model_service);
  }
  return driver;
}

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

void ContentAutofillAssistantDriver::GetAnnotateDomModel(
    base::TimeDelta timeout,
    GetAnnotateDomModelCallback callback) {
  if (!annotate_dom_model_service_) {
    NOTREACHED() << "No model service";
    std::move(callback).Run(mojom::ModelStatus::kUnexpectedError, base::File());
    return;
  }

  absl::optional<base::File> file = annotate_dom_model_service_->GetModelFile();
  if (file) {
    std::move(callback).Run(mojom::ModelStatus::kSuccess, *std::move(file));
    return;
  }

  callback_ = std::move(callback);
  timer_ = std::make_unique<base::OneShotTimer>();
  timer_->Start(FROM_HERE, timeout,
                base::BindOnce(&ContentAutofillAssistantDriver::OnTimeout,
                               weak_pointer_factory_.GetWeakPtr()));

  annotate_dom_model_service_->NotifyOnModelFileAvailable(base::BindOnce(
      &ContentAutofillAssistantDriver::OnModelAvailabilityChanged,
      weak_pointer_factory_.GetWeakPtr()));
}

void ContentAutofillAssistantDriver::OnTimeout() {
  if (!callback_) {
    return;
  }
  std::move(callback_).Run(mojom::ModelStatus::kTimeout, base::File());
}

void ContentAutofillAssistantDriver::OnModelAvailabilityChanged(
    bool is_available) {
  if (!callback_) {
    return;
  }

  if (!is_available) {
    std::move(callback_).Run(mojom::ModelStatus::kUnexpectedError,
                             base::File());
    return;
  }

  absl::optional<base::File> file = annotate_dom_model_service_->GetModelFile();
  if (!file) {
    NOTREACHED() << "No model file where expected.";
    std::move(callback_).Run(mojom::ModelStatus::kUnexpectedError,
                             base::File());
    return;
  }
  std::move(callback_).Run(mojom::ModelStatus::kSuccess, *std::move(file));
}

void ContentAutofillAssistantDriver::SetAnnotateDomModelService(
    AnnotateDomModelService* annotate_dom_model_service) {
  DCHECK(annotate_dom_model_service);
  annotate_dom_model_service_ = annotate_dom_model_service;
}

}  // namespace autofill_assistant
