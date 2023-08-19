// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_FAKE_NEARBY_PRESENCE_CREDENTIAL_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_FAKE_NEARBY_PRESENCE_CREDENTIAL_MANAGER_H_

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager.h"

#include "base/functional/callback.h"

namespace ash::nearby::presence {

// A fake implementation of the Nearby Presence Credential Manager.
// Only use in unit tests.
class FakeNearbyPresenceCredentialManager
    : public NearbyPresenceCredentialManager {
 public:
  FakeNearbyPresenceCredentialManager();
  ~FakeNearbyPresenceCredentialManager() override;

  FakeNearbyPresenceCredentialManager(FakeNearbyPresenceCredentialManager&) =
      delete;
  FakeNearbyPresenceCredentialManager& operator=(
      FakeNearbyPresenceCredentialManager&) = delete;

  // NearbyPresenceCredentialManager:
  bool IsLocalDeviceRegistered() override;
  void RegisterPresence(
      base::OnceCallback<void(bool)> on_registered_callback) override;
  void UpdateCredentials() override;
  void InitializeDeviceMetadata(
      base::OnceClosure on_metadata_initialized_callback) override;

  bool WasUpdateCredentialsCalled() { return was_update_credentials_called_; }

 private:
  bool is_registered_ = true;
  bool was_update_credentials_called_ = false;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_FAKE_NEARBY_PRESENCE_CREDENTIAL_MANAGER_H_
