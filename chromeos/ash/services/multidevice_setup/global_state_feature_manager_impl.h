// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_GLOBAL_STATE_FEATURE_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_GLOBAL_STATE_FEATURE_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace multidevice {
enum class SoftwareFeature;
}

namespace multidevice_setup {

// Concrete GlobalStateFeatureManager implementation, which utilizes
// DeviceSyncClient to communicate with the back-end.
//
// This toggles the managed feature's host state between enabled/supported on
// cryptauth for a synced phone, where the supported state is considered
// disabled by user.
//
// Toggling the feature state is a global action, so it will be reflected on all
// synced devices.
class GlobalStateFeatureManagerImpl
    : public GlobalStateFeatureManager,
      public HostStatusProvider::Observer,
      public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    enum class Option {
      // Corresponds to |mojom::Feature::kWifiSync|.
      kWifiSync
    };

    static std::unique_ptr<GlobalStateFeatureManager> Create(
        Option option,
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<GlobalStateFeatureManager> CreateInstance(
        Option option,
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        device_sync::DeviceSyncClient* device_sync_client,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~GlobalStateFeatureManagerImpl() override;
  GlobalStateFeatureManagerImpl(const GlobalStateFeatureManagerImpl&) = delete;
  GlobalStateFeatureManagerImpl& operator=(
      const GlobalStateFeatureManagerImpl&) = delete;

 private:
  GlobalStateFeatureManagerImpl(
      mojom::Feature managed_feature,
      multidevice::SoftwareFeature managed_host_feature,
      const std::string& pending_state_pref_name,
      HostStatusProvider* host_status_provider,
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      std::unique_ptr<base::OneShotTimer> timer);

  // HostStatusProvider::Observer,
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  // DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  // GlobalStateFeatureManager:

  // Attempts to enable/disable the managed feature on the backend for the host
  // device that is synced at the time SetIsFeatureEnabled is called.
  //
  // If a the request fails (e.g., the device is offline or the server is down),
  // this object will continue to attempt the request until one of the following
  // happens: the
  // request succeeds, SetIsFeatureEnabled() called is with different value, or
  // the synced host device changes.
  //
  // If there is already a pending request and this function is called with the
  // same request, a retry will be attempted immediately.
  void SetIsFeatureEnabled(bool enabled) override;

  // Returns whether the managed feature is enabled/disabled. If there is a
  // pending request to enable or disable the feature, the state that the
  // pending request is intending to set to is returned, otherwise the state on
  // the backend is returned.
  bool IsFeatureEnabled() override;

  // Numerical values cannot be changed because they map to integers that are
  // stored persistently in prefs.
  enum class PendingState {
    kPendingNone = 0,
    kPendingEnable = 1,
    kPendingDisable = 2,
    kSetPendingEnableOnVerify = 3
  };

  enum class CurrentState {
    kNoVerifiedHost,
    kNoPendingRequest,
    kPendingMatchesBackend,
    kValidPendingRequest
  };

  PendingState GetPendingState();
  CurrentState GetCurrentState();

  void ResetPendingNetworkRequest();
  void SetPendingState(PendingState pending_state);
  void AttemptSetHostStateNetworkRequest(bool is_retry);
  void OnSetHostStateNetworkRequestFinished(
      bool attempted_to_enable,
      device_sync::mojom::NetworkRequestResult result_code);
  bool ShouldEnableOnVerify();
  void ProcessEnableOnVerifyAttempt();
  bool ShouldAttemptToEnableAfterHostVerified();

  // The feature being managed.
  mojom::Feature managed_feature_;
  // Corresponding CryptAuth host feature for |managed_feature_|.
  multidevice::SoftwareFeature managed_host_feature_;
  const std::string pending_state_pref_name_;
  raw_ptr<HostStatusProvider> host_status_provider_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
  std::unique_ptr<base::OneShotTimer> timer_;

  bool network_request_in_flight_ = false;

  base::WeakPtrFactory<GlobalStateFeatureManagerImpl> weak_ptr_factory_{this};
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_GLOBAL_STATE_FEATURE_MANAGER_IMPL_H_
