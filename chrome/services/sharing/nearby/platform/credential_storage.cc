// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/credential_storage.h"

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
    SaveCredentialsResultCallback callback) {}

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

}  // namespace nearby::chrome
