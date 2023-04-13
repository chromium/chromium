// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager.h"

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::nearby::presence {

class NearbyPresenceCredentialManagerImpl
    : public NearbyPresenceCredentialManager {
 public:
  NearbyPresenceCredentialManagerImpl(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager);
  ~NearbyPresenceCredentialManagerImpl() override;

  NearbyPresenceCredentialManagerImpl(NearbyPresenceCredentialManagerImpl&) =
      delete;
  NearbyPresenceCredentialManagerImpl& operator=(
      NearbyPresenceCredentialManagerImpl&) = delete;

  // NearbyPresenceCredentialManager:
  bool IsLocalDeviceRegistered() override;
  void RegisterPresence(
      base::OnceCallback<void(bool)> on_registered_callback) override;
  void UpdateCredentials() override;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_
