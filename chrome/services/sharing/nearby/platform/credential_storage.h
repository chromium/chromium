// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_CREDENTIAL_STORAGE_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_CREDENTIAL_STORAGE_H_

#include <optional>

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/nearby/src/internal/platform/implementation/credential_callbacks.h"
#include "third_party/nearby/src/internal/platform/implementation/credential_storage.h"

namespace nearby::chrome {

// A platform interface used to triggle file I/O credential messages on
// a Mojo remote to communicate with the browser process.
class CredentialStorage : public nearby::api::CredentialStorage {
 public:
  explicit CredentialStorage(
      const mojo::SharedRemote<
          ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>&
          nearby_presence_credential_storage);

  CredentialStorage(CredentialStorage&& other) = delete;
  CredentialStorage& operator=(CredentialStorage&& other) = delete;

  ~CredentialStorage() override;

  // nearby::api::CredentialStorage:
  void SaveCredentials(std::string_view manager_app_id,
                       std::string_view account_name,
                       const std::vector<LocalCredential>& private_credentials,
                       const std::vector<SharedCredential>& public_credentials,
                       PublicCredentialType public_credential_type,
                       SaveCredentialsResultCallback callback) override;
  void UpdateLocalCredential(std::string_view manager_app_id,
                             std::string_view account_name,
                             nearby::internal::LocalCredential credential,
                             SaveCredentialsResultCallback callback) override;
  void GetLocalCredentials(const CredentialSelector& credential_selector,
                           GetLocalCredentialsResultCallback callback) override;
  void GetPublicCredentials(
      const CredentialSelector& credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback) override;

 private:
  void OnCredentialsSaved(
      nearby::presence::SaveCredentialsResultCallback
          on_credentials_saved_callback,
      mojo_base::mojom::AbslStatusCode credential_save_result);
  void OnPublicCredentialsRetrieved(
      nearby::presence::GetPublicCredentialsResultCallback
          on_public_credentials_retrieved_callback,
      mojo_base::mojom::AbslStatusCode retrieved_status,
      std::optional<
          std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>>
          shared_credentials_mojom);
  void OnLocalCredentialsRetrieved(
      nearby::presence::GetLocalCredentialsResultCallback
          on_local_credentials_retrieved_callback,
      mojo_base::mojom::AbslStatusCode retrieved_status,
      std::optional<
          std::vector<ash::nearby::presence::mojom::LocalCredentialPtr>>
          local_credentials_mojom);
  void OnLocalCredentialUpdated(nearby::presence::SaveCredentialsResultCallback
                                    on_local_credential_updated_callback,
                                mojo_base::mojom::AbslStatusCode update_status);

  const mojo::SharedRemote<
      ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
      nearby_presence_credential_storage_;
  base::WeakPtrFactory<CredentialStorage> weak_ptr_factory_{this};
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_CREDENTIAL_STORAGE_H_
