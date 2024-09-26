// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_IMPL_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_activity_getter.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_manager.h"
#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/ash/services/device_sync/device_sync_base.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "chromeos/ash/services/device_sync/remote_device_provider.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;

namespace base {
class Clock;
class OneShotTimer;
}  // namespace base

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace instance_id {
class InstanceIDDriver;
}  // namespace instance_id

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

namespace device_sync {

class ClientAppMetadataProvider;
class CryptAuthClientFactory;
class CryptAuthDeviceNotifier;
class CryptAuthDeviceRegistry;
class CryptAuthFeatureStatusSetter;
class CryptAuthKeyRegistry;
class CryptAuthScheduler;
class CryptAuthV2DeviceManager;
class GcmDeviceInfoProvider;

// Concrete DeviceSync implementation. When DeviceSyncImpl is constructed, it
// starts an initialization flow with the following steps:
// (1) Check if the primary user is logged in with a valid account ID.
// (2) If not, wait for IdentityManager to provide the primary account ID.
// (3) Register with GCM.
// (4) Fetch ClientAppMetadata (CryptAuth v2 only).
// (5) Instantiate classes which communicate with the CryptAuth back-end.
// (6) Check enrollment state; if not yet enrolled, enroll the device.
// (7) When enrollment is valid, listen for device sync updates.
class DeviceSyncImpl : public DeviceSyncBase,
                       public signin::IdentityManager::Observer,
                       public CryptAuthGCMManager::Observer,
                       public CryptAuthEnrollmentManager::Observer,
                       public RemoteDeviceProvider::Observer {
 public:
  class Factory {
   public:
    // Note: |timer| should be a newly-created base::OneShotTimer object; this
    // parameter only exists for testing via dependency injection.
    static std::unique_ptr<DeviceSyncBase> Create(
        signin::IdentityManager* identity_manager,
        gcm::GCMDriver* gcm_driver,
        instance_id::InstanceIDDriver* instance_id_driver,
        PrefService* profile_prefs,
        const GcmDeviceInfoProvider* gcm_device_info_provider,
        ClientAppMetadataProvider* client_app_metadata_provider,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        std::unique_ptr<base::OneShotTimer> timer,
        AttestationCertificatesSyncer::GetAttestationCertificatesFunction
            get_attestation_certificates_function);
    static void SetCustomFactory(Factory* custom_factory);
    static bool IsCustomFactorySet();

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<DeviceSyncBase> CreateInstance(
        signin::IdentityManager* identity_manager,
        gcm::GCMDriver* gcm_driver,
        instance_id::InstanceIDDriver* instance_id_driver,
        PrefService* profile_prefs,
        const GcmDeviceInfoProvider* gcm_device_info_provider,
        ClientAppMetadataProvider* client_app_metadata_provider,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        std::unique_ptr<base::OneShotTimer> timer,
        AttestationCertificatesSyncer::GetAttestationCertificatesFunction
            get_attestation_certificates_function) = 0;

   private:
    static Factory* custom_factory_instance_;
  };

  DeviceSyncImpl(const DeviceSyncImpl&) = delete;
  DeviceSyncImpl& operator=(const DeviceSyncImpl&) = delete;

  ~DeviceSyncImpl() override;

 protected:
  // device_sync::mojom::DeviceSync:
  void ForceEnrollmentNow(ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(ForceSyncNowCallback callback) override;
  void GetGroupPrivateKeyStatus(
      GetGroupPrivateKeyStatusCallback callback) override;
  void GetBetterTogetherMetadataStatus(
      GetBetterTogetherMetadataStatusCallback callback) override;
  void GetLocalDeviceMetadata(GetLocalDeviceMetadataCallback callback) override;
  void GetSyncedDevices(GetSyncedDevicesCallback callback) override;
  void SetSoftwareFeatureState(
      const std::string& device_public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      SetSoftwareFeatureStateCallback callback) override;
  void SetFeatureStatus(const std::string& device_instance_id,
                        multidevice::SoftwareFeature feature,
                        FeatureStatusChange status_change,
                        SetFeatureStatusCallback callback) override;
  void FindEligibleDevices(multidevice::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void NotifyDevices(const std::vector<std::string>& device_instance_ids,
                     cryptauthv2::TargetService target_service,
                     multidevice::SoftwareFeature feature,
                     NotifyDevicesCallback callback) override;
  void GetDevicesActivityStatus(
      GetDevicesActivityStatusCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;

  // CryptAuthGcmManager::Observer:
  void OnGCMRegistrationResult(bool success) override;

  // CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentFinished(bool success) override;

  // RemoteDeviceProvider::Observer:
  void OnSyncDeviceListChanged() override;

 private:
  friend class DeviceSyncServiceTest;

  enum class InitializationStatus {
    kNotStarted,
    kFetchingAccountInfo,
    kWaitingForGcmRegistration,
    kWaitingForClientAppMetadata,
    kWaitingForEnrollment,
    kReady
  };

  class PendingSetSoftwareFeatureRequest {
   public:
    PendingSetSoftwareFeatureRequest(
        const std::string& device_public_key,
        multidevice::SoftwareFeature software_feature,
        bool enabled,
        RemoteDeviceProvider* remote_device_provider,
        SetSoftwareFeatureStateCallback callback);
    ~PendingSetSoftwareFeatureRequest();

    // Whether the request has been fulfilled (i.e., whether the requested
    // feature has been set according to the parameters used).
    bool IsFulfilled() const;

    void InvokeCallback(mojom::NetworkRequestResult result);

    multidevice::SoftwareFeature software_feature() const {
      return software_feature_;
    }

    bool enabled() const { return enabled_; }

   private:
    std::string device_public_key_;
    multidevice::SoftwareFeature software_feature_;
    bool enabled_;
    raw_ptr<RemoteDeviceProvider> remote_device_provider_;
    SetSoftwareFeatureStateCallback callback_;
  };

  class PendingSetFeatureStatusRequest {
   public:
    PendingSetFeatureStatusRequest(
        const std::string& device_instance_id,
        multidevice::SoftwareFeature software_feature,
        FeatureStatusChange status_change,
        RemoteDeviceProvider* remote_device_provider,
        SetFeatureStatusCallback callback);
    ~PendingSetFeatureStatusRequest();

    // True if the device and software feature status specified in the request
    // agrees with the device data returned by CryptAuth.
    bool IsFulfilled() const;

    void InvokeCallback(mojom::NetworkRequestResult result);

   private:
    std::string device_instance_id_;
    multidevice::SoftwareFeature software_feature_;
    FeatureStatusChange status_change_;
    raw_ptr<RemoteDeviceProvider> remote_device_provider_;
    SetFeatureStatusCallback callback_;
  };

  DeviceSyncImpl(
      signin::IdentityManager* identity_manager,
      gcm::GCMDriver* gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver,
      PrefService* profile_prefs,
      const GcmDeviceInfoProvider* gcm_device_info_provider,
      ClientAppMetadataProvider* client_app_metadata_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::Clock* clock,
      std::unique_ptr<base::OneShotTimer> timer,
      AttestationCertificatesSyncer::GetAttestationCertificatesFunction
          get_attestation_certificates_function);

  // DeviceSyncBase:
  void Shutdown() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  // Initialization functions.
  void RunNextInitializationStep();
  void FetchAccountInfo();
  void ProcessPrimaryAccountInfo(const CoreAccountInfo& primary_account_info);
  void RegisterWithGcm();
  void FetchClientAppMetadata();
  void OnClientAppMetadataFetchTimeout();
  void OnClientAppMetadataFetched(
      const std::optional<cryptauthv2::ClientAppMetadata>& client_app_metadata);
  void OnClientAppMetadataFetchFailure();
  void WaitForValidEnrollment();
  void InitializeCryptAuthManagementObjects();
  void CompleteInitializationAfterSuccessfulEnrollment();

  std::optional<multidevice::RemoteDevice> GetSyncedDeviceWithPublicKey(
      const std::string& public_key) const;

  void OnSetSoftwareFeatureStateSuccess();
  void OnSetSoftwareFeatureStateError(const base::UnguessableToken& request_id,
                                      NetworkRequestError error);
  void OnSetFeatureStatusSuccess();
  void OnSetFeatureStatusError(const base::UnguessableToken& request_id,
                               NetworkRequestError error);
  void OnFindEligibleDevicesSuccess(
      base::OnceCallback<void(mojom::NetworkRequestResult,
                              mojom::FindEligibleDevicesResponsePtr)> callback,
      const std::vector<cryptauth::ExternalDeviceInfo>& eligible_devices,
      const std::vector<cryptauth::IneligibleDevice>& ineligible_devices);
  void OnFindEligibleDevicesError(
      const base::OnceCallback<void(mojom::NetworkRequestResult,
                                    mojom::FindEligibleDevicesResponsePtr)>
          callback,
      NetworkRequestError error);
  void OnNotifyDevicesSuccess(const base::UnguessableToken& request_id);
  void OnNotifyDevicesError(const base::UnguessableToken& request_id,
                            NetworkRequestError error);
  void OnGetDevicesActivityStatusFinished(
      const base::UnguessableToken& request_id,
      CryptAuthDeviceActivityGetter::DeviceActivityStatusResult
          device_activity_status_result);
  void OnGetDevicesActivityStatusError(
      const base::UnguessableToken& request_id,
      NetworkRequestError network_request_error);

  // Note: If the timer is already running, StartSetSoftwareFeatureTimer()
  // restarts it.
  void StartSetSoftwareFeatureTimer();
  void OnSetSoftwareFeatureTimerFired();

  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<gcm::GCMDriver> gcm_driver_;
  raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<const GcmDeviceInfoProvider> gcm_device_info_provider_;
  raw_ptr<ClientAppMetadataProvider> client_app_metadata_provider_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<base::Clock> clock_;
  std::unique_ptr<base::OneShotTimer> timer_;
  AttestationCertificatesSyncer::GetAttestationCertificatesFunction
      get_attestation_certificates_function_;

  InitializationStatus status_ = InitializationStatus::kNotStarted;
  CoreAccountInfo primary_account_info_;
  base::flat_map<base::UnguessableToken,
                 std::unique_ptr<PendingSetSoftwareFeatureRequest>>
      id_to_pending_set_software_feature_request_map_;
  base::flat_map<base::UnguessableToken,
                 std::unique_ptr<PendingSetFeatureStatusRequest>>
      id_to_pending_set_feature_status_request_map_;
  base::flat_map<base::UnguessableToken, NotifyDevicesCallback>
      pending_notify_devices_callbacks_;
  base::flat_map<base::UnguessableToken, GetDevicesActivityStatusCallback>
      get_devices_activity_status_callbacks_;

  std::optional<cryptauthv2::ClientAppMetadata> client_app_metadata_;
  size_t num_gcm_registration_failures_ = 0;
  size_t num_client_app_metadata_fetch_failures_ = 0;
  base::TimeTicks initialization_start_timestamp_;
  base::TimeTicks gcm_registration_start_timestamp_;
  base::TimeTicks client_app_metadata_fetch_start_timestamp_;

  std::unique_ptr<CryptAuthGCMManager> cryptauth_gcm_manager_;
  std::unique_ptr<CryptAuthClientFactory> cryptauth_client_factory_;

  // Only created and used if v2 Enrollment is enabled; null otherwise.
  std::unique_ptr<CryptAuthKeyRegistry> cryptauth_key_registry_;
  std::unique_ptr<CryptAuthScheduler> cryptauth_scheduler_;

  // Only created and used if v2 DeviceSync is enabled; null otherwise.
  std::unique_ptr<CryptAuthDeviceRegistry> cryptauth_device_registry_;
  std::unique_ptr<CryptAuthV2DeviceManager> cryptauth_v2_device_manager_;
  std::unique_ptr<CryptAuthDeviceNotifier> device_notifier_;
  std::unique_ptr<CryptAuthFeatureStatusSetter> feature_status_setter_;

  std::unique_ptr<CryptAuthEnrollmentManager> cryptauth_enrollment_manager_;
  std::unique_ptr<RemoteDeviceProvider> remote_device_provider_;
  std::unique_ptr<CryptAuthDeviceActivityGetter>
      cryptauth_device_activity_getter_;

  base::WeakPtrFactory<DeviceSyncImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_IMPL_H_
