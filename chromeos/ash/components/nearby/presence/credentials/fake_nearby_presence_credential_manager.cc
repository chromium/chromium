// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/fake_nearby_presence_credential_manager.h"

namespace ash::nearby::presence {

FakeNearbyPresenceCredentialManager::FakeNearbyPresenceCredentialManager() =
    default;
FakeNearbyPresenceCredentialManager::~FakeNearbyPresenceCredentialManager() =
    default;

bool FakeNearbyPresenceCredentialManager::IsLocalDeviceRegistered() {
  return is_registered_;
}

// Not implemented.
void FakeNearbyPresenceCredentialManager::RegisterPresence(
    base::OnceCallback<void(bool)> on_registered_callback) {}

void FakeNearbyPresenceCredentialManager::UpdateCredentials() {
  was_update_credentials_called_ = true;
}

// Not implemented.
void FakeNearbyPresenceCredentialManager::InitializeDeviceMetadata(
    base::OnceClosure on_metadata_initialized_callback) {}

}  // namespace ash::nearby::presence
