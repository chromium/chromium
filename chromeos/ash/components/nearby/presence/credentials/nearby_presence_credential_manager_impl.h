// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "chromeos/ash/components/nearby/presence/metrics/nearby_presence_metrics.h"
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
class ListSharedCredentialsResponse;
}  // namespace ash::nearby::proto

namespace nearby::internal {
class SharedCredential;
}  // namespace nearby::internal

namespace ash::nearby::presence {

class LocalDeviceDataProvider;
class NearbyPresenceServerClient;

// This class is a singleton, and callers can only create one instance via
// the Creator.
class NearbyPresenceCredentialManagerImpl
    : public NearbyPresenceCredentialManager {
 public:
  // A creator class for building a CredentialManager instance. The `Create`
  // function is async in order to ensure a ready CredentialManager is returned
  // to callers. A ready CredentialManager can be created by one of two flows:
  //
  // Case 1: First Time Registration (the case where this is the first time
  // Nearby Presence is being run on this ChromeOS device). Before the
  // CredentialManager is returned to callers, it completes the first time
  // server registration flow to register with the Nearby Presence server,
  // generate and upload local device credentials, and download remote devices'
  // credentials.
  //
  // Case 2: Other cases (most common path): Before the CredentialManager is
  // returned to callers, it sets the device metadata in the NP library over
  // the mojo pipe.
  //
  // This class expects and enforces that it will only be used once to create a
  // single CredentialManager instance during its lifetime.
  class Creator {
   public:
    using CreateCallback = base::OnceCallback<void(
        std::unique_ptr<NearbyPresenceCredentialManager>)>;

    virtual ~Creator();
    Creator(Creator&) = delete;
    Creator& operator=(Creator&) = delete;

    static Creator* Get();
    static void SetNextCredentialManagerInstanceForTesting(
        std::unique_ptr<NearbyPresenceCredentialManager> credential_manager);

    void Create(
        PrefService* pref_service,
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence,
        CreateCallback on_created);

   protected:
    Creator();

    // For unit tests only. |local_device_data_provider| parameter is used to
    // inject a FakeLocalDeviceDataProvider.
    virtual void Create(
        PrefService* pref_service,
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence,
        std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider,
        CreateCallback on_created);

    bool has_credential_manager_been_created_ = false;

   private:
    friend class base::NoDestructor<Creator>;

    void OnCredentialManagerRegistered(bool success);
    void OnCredentialManagerInitialized();

    // While a new CredentialManager is being initialized the factory retains a
    // reference to it. After initialization is complete |on_created_|
    // is run. If |credential_manager_under_initialization_| is set before the
    // `Create` function for testing using `SetCredentialManagerForTesting`,
    // then it will be returned on the callback, skipping all of the
    // initialization flow, as long as the
    // `CredentialManager::IsLocalDeviceRegistered` returns true.
    std::unique_ptr<NearbyPresenceCredentialManager>
        credential_manager_under_initialization_;

    CreateCallback on_created_;
  };

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
  void InitializeDeviceMetadata(
      base::OnceClosure on_metadata_initialized_callback) override;

 protected:
  NearbyPresenceCredentialManagerImpl(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const mojo::SharedRemote<mojom::NearbyPresence>& nearby_presence,
      std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider);

 private:
  void StartFirstTimeRegistration();
  void OnFirstTimeRegistrationComplete(
      metrics::FirstTimeRegistrationResult result);

  // Callbacks for server registration UpdateDevice RPC via
  // |RegisterPresence|.
  void HandleFirstTimeRegistrationTimeout();
  void HandleFirstTimeRegistrationFailure(ash::nearby::NearbyHttpResult result);
  void OnRegistrationRpcSuccess(
      base::TimeTicks registration_request_start_time,
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
      mojo_base::mojom::AbslStatusCode status);

  // Callback for first time remote credential saving in the NP library.
  void OnFirstTimeRemoteCredentialsSaved(
      mojo_base::mojom::AbslStatusCode status);

  // Callbacks for credential upload/download during first time
  // server registration and daily syncs.
  void OnFirstTimeCredentialsUpload(bool success);
  void OnFirstTimeCredentialsDownload(
      std::vector<::nearby::internal::SharedCredential> credentials,
      bool success);

  void StartDailySync();

  // Callback for fetching local device credentials from the NP library during
  // daily syncs.
  void OnGetLocalSharedCredentials(
      std::vector<mojom::SharedCredentialPtr> shared_credentials,
      mojo_base::mojom::AbslStatusCode status);

  // Callbacks for uploading/downloading credentials as part of the
  // daily syncs.
  void OnDailySyncCredentialUpload(bool success);
  void OnDailySyncCredentialDownload(
      std::vector<::nearby::internal::SharedCredential> credentials,
      bool success);

  // Callback for remote credential saving in the NP library that is part of
  // the daily sync flow.
  void OnDailySyncRemoteCredentialsSaved(
      mojo_base::mojom::AbslStatusCode status);

  // Helper functions to trigger uploading credentials in the NP server. The
  // helper functions are used for first time server registration to upload
  // newly generated credentials, daily syncs with the server to upload
  // credentials if they have changed. These helper functions are also used in
  // `UpdateCredentials` flows.
  //
  // They take a repeating callback because `UploadCredentials()` and
  // `DownloadCredentials()` must be bound as a RepeatingCallback itself as a
  // task in a NearbyScheduler.
  void ScheduleUploadCredentials(
      std::vector<::nearby::internal::SharedCredential>
          proto_shared_credentials,
      base::RepeatingCallback<void(bool)> on_upload);
  void ScheduleDownloadCredentials(
      base::RepeatingCallback<
          void(std::vector<::nearby::internal::SharedCredential>, bool)>
          on_download);
  void UploadCredentials(
      std::vector<::nearby::internal::SharedCredential> credentials,
      base::RepeatingCallback<void(bool)> upload_credentials_result_callback);
  void HandleUploadCredentialsResult(
      base::RepeatingCallback<void(bool)> upload_credentials_callback,
      ash::nearby::NearbyHttpResult result);
  void OnUploadCredentialsTimeout(
      base::RepeatingCallback<void(bool)> upload_credentials_callback);
  void OnUploadCredentialsSuccess(
      base::RepeatingCallback<void(bool)> upload_credentials_callback,
      base::TimeTicks upload_request_start_time,
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
      ash::nearby::NearbyHttpResult result,
      std::vector<::nearby::internal::SharedCredential> credentials);
  void OnDownloadCredentialsTimeout(
      base::RepeatingCallback<
          void(std::vector<::nearby::internal::SharedCredential>, bool)>
          download_credentials_result_callback);
  void OnDownloadCredentialsSuccess(
      base::RepeatingCallback<
          void(std::vector<::nearby::internal::SharedCredential>, bool)>
          download_credentials_result_callback,
      base::TimeTicks download_request_start_time,
      const ash::nearby::proto::ListSharedCredentialsResponse& response);
  void OnDownloadCredentialsFailure(
      base::RepeatingCallback<
          void(std::vector<::nearby::internal::SharedCredential>, bool)>
          download_credentials_result_callback,
      ash::nearby::NearbyHttpError error);

  const raw_ptr<PrefService> pref_service_ = nullptr;
  const raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // Constructed per RPC request, and destroyed on RPC response (server
  // interaction completed). This field is reused by multiple RPCs during the
  // lifetime of the NearbyPresenceCredentialManagerImpl object.
  std::unique_ptr<NearbyPresenceServerClient> server_client_;

  std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider_;

  base::OneShotTimer server_response_timer_;
  const raw_ref<const mojo::SharedRemote<mojom::NearbyPresence>>
      nearby_presence_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Schedulers used to schedule immediate tasks to communicate with the
  // server during the first time registration flow and daily credential sync
  // flow.
  std::unique_ptr<ash::nearby::NearbyScheduler> upload_on_demand_scheduler_;
  std::unique_ptr<ash::nearby::NearbyScheduler> download_on_demand_scheduler_;

  // Stores the number of current attempts for uploading credentials to the
  // server. Increments on each attempt to upload credentials, and is reset to 0
  // once the upload request is completed (either resulting in final
  // success or failure).
  int upload_credentials_attempts_needed_count_ = 0;

  // Stores the number of current attempts for downloading credentials from the
  // server. Increments on each attempt to download credentials, and is reset to
  // 0 once the download request is completed (either resulting in final
  // success or failure).
  int download_credentials_attempts_needed_count_ = 0;

  // Stores the number of current attempts for registered the local device with
  // the Nearby Presence server. Increments on each attempt, and is reset to 0
  // once the registration request is completed (either resulting in final
  // success or failure).
  int first_time_server_registration_attempts_needed_count_ = 0;

  // Initialized during the first time registration flow kicked off in
  // `RegisterPresence()`. Not expected to be a valid pointer unless used during
  // the first time registration flow.
  std::unique_ptr<ash::nearby::NearbyScheduler>
      first_time_registration_on_demand_scheduler_;

  // Scheduler used for daily credential syncs with the server. Every 24 hours,
  // attempt to upload the local device's credentials and download/save
  // remote devices' credentials.
  std::unique_ptr<ash::nearby::NearbyScheduler>
      daily_credential_sync_scheduler_;

  bool is_daily_sync_in_progress_ = false;

  // Stores the last success time of a daily sync to prevent slamming the
  // server with requests to `UpdateCredentials()`.
  std::optional<base::Time> last_daily_sync_success_time_;

  // Stores a count of the number of requests to `UpdateCredentials()` made
  // to match with a corresponding cool off period in between requests to
  // prevent overwhelming the server.
  int update_credential_request_count_ = 0;

  // Callback to return the result of the first time registration. Not
  // guaranteed to be a valid callback, as this is set only during first time
  // registration flow via |RegisterPresence|.
  base::OnceCallback<void(bool)> on_registered_callback_;

  base::WeakPtrFactory<NearbyPresenceCredentialManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_NEARBY_PRESENCE_CREDENTIAL_MANAGER_IMPL_H_
