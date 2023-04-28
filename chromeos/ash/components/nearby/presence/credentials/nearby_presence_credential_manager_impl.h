// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash::nearby {
class NearbyScheduler;
}  // namespace ash::nearby

namespace ash::nearby::proto {
class UpdateDeviceResponse;
}  // namespace ash::nearby::proto

namespace ash::nearby::presence {

class LocalDeviceDataProvider;
class NearbyPresenceServerClient;

class NearbyPresenceCredentialManagerImpl
    : public NearbyPresenceCredentialManager {
 public:
  NearbyPresenceCredentialManagerImpl(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  NearbyPresenceCredentialManagerImpl(NearbyPresenceCredentialManagerImpl&) =
      delete;
  NearbyPresenceCredentialManagerImpl& operator=(
      NearbyPresenceCredentialManagerImpl&) = delete;
  ~NearbyPresenceCredentialManagerImpl() override;

  // NearbyPresenceCredentialManager:
  bool IsLocalDeviceRegistered() override;
  void RegisterPresence(
      base::OnceCallback<void(bool)> on_registered_callback) override;
  void UpdateCredentials() override;

 protected:
  // For unit tests only. |local_device_data_provider| parameter is used to
  // inject a FakeLocalDeviceDataProvider.
  NearbyPresenceCredentialManagerImpl(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider);

 private:
  void StartFirstTimeRegistration();

  // Callbacks for server registration UpdateDevice RPC via
  // |RegisterPresence|.
  void HandleFirstTimeRegistrationFailure();
  void OnRegistrationRpcSuccess(
      const ash::nearby::proto::UpdateDeviceResponse& response);
  void OnRegistrationRpcFailure(ash::nearby::NearbyHttpError error);

  // Constructed per RPC request, and destroyed on RPC response (server
  // interaction completed). This field is reused by multiple RPCs during the
  // lifetime of the NearbyPresenceCredentialManagerImpl object.
  std::unique_ptr<NearbyPresenceServerClient> server_client_;

  std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider_;

  const raw_ptr<PrefService> pref_service_ = nullptr;
  const raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  base::OneShotTimer server_response_timer_;
  std::unique_ptr<NearbyScheduler> first_time_registration_on_demand_scheduler_;

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Callback to return the result of the first time registration. Not
  // guaranteed to be a valid callback, as this is set only during first time
  // registration flow via |RegisterPresence|.
  base::OnceCallback<void(bool)> on_registered_callback_;

  base::WeakPtrFactory<NearbyPresenceCredentialManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_
