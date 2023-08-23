// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/credential_storage.h"
#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"

namespace nearby::chrome {

CredentialStorage::CredentialStorage(
    const mojo::SharedRemote<
        ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>&
        nearby_presence_credential_storage)
    : nearby_presence_credential_storage_(nearby_presence_credential_storage) {}

CredentialStorage::~CredentialStorage() = default;

// TODO(b/287333989): Implement.
void CredentialStorage::SaveCredentials(
    absl::string_view manager_app_id,
    absl::string_view account_name,
    const std::vector<LocalCredential>& private_credentials,
    const std::vector<SharedCredential>& public_credentials,
    PublicCredentialType public_credential_type,
    SaveCredentialsResultCallback callback) {
  std::vector<ash::nearby::presence::mojom::LocalCredentialPtr>
      local_credentials_mojom;
  for (const auto& local_credential : private_credentials) {
    local_credentials_mojom.push_back(
        ash::nearby::presence::proto::LocalCredentialToMojom(local_credential));
  }

  nearby_presence_credential_storage_->SaveCredentials(
      std::move(local_credentials_mojom),
      base::BindOnce(&CredentialStorage::OnCredentialsSaved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// TODO(b/287334012): Implement.
void CredentialStorage::UpdateLocalCredential(
    absl::string_view manager_app_id,
    absl::string_view account_name,
    nearby::internal::LocalCredential credential,
    SaveCredentialsResultCallback callback) {}

// TODO(b/287334225): Implement.
void CredentialStorage::GetLocalCredentials(
    const CredentialSelector& credential_selector,
    GetLocalCredentialsResultCallback callback) {}

// TODO(b/287334335): Implement.
void CredentialStorage::GetPublicCredentials(
    const CredentialSelector& credential_selector,
    PublicCredentialType public_credential_type,
    GetPublicCredentialsResultCallback callback) {}

void CredentialStorage::OnCredentialsSaved(
    nearby::presence::SaveCredentialsResultCallback
        on_credentials_saved_callback,
    mojo_base::mojom::AbslStatusCode credential_save_result) {
  if (credential_save_result == mojo_base::mojom::AbslStatusCode::kOk) {
    std::move(on_credentials_saved_callback)
        .credentials_saved_cb(absl::OkStatus());
  } else {
    std::move(on_credentials_saved_callback)
        .credentials_saved_cb(absl::Status(absl::StatusCode::kUnknown,
                                           "Failed to save to database."));
  }
}

}  // namespace nearby::chrome
