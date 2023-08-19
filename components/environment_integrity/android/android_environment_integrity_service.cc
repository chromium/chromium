// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/environment_integrity/android/android_environment_integrity_service.h"

#include <memory>
#include "base/functional/bind.h"
#include "components/environment_integrity/android/android_environment_integrity_data_manager.h"
#include "components/environment_integrity/android/integrity_service_bridge.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/origin.h"

namespace environment_integrity {

using blink::mojom::EnvironmentIntegrityResponseCode;

namespace {

EnvironmentIntegrityResponseCode MapResponseCode(
    IntegrityResponse response_code) {
  switch (response_code) {
    case IntegrityResponse::kSuccess:
      return EnvironmentIntegrityResponseCode::kSuccess;
    case IntegrityResponse::kTimeout:
      return EnvironmentIntegrityResponseCode::kTimeout;
    case IntegrityResponse::kUnknownError:
    case IntegrityResponse::kApiNotAvailable:
    case IntegrityResponse::kInvalidHandle:
      return EnvironmentIntegrityResponseCode::kInternalError;
  }
}
}  // namespace

AndroidEnvironmentIntegrityService::AndroidEnvironmentIntegrityService(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService> receiver,
    std::unique_ptr<IntegrityService> integrity_service)
    : DocumentService(render_frame_host, std::move(receiver)),
      integrity_service_(std::move(integrity_service)) {}

AndroidEnvironmentIntegrityService::~AndroidEnvironmentIntegrityService() =
    default;

// static
void AndroidEnvironmentIntegrityService::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService> receiver) {
  CHECK(render_frame_host);

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new AndroidEnvironmentIntegrityService(*render_frame_host,
                                         std::move(receiver),
                                         std::make_unique<IntegrityService>());
}

// static
void AndroidEnvironmentIntegrityService::CreateForTest(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService> receiver,
    std::unique_ptr<IntegrityService> integrity_service) {
  new AndroidEnvironmentIntegrityService(
      *render_frame_host, std::move(receiver), std::move(integrity_service));
}

AndroidEnvironmentIntegrityDataManager*
AndroidEnvironmentIntegrityService::GetDataManager() {
  content::StoragePartition* storage_partition =
      render_frame_host().GetOutermostMainFrame()->GetStoragePartition();
  return AndroidEnvironmentIntegrityDataManager::GetOrCreateForStoragePartition(
      storage_partition);
}

void AndroidEnvironmentIntegrityService::GetEnvironmentIntegrity(
    const std::vector<uint8_t>& content_binding,
    GetEnvironmentIntegrityCallback callback) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebEnvironmentIntegrity)) {
    ReportBadMessageAndDeleteThis(
        "Feature not enabled. IPC call not expected.");
    return;
  }
  if (!integrity_service_->IsIntegrityAvailable()) {
    std::move(callback).Run(EnvironmentIntegrityResponseCode::kInternalError,
                            std::vector<uint8_t>());
    return;
  }

  const url::Origin& origin =
      render_frame_host().GetOutermostMainFrame()->GetLastCommittedOrigin();
  GetDataManager()->GetHandle(
      origin, base::BindOnce(&AndroidEnvironmentIntegrityService::OnGetHandle,
                             weak_factory_.GetWeakPtr(),
                             EnvironmentIntegrityRequest(std::move(callback),
                                                         content_binding)));
}

void AndroidEnvironmentIntegrityService::OnGetHandle(
    EnvironmentIntegrityRequest request,
    absl::optional<int64_t> maybe_handle) {
  if (!maybe_handle) {
    // If no handle is stored, create a new handle and store it.
    integrity_service_->CreateIntegrityHandle(
        base::BindOnce(&AndroidEnvironmentIntegrityService::OnCreateHandle,
                       weak_factory_.GetWeakPtr(), std::move(request)));
    return;
  }

  request.handle = *maybe_handle;
  // Create copy to pass into Java code
  std::vector<uint8_t> content_binding = request.content_binding;
  integrity_service_->GetEnvironmentIntegrityToken(
      *maybe_handle, content_binding,
      base::BindOnce(&AndroidEnvironmentIntegrityService::OnGetToken,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void AndroidEnvironmentIntegrityService::OnCreateHandle(
    EnvironmentIntegrityRequest request,
    HandleCreationResult result) {
  if (result.response_code != IntegrityResponse::kSuccess) {
    std::move(request.callback).Run(MapResponseCode(result.response_code), {});
    return;
  }

  request.handle = result.handle;
  const url::Origin& origin =
      render_frame_host().GetOutermostMainFrame()->GetLastCommittedOrigin();
  GetDataManager()->SetHandle(origin, request.handle);

  std::vector<uint8_t> content_binding = request.content_binding;
  integrity_service_->GetEnvironmentIntegrityToken(
      result.handle, content_binding,
      base::BindOnce(&AndroidEnvironmentIntegrityService::OnGetToken,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void AndroidEnvironmentIntegrityService::OnGetToken(
    EnvironmentIntegrityRequest request,
    GetTokenResult result) {
  if (result.response_code == IntegrityResponse::kInvalidHandle &&
      !request.recreating_old_handle) {
    // If the handle was invalid and we didn't already try to replace it,
    // restart the flow from creating a new handle.
    request.recreating_old_handle = true;
    integrity_service_->CreateIntegrityHandle(
        base::BindOnce(&AndroidEnvironmentIntegrityService::OnCreateHandle,
                       weak_factory_.GetWeakPtr(), std::move(request)));
    return;
  }

  if (result.response_code != IntegrityResponse::kSuccess) {
    // Respond with an empty token.
    std::move(request.callback)
        .Run(MapResponseCode(result.response_code), std::vector<uint8_t>());
    return;
  }

  std::move(request.callback)
      .Run(MapResponseCode(result.response_code), std::move(result.token));
}

AndroidEnvironmentIntegrityService::EnvironmentIntegrityRequest::
    EnvironmentIntegrityRequest(
        GetEnvironmentIntegrityCallback callback_param,
        const std::vector<uint8_t>& content_binding_param)
    : callback(std::move(callback_param)),
      content_binding(content_binding_param),
      handle(0),
      recreating_old_handle(false) {}

AndroidEnvironmentIntegrityService::EnvironmentIntegrityRequest::
    ~EnvironmentIntegrityRequest() = default;
AndroidEnvironmentIntegrityService::EnvironmentIntegrityRequest::
    EnvironmentIntegrityRequest(
        AndroidEnvironmentIntegrityService::EnvironmentIntegrityRequest&&) =
        default;
AndroidEnvironmentIntegrityService::EnvironmentIntegrityRequest&
AndroidEnvironmentIntegrityService::EnvironmentIntegrityRequest::operator=(
    AndroidEnvironmentIntegrityService::EnvironmentIntegrityRequest&&) =
    default;

}  // namespace environment_integrity
