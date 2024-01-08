// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_MANAGER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_manager.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/sync_scheduler.h"

class PrefService;

namespace ash {

namespace device_sync {

class CryptAuthClient;
class CryptAuthClientFactory;

class CryptAuthDeviceManagerImpl : public CryptAuthDeviceManager,
                                   public SyncScheduler::Delegate,
                                   public CryptAuthGCMManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthDeviceManager> Create(
        base::Clock* clock,
        CryptAuthClientFactory* cryptauth_client_factory,
        CryptAuthGCMManager* gcm_manager,
        PrefService* pref_service);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthDeviceManager> CreateInstance(
        base::Clock* clock,
        CryptAuthClientFactory* cryptauth_client_factory,
        CryptAuthGCMManager* gcm_manager,
        PrefService* pref_service) = 0;

   private:
    static Factory* factory_instance_;
  };

  CryptAuthDeviceManagerImpl(const CryptAuthDeviceManagerImpl&) = delete;
  CryptAuthDeviceManagerImpl& operator=(const CryptAuthDeviceManagerImpl&) =
      delete;

  ~CryptAuthDeviceManagerImpl() override;

  // CryptAuthDeviceManager:
  void Start() override;
  void ForceSyncNow(cryptauth::InvocationReason invocation_reason) override;
  base::Time GetLastSyncTime() const override;
  base::TimeDelta GetTimeToNextAttempt() const override;
  bool IsSyncInProgress() const override;
  bool IsRecoveringFromFailure() const override;
  std::vector<cryptauth::ExternalDeviceInfo> GetSyncedDevices() const override;
  std::vector<cryptauth::ExternalDeviceInfo> GetUnlockKeys() const override;
  std::vector<cryptauth::ExternalDeviceInfo> GetPixelUnlockKeys()
      const override;
  std::vector<cryptauth::ExternalDeviceInfo> GetTetherHosts() const override;
  std::vector<cryptauth::ExternalDeviceInfo> GetPixelTetherHosts()
      const override;

 protected:
  // Creates the manager:
  // |clock|: Used to determine the time between sync attempts.
  // |cryptauth_client_factory|: Creates CryptAuthClient instances to perform
  // each sync. |gcm_manager|: Notifies when GCM push messages trigger device
  // syncs.
  //                Not owned and must outlive this instance.
  // |pref_service|: Stores syncing metadata and unlock key information to
  //                 persist across browser restarts. Must already be registered
  //                 with RegisterPrefs().
  CryptAuthDeviceManagerImpl(base::Clock* clock,
                             CryptAuthClientFactory* cryptauth_client_factory,
                             CryptAuthGCMManager* gcm_manager,
                             PrefService* pref_service);

  void SetSyncSchedulerForTest(std::unique_ptr<SyncScheduler> sync_scheduler);

 private:
  // CryptAuthGCMManager::Observer:
  void OnResyncMessage(
      const std::optional<std::string>& session_id,
      const std::optional<CryptAuthFeatureType>& feature_type) override;

  // Updates |unlock_keys_| by fetching the list stored in |pref_service_|.
  void UpdateUnlockKeysFromPrefs();

  // SyncScheduler::Delegate:
  void OnSyncRequested(
      std::unique_ptr<SyncScheduler::SyncRequest> sync_request) override;

  // Callback when |cryptauth_client_| completes with the response.
  void OnGetMyDevicesSuccess(const cryptauth::GetMyDevicesResponse& response);
  void OnGetMyDevicesFailure(NetworkRequestError error);

  // Used to determine the time.
  raw_ptr<base::Clock> clock_;

  // Creates CryptAuthClient instances for each sync attempt.
  raw_ptr<CryptAuthClientFactory> cryptauth_client_factory_;

  // Notifies when GCM push messages trigger device sync. Not owned and must
  // outlive this instance.
  raw_ptr<CryptAuthGCMManager> gcm_manager_;

  // Contains preferences that outlive the lifetime of this object and across
  // process restarts. |pref_service_| must outlive the lifetime of this
  // instance.
  const raw_ptr<PrefService> pref_service_;

  // All devices currently synced from CryptAuth.
  std::vector<cryptauth::ExternalDeviceInfo> synced_devices_;

  // Schedules the time between device sync attempts.
  std::unique_ptr<SyncScheduler> scheduler_;

  // Contains the SyncRequest that |scheduler_| requests when a device sync
  // attempt is made.
  std::unique_ptr<SyncScheduler::SyncRequest> sync_request_;

  // The CryptAuthEnroller instance for the current sync attempt. A new
  // instance will be created for each individual attempt.
  std::unique_ptr<CryptAuthClient> cryptauth_client_;

  base::WeakPtrFactory<CryptAuthDeviceManagerImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_MANAGER_IMPL_H_
