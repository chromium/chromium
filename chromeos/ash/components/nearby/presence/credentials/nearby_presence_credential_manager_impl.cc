// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"

#include "base/functional/callback.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace ash::nearby::presence {

NearbyPresenceCredentialManagerImpl::NearbyPresenceCredentialManagerImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      local_device_data_provider_(std::move(local_device_data_provider)) {
  // TODO(b/276307539): Add mojo remote as a parameter once implemented.
  CHECK(pref_service_);
  CHECK(identity_manager_);
  CHECK(local_device_data_provider_);
}

NearbyPresenceCredentialManagerImpl::NearbyPresenceCredentialManagerImpl(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : pref_service_(pref_service), identity_manager_(identity_manager) {
  // TODO (b/276307539): Add mojo remote as a parameter once implemented.
  CHECK(pref_service_);
  CHECK(identity_manager_);

  local_device_data_provider_ = std::make_unique<LocalDeviceDataProviderImpl>(
      pref_service, identity_manager);
}

NearbyPresenceCredentialManagerImpl::~NearbyPresenceCredentialManagerImpl() =
    default;

bool NearbyPresenceCredentialManagerImpl::IsLocalDeviceRegistered() {
  return local_device_data_provider_->IsUserRegistrationInfoSaved();
}

void NearbyPresenceCredentialManagerImpl::RegisterPresence(
    base::OnceCallback<void(bool)> on_registered_callback) {
  // TODO (b/276307539): Implement `RegisterPresence`.
}

void NearbyPresenceCredentialManagerImpl::UpdateCredentials() {
  // TODO (b/276307539): Implement `UpdateCredentials`.
}

}  // namespace ash::nearby::presence
