// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_WIFI_SYNC_FEATURE_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_WIFI_SYNC_FEATURE_MANAGER_IMPL_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/wifi_sync_feature_manager.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

namespace multidevice_setup {

// Concrete WifiSyncFeatureManager implementation, which utilizes
// DeviceSyncClient to communicate with the back-end.
//
// This toggles WIFI_SYNC_HOST between enabled/supported on cryptauth for a
// synced phone, where supported is considered disabled by user.
//
// Toggling WIFI_SYNC_HOST is a global action, so it will be reflected on all
// synced devices.
class WifiSyncFeatureManagerImpl
    : public WifiSyncFeatureManager,
      public HostStatusProvider::Observer,
      public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<WifiSyncFeatureManager> Create(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<WifiSyncFeatureManager> CreateInstance(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~WifiSyncFeatureManagerImpl() override;
  WifiSyncFeatureManagerImpl(const WifiSyncFeatureManagerImpl&) = delete;
  WifiSyncFeatureManagerImpl& operator=(const WifiSyncFeatureManagerImpl&) =
      delete;

 private:
  WifiSyncFeatureManagerImpl(HostStatusProvider* host_status_provider,
                             PrefService* pref_service,
                             device_sync::DeviceSyncClient* device_sync_client,
                             std::unique_ptr<base::OneShotTimer> timer);

  // HostStatusProvider::Observer,
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  // DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  // WifiSyncFeatureManager:

  // Attempts to enable/disable WIFI_SYNC_HOST on the backend for the host
  // device that is synced at the time SetIsWifiSyncEnabled  is called.
  //
  // If a the request fails (e.g., the device is offline or the server is down),
  // the OnBackendRequestFailed() observer function is invoked, but this object
  // continues to attempt the request until one of the following happens: the
  // request succeeds, SetIsWifiSyncEnabled() called is with different value, or
  // the synced host device changes.
  //
  // If there is already a pending request and this function is called with the
  // same request, a retry will be attempted immediately.
  void SetIsWifiSyncEnabled(bool enabled) override;

  // Returns whether WIFI_SYNC_HOST is enabled/disabled. If there is a pending
  // request to enable or disable WIFI_SYNC_HOST, the state that the pending
  // request is intending to set WIFI_SYNC_HOST to is returned, otherwise the
  // state on the back-end is returned.
  bool IsWifiSyncEnabled() override;

  // Numerical values cannot be changed because they map to integers that are
  // stored persistently in prefs.
  enum class PendingState {
    kPendingNone = 0,
    kPendingEnable = 1,
    kPendingDisable = 2
  };

  enum class CurrentState {
    kNoVerifiedHost,
    kNoPendingRequest,
    kPendingMatchesBackend,
    kValidPendingRequest
  };

  void ResetPendingWifiSyncHostNetworkRequest();
  PendingState GetPendingState();
  CurrentState GetCurrentState();
  void SetPendingWifiSyncHostNetworkRequest(PendingState pending_state);
  void AttemptSetWifiSyncHostStateNetworkRequest(bool is_retry);
  void OnSetWifiSyncHostStateNetworkRequestFinished(
      bool attempted_to_enable,
      device_sync::mojom::NetworkRequestResult result_code);

  HostStatusProvider* host_status_provider_;
  PrefService* pref_service_;
  device_sync::DeviceSyncClient* device_sync_client_;
  std::unique_ptr<base::OneShotTimer> timer_;

  bool network_request_in_flight_ = false;

  base::WeakPtrFactory<WifiSyncFeatureManagerImpl> weak_ptr_factory_{this};
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_WIFI_SYNC_FEATURE_MANAGER_IMPL_H_
