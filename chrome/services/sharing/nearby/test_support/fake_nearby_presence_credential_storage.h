// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_NEARBY_PRESENCE_CREDENTIAL_STORAGE_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_NEARBY_PRESENCE_CREDENTIAL_STORAGE_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::nearby::presence {

// An empty implementation of `mojom::NearbyPresenceCredentialStorage` for
// testing.
class FakeNearbyPresenceCredentialStorage
    : public ash::nearby::presence::mojom::NearbyPresenceCredentialStorage {
 public:
  FakeNearbyPresenceCredentialStorage();
  ~FakeNearbyPresenceCredentialStorage() override;

  // mojom::NearbyPresenceCredentialStorage:
  void SaveCredentials(
      std::vector<ash::nearby::presence::mojom::LocalCredentialPtr>
          local_credentials,
      std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
          shared_credentials,
      ash::nearby::presence::mojom::PublicCredentialType public_credential_type,
      ash::nearby::presence::mojom::NearbyPresenceCredentialStorage::
          SaveCredentialsCallback callback) override {}
  void GetPublicCredentials(
      ash::nearby::presence::mojom::PublicCredentialType public_credential_type,
      GetPublicCredentialsCallback callback) override {}
  void GetPrivateCredentials(GetPrivateCredentialsCallback callback) override {}
  void UpdateLocalCredential(
      ash::nearby::presence::mojom::LocalCredentialPtr local_credential,
      UpdateLocalCredentialCallback callback) override {}

  mojo::Receiver<mojom::NearbyPresenceCredentialStorage>& receiver() {
    return receiver_;
  }

 private:
  mojo::Receiver<mojom::NearbyPresenceCredentialStorage> receiver_{this};
};

}  // namespace ash::nearby::presence

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_NEARBY_PRESENCE_CREDENTIAL_STORAGE_H_
