// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/browser/content_autofill_assistant_driver.h"

#include "base/files/file.h"
#include "base/guid.h"
#include "base/location.h"
#include "components/autofill_assistant/content/common/proto/semantic_feature_overrides.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace autofill_assistant {

DOCUMENT_USER_DATA_KEY_IMPL(ContentAutofillAssistantDriver);

ContentAutofillAssistantDriver::PendingCall::PendingCall(
    std::unique_ptr<base::OneShotTimer> timer,
    GetAnnotateDomModelCallback callback)
    : timer_(std::move(timer)), callback_(std::move(callback)) {}

ContentAutofillAssistantDriver::PendingCall::~PendingCall() = default;

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
  if (driver && annotate_dom_model_service) {
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
    std::move(callback).Run(mojom::ModelStatus::kUnexpectedError, base::File(),
                            /*overrides_policy=*/"");
    return;
  }

  absl::optional<base::File> file = annotate_dom_model_service_->GetModelFile();
  if (file) {
    std::move(callback).Run(mojom::ModelStatus::kSuccess, *std::move(file),
                            GetOverridesPolicy());
    return;
  }

  std::string guid(base::GenerateGUID());
  auto pending_call = std::make_unique<PendingCall>(
      std::make_unique<base::OneShotTimer>(), std::move(callback));
  pending_call->timer_->Start(
      FROM_HERE, timeout,
      base::BindOnce(&ContentAutofillAssistantDriver::RunCallback,
                     weak_pointer_factory_.GetWeakPtr(), guid,
                     mojom::ModelStatus::kTimeout, base::File()));
  pending_calls_.emplace(guid, std::move(pending_call));

  annotate_dom_model_service_->NotifyOnModelFileAvailable(base::BindOnce(
      &ContentAutofillAssistantDriver::OnModelAvailabilityChanged,
      weak_pointer_factory_.GetWeakPtr(), guid));
}

void ContentAutofillAssistantDriver::OnModelAvailabilityChanged(
    const std::string& guid,
    bool is_available) {
  if (!is_available) {
    RunCallback(guid, mojom::ModelStatus::kUnexpectedError, base::File());
    return;
  }

  absl::optional<base::File> file = annotate_dom_model_service_->GetModelFile();
  if (!file) {
    NOTREACHED() << "No model file where expected.";
    RunCallback(guid, mojom::ModelStatus::kUnexpectedError, base::File());
    return;
  }
  RunCallback(guid, mojom::ModelStatus::kSuccess, *std::move(file));
}

void ContentAutofillAssistantDriver::RunCallback(
    const std::string& guid,
    mojom::ModelStatus model_status,
    base::File model_file) {
  auto it = pending_calls_.find(guid);
  if (it == pending_calls_.end()) {
    return;
  }
  DCHECK(it->second->callback_);
  std::move(it->second->callback_)
      .Run(model_status, std::move(model_file), GetOverridesPolicy());
  pending_calls_.erase(it);
}

void ContentAutofillAssistantDriver::SetAnnotateDomModelService(
    AnnotateDomModelService* annotate_dom_model_service) {
  annotate_dom_model_service_ = annotate_dom_model_service;
}

std::string ContentAutofillAssistantDriver::GetOverridesPolicy() const {
  if (!annotate_dom_model_service_) {
    return "";
  }
  return annotate_dom_model_service_->GetOverridesPolicy();
}

}  // namespace autofill_assistant
