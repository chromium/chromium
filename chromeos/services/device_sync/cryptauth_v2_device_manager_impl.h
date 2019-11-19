// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_DEVICE_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_DEVICE_MANAGER_IMPL_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_device_registry.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/cryptauth_scheduler.h"
#include "chromeos/services/device_sync/cryptauth_v2_device_manager.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

class ClientAppMetadataProvider;
class CryptAuthClientFactory;
class CryptAuthDeviceSyncer;
class CryptAuthKeyRegistry;

// Implementation of CryptAuthV2DeviceManager that considers three sources of
// DeviceSync requests:
//  1) The scheduler requests a DeviceSync to recover from a failed attempt or
//     after receiving an InvokeNext instruction from CryptAuth in a
//     ClientDirective.
//  2) The device manager listens to the GCM manager for re-sync requests.
//  3) The ForceDeviceSyncNow() method allows for immediate requests.
class CryptAuthV2DeviceManagerImpl
    : public CryptAuthV2DeviceManager,
      public CryptAuthScheduler::DeviceSyncDelegate,
      public CryptAuthGCMManager::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthV2DeviceManager> BuildInstance(
        ClientAppMetadataProvider* client_app_metadata_provider,
        CryptAuthDeviceRegistry* device_registry,
        CryptAuthKeyRegistry* key_registry,
        CryptAuthClientFactory* client_factory,
        CryptAuthGCMManager* gcm_manager,
        CryptAuthScheduler* scheduler,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  ~CryptAuthV2DeviceManagerImpl() override;

 protected:
  CryptAuthV2DeviceManagerImpl(
      ClientAppMetadataProvider* client_app_metadata_provider,
      CryptAuthDeviceRegistry* device_registry,
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      CryptAuthGCMManager* gcm_manager,
      CryptAuthScheduler* scheduler,
      std::unique_ptr<base::OneShotTimer> timer);

 private:
  enum class State {
    kIdle,
    kWaitingForClientAppMetadata,
    kWaitingForDeviceSync
  };

  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  // CryptAuthV2DeviceManager:
  void Start() override;
  const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& GetSyncedDevices()
      const override;
  void ForceDeviceSyncNow(
      const cryptauthv2::ClientMetadata::InvocationReason&,
      const base::Optional<std::string>& session_id) override;
  bool IsDeviceSyncInProgress() const override;
  bool IsRecoveringFromFailure() const override;
  base::Optional<base::Time> GetLastDeviceSyncTime() const override;
  base::Optional<base::TimeDelta> GetTimeToNextAttempt() const override;

  // CryptAuthScheduler::DeviceSyncDelegate:
  void OnDeviceSyncRequested(
      const cryptauthv2::ClientMetadata& client_metadata) override;

  // CryptAuthGCMManager::Observer:
  void OnResyncMessage(
      const base::Optional<std::string>& session_id,
      const base::Optional<CryptAuthFeatureType>& feature_type) override;

  void SetState(State state);
  void OnTimeout();

  void OnClientAppMetadataFetched(
      const base::Optional<cryptauthv2::ClientAppMetadata>&
          client_app_metadata);
  void AttemptDeviceSync();
  void OnDeviceSyncFinished(CryptAuthDeviceSyncResult device_sync_result);

  State state_ = State::kIdle;

  base::Optional<cryptauthv2::ClientMetadata> current_client_metadata_;
  base::Optional<cryptauthv2::ClientAppMetadata> client_app_metadata_;
  std::unique_ptr<CryptAuthDeviceSyncer> device_syncer_;

  ClientAppMetadataProvider* client_app_metadata_provider_ = nullptr;
  CryptAuthDeviceRegistry* device_registry_ = nullptr;
  CryptAuthKeyRegistry* key_registry_ = nullptr;
  CryptAuthClientFactory* client_factory_ = nullptr;
  CryptAuthGCMManager* gcm_manager_ = nullptr;
  CryptAuthScheduler* scheduler_ = nullptr;
  std::unique_ptr<base::OneShotTimer> timer_;

  // For weak pointers used in callbacks. These weak pointers are invalidated
  // when the current DeviceSync attempt finishes in order to cancel outstanding
  // callbacks.
  base::WeakPtrFactory<CryptAuthV2DeviceManagerImpl> callback_weak_ptr_factory_{
      this};

  // For sending a weak pointer to the scheduler, whose lifetime exceeds that of
  // CryptAuthV2DeviceManagerImpl.
  base::WeakPtrFactory<CryptAuthV2DeviceManagerImpl>
      scheduler_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CryptAuthV2DeviceManagerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_DEVICE_MANAGER_IMPL_H_
