// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"

#include "base/functional/callback.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace ash::nearby::presence {

NearbyPresenceCredentialManagerImpl::NearbyPresenceCredentialManagerImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager) {
  // TODO (b/276307539): Add mojo remote as a parameter once implemented.
}

NearbyPresenceCredentialManagerImpl::~NearbyPresenceCredentialManagerImpl() =
    default;

bool NearbyPresenceCredentialManagerImpl::IsLocalDeviceRegistered() {
  // TODO (b/276307539): Implement `IsLocalDeviceRegistered`, this
  // default implementation is to get the skeleton class to compile.
  return false;
}

void NearbyPresenceCredentialManagerImpl::RegisterPresence(
    base::OnceCallback<void(bool)> on_registered_callback) {
  // TODO (b/276307539): Implement `RegisterPresence`.
}

void NearbyPresenceCredentialManagerImpl::UpdateCredentials() {
  // TODO (b/276307539): Implement `UpdateCredentials`.
}

}  // namespace ash::nearby::presence
