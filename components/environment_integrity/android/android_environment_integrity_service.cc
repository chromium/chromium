// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/environment_integrity/android/android_environment_integrity_service.h"

#include "components/environment_integrity/android/android_environment_integrity_data_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/origin.h"

namespace environment_integrity {

AndroidEnvironmentIntegrityService::AndroidEnvironmentIntegrityService(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

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
                                         std::move(receiver));
}

void AndroidEnvironmentIntegrityService::GetEnvironmentIntegrity(
    GetEnvironmentIntegrityCallback callback) {
  url::Origin origin =
      render_frame_host().GetOutermostMainFrame()->GetLastCommittedOrigin();
  content::StoragePartition* storage_partition =
      render_frame_host().GetOutermostMainFrame()->GetStoragePartition();
  AndroidEnvironmentIntegrityDataManager* data_manager =
      AndroidEnvironmentIntegrityDataManager::GetOrCreateForStoragePartition(
          storage_partition);

  data_manager->GetHandle(
      origin, base::BindOnce(&AndroidEnvironmentIntegrityService::OnGetHandle,
                             weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AndroidEnvironmentIntegrityService::OnGetHandle(
    GetEnvironmentIntegrityCallback callback,
    absl::optional<int64_t> maybe_handle) {
  // If no handle is stored, create a new handle and store it.
  int64_t handle;

  if (maybe_handle.has_value()) {
    handle = maybe_handle.value();
  } else {
    // TODO(crbug.com/1439945) Get handle from PIA Crystal API.
    handle = 0;

    url::Origin origin =
        render_frame_host().GetOutermostMainFrame()->GetLastCommittedOrigin();
    content::StoragePartition* storage_partition =
        render_frame_host().GetOutermostMainFrame()->GetStoragePartition();
    AndroidEnvironmentIntegrityDataManager* data_manager =
        AndroidEnvironmentIntegrityDataManager::GetOrCreateForStoragePartition(
            storage_partition);

    data_manager->SetHandle(origin, handle);
  }

  GetIntegrityTokenForHandle(handle, std::move(callback));
}

void AndroidEnvironmentIntegrityService::GetIntegrityTokenForHandle(
    int64_t handle,
    GetEnvironmentIntegrityCallback callback) {
  // TODO(crbug.com/1439945) Get integrity token from PIA Crystal API.
  std::move(callback).Run();
}

}  // namespace environment_integrity
