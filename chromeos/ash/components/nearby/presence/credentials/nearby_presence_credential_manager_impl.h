// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

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
class ListPublicCertificatesResponse;
}  // namespace ash::nearby::proto

namespace nearby::internal {
class SharedCredential;
}  // namespace nearby::internal

namespace ash::nearby::presence {

class LocalDeviceDataProvider;
class NearbyPresenceServerClient;

class NearbyPresenceCredentialManagerImpl
    : public NearbyPresenceCredentialManager {
 public:
  NearbyPresenceCredentialManagerImpl(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence);

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
      const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence,
      std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider);

 private:
  void StartFirstTimeRegistration();

  // Callbacks for server registration UpdateDevice RPC via
  // |RegisterPresence|.
  void HandleFirstTimeRegistrationTimeout();
  void HandleFirstTimeRegistrationFailure();
  void OnRegistrationRpcSuccess(
      const ash::nearby::proto::UpdateDeviceResponse& response);
  void OnRegistrationRpcFailure(ash::nearby::NearbyHttpError error);

  // Callback for credential generation in the NP library during the first
  // time registration flow.
  //
  // TODO(b/286594539): Revisit this flow when there are additional triggers to
  // regenerate credentials outside the first time flow stemming from metadata
  // changes.
  void OnFirstTimeCredentialsGenerated(
      std::vector<mojom::SharedCredentialPtr> shared_credentials,
      mojom::StatusCode status);

  // Callbacks for first time credential upload/download during first time
  // server registration.
  void ScheduleUploadCredentials(
      std::vector<::nearby::internal::SharedCredential>
          proto_shared_credentials);
  void OnFirstTimeCredentialsUpload(bool success);
  void ScheduleDownloadCredentials();
  void OnFirstTimeCredentialsDownload(
      std::vector<::nearby::internal::SharedCredential> credentials,
      bool success);

  // Helper functions to trigger uploading and downloading credentials in the NP
  // server. The helper functions are used for first time server registration to
  // upload newly generated credentials and download remote devices'
  // credentials, as well as daily syncs with the server to upload
  // credentials if they have changed and download remote devices' credentials.
  // These helper functions are also used in `UpdateCredentials` flows.
  //
  // They take a repeating callback because `UploadCredentials()` and
  // `DownloadCredentials()` must be bound as a RepeatingCallback itself as a
  // task in a NearbyScheduler.
  void UploadCredentials(
      std::vector<::nearby::internal::SharedCredential> credentials,
      base::RepeatingCallback<void(bool)> upload_credentials_result_callback);
  void HandleUploadCredentialsResult(
      base::RepeatingCallback<void(bool)> upload_credentials_callback,
      bool success);
  void OnUploadCredentialsTimeout(
      base::RepeatingCallback<void(bool)> upload_credentials_callback);
  void OnUploadCredentialsSuccess(
      base::RepeatingCallback<void(bool)> upload_credentials_callback,
      const ash::nearby::proto::UpdateDeviceResponse& response);
  void OnUploadCredentialsFailure(
      base::RepeatingCallback<void(bool)> upload_credentials_callback,
      ash::nearby::NearbyHttpError error);
  void DownloadCredentials(
      base::RepeatingCallback<
          void(std::vector<::nearby::internal::SharedCredential>, bool)>
          download_credentials_result_callback);
  void HandleDownloadCredentialsResult(
      base::RepeatingCallback<
          void(std::vector<::nearby::internal::SharedCredential>, bool)>
          download_credentials_result_callback,
      bool success,
      std::vector<::nearby::internal::SharedCredential> credentials);
  void OnDownloadCredentialsTimeout(
      base::RepeatingCallback<
          void(std::vector<::nearby::internal::SharedCredential>, bool)>
          download_credentials_result_callback);
  void OnDownloadCredentialsSuccess(
      base::RepeatingCallback<
          void(std::vector<::nearby::internal::SharedCredential>, bool)>
          download_credentials_result_callback,
      const ash::nearby::proto::ListPublicCertificatesResponse& response);
  void OnDownloadCredentialsFailure(
      base::RepeatingCallback<
          void(std::vector<::nearby::internal::SharedCredential>, bool)>
          download_credentials_result_callback,
      ash::nearby::NearbyHttpError error);

  // Constructed per RPC request, and destroyed on RPC response (server
  // interaction completed). This field is reused by multiple RPCs during the
  // lifetime of the NearbyPresenceCredentialManagerImpl object.
  std::unique_ptr<NearbyPresenceServerClient> server_client_;

  std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider_;
  const raw_ptr<PrefService> pref_service_ = nullptr;
  const raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  base::OneShotTimer server_response_timer_;
  const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Schedulers used to schedule immediate tasks to communicate with the
  // server during the first time registration flow. Initialized during the
  // first time registration flow kicked off in `RegisterPresence()`. Not
  // expected to be a valid pointer unless used during the first time
  // registration flow.
  std::unique_ptr<ash::nearby::NearbyScheduler>
      first_time_registration_on_demand_scheduler_;
  std::unique_ptr<ash::nearby::NearbyScheduler>
      first_time_upload_on_demand_scheduler_;
  std::unique_ptr<ash::nearby::NearbyScheduler>
      first_time_download_on_demand_scheduler_;

  // Callback to return the result of the first time registration. Not
  // guaranteed to be a valid callback, as this is set only during first time
  // registration flow via |RegisterPresence|.
  base::OnceCallback<void(bool)> on_registered_callback_;

  base::WeakPtrFactory<NearbyPresenceCredentialManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_
