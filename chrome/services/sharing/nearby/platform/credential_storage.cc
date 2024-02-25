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

void CredentialStorage::SaveCredentials(
    std::string_view manager_app_id,
    std::string_view account_name,
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

  std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
      shared_credentials_mojom;
  for (const auto& shared_credential : public_credentials) {
    shared_credentials_mojom.push_back(
        ash::nearby::presence::proto::SharedCredentialToMojom(
            shared_credential));
  }

  nearby_presence_credential_storage_->SaveCredentials(
      std::move(local_credentials_mojom), std::move(shared_credentials_mojom),
      ash::nearby::presence::proto::PublicCredentialTypeToMojom(
          public_credential_type),
      base::BindOnce(&CredentialStorage::OnCredentialsSaved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CredentialStorage::UpdateLocalCredential(
    std::string_view manager_app_id,
    std::string_view account_name,
    nearby::internal::LocalCredential credential,
    SaveCredentialsResultCallback callback) {
  // Credentials are stored per-account with a constant manager_app_id, so
  // these parameters are disregarded.
  ash::nearby::presence::mojom::LocalCredentialPtr local_credential_mojom =
      ash::nearby::presence::proto::LocalCredentialToMojom(credential);

  nearby_presence_credential_storage_->UpdateLocalCredential(
      std::move(local_credential_mojom),
      base::BindOnce(&CredentialStorage::OnLocalCredentialUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CredentialStorage::GetLocalCredentials(
    const CredentialSelector& credential_selector,
    GetLocalCredentialsResultCallback callback) {
  // Because 'manager_app_id' and 'account_name' are consistent per user, and
  // credentials are stored in the user's cryptohome, 'credential_selector' is
  // redundant.
  nearby_presence_credential_storage_->GetPrivateCredentials(
      base::BindOnce(&CredentialStorage::OnLocalCredentialsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// TODO(b/287334335): Implement.
void CredentialStorage::GetPublicCredentials(
    const CredentialSelector& credential_selector,
    PublicCredentialType public_credential_type,
    GetPublicCredentialsResultCallback callback) {
  // TODO(b/299300880): Extract identity_type from credential_selector
  // and pass to the browser process to filter SharedCredentials.
  nearby_presence_credential_storage_->GetPublicCredentials(
      ash::nearby::presence::proto::PublicCredentialTypeToMojom(
          public_credential_type),
      base::BindOnce(&CredentialStorage::OnPublicCredentialsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

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

void CredentialStorage::OnPublicCredentialsRetrieved(
    nearby::presence::GetPublicCredentialsResultCallback
        on_public_credentials_retrieved_callback,
    mojo_base::mojom::AbslStatusCode retrieved_status,
    std::optional<
        std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>>
        shared_credentials_mojom) {
  if (retrieved_status != mojo_base::mojom::AbslStatusCode::kOk) {
    std::move(on_public_credentials_retrieved_callback)
        .credentials_fetched_cb(absl::Status(
            absl::StatusCode::kAborted,
            "Failed to retrieve public credentials from database."));
    return;
  }

  CHECK(shared_credentials_mojom.has_value());

  std::vector<SharedCredential> shared_credentials;
  for (const auto& shared_credential_mojom : *shared_credentials_mojom) {
    shared_credentials.push_back(
        ash::nearby::presence::proto::SharedCredentialFromMojom(
            shared_credential_mojom.get()));
  }

  std::move(on_public_credentials_retrieved_callback)
      .credentials_fetched_cb(shared_credentials);
}

void CredentialStorage::OnLocalCredentialsRetrieved(
    nearby::presence::GetLocalCredentialsResultCallback
        on_local_credentials_retrieved_callback,
    mojo_base::mojom::AbslStatusCode retrieved_status,
    std::optional<std::vector<ash::nearby::presence::mojom::LocalCredentialPtr>>
        local_credentials_mojom) {
  if (retrieved_status != mojo_base::mojom::AbslStatusCode::kOk) {
    std::move(on_local_credentials_retrieved_callback)
        .credentials_fetched_cb(absl::Status(
            absl::StatusCode::kAborted, "Failed to retrieve from database."));
    return;
  }

  CHECK(local_credentials_mojom.has_value());

  std::vector<LocalCredential> local_credentials;
  for (const auto& local_credential_mojom : *local_credentials_mojom) {
    local_credentials.push_back(
        ash::nearby::presence::proto::LocalCredentialFromMojom(
            local_credential_mojom.get()));
  }

  std::move(on_local_credentials_retrieved_callback)
      .credentials_fetched_cb(local_credentials);
}

void CredentialStorage::OnLocalCredentialUpdated(
    nearby::presence::SaveCredentialsResultCallback
        on_local_credential_updated_callback,
    mojo_base::mojom::AbslStatusCode update_status) {
  if (update_status != mojo_base::mojom::AbslStatusCode::kOk) {
    std::move(on_local_credential_updated_callback)
        .credentials_saved_cb(
            absl::Status(absl::StatusCode::kAborted,
                         "Failed to update the local credential."));
    return;
  }
  std::move(on_local_credential_updated_callback)
      .credentials_saved_cb(absl::OkStatus());
}

}  // namespace nearby::chrome
