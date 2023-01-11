// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_IMPL_H_

#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/components/tether/active_host.h"
#include "chromeos/ash/components/tether/crash_recovery_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class NetworkStateHandler;

namespace tether {

class HostScanCache;

// Concrete CrashRecoveryManager implementation.
class CrashRecoveryManagerImpl : public CrashRecoveryManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<CrashRecoveryManager> Create(
        NetworkStateHandler* network_state_handler,
        ActiveHost* active_host,
        HostScanCache* host_scan_cache);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<CrashRecoveryManager> CreateInstance(
        NetworkStateHandler* network_state_handler,
        ActiveHost* active_host,
        HostScanCache* host_scan_cache) = 0;
    virtual ~Factory();

   private:
    static Factory* factory_instance_;
  };

  CrashRecoveryManagerImpl(const CrashRecoveryManagerImpl&) = delete;
  CrashRecoveryManagerImpl& operator=(const CrashRecoveryManagerImpl&) = delete;

  ~CrashRecoveryManagerImpl() override;

  // CrashRecoveryManager:
  void RestorePreCrashStateIfNecessary(
      base::OnceClosure on_restoration_finished) override;

 protected:
  CrashRecoveryManagerImpl(NetworkStateHandler* network_state_handler,
                           ActiveHost* active_host,
                           HostScanCache* host_scan_cache);

 private:
  void RestoreConnectedState(base::OnceClosure on_restoration_finished,
                             const std::string& active_host_device_id,
                             const std::string& tether_network_guid,
                             const std::string& wifi_network_guid);
  void OnActiveHostFetched(
      base::OnceClosure on_restoration_finished,
      ActiveHost::ActiveHostStatus active_host_status,
      absl::optional<multidevice::RemoteDeviceRef> active_host,
      const std::string& tether_network_guid,
      const std::string& wifi_network_guid);

  NetworkStateHandler* network_state_handler_;
  ActiveHost* active_host_;
  HostScanCache* host_scan_cache_;

  base::WeakPtrFactory<CrashRecoveryManagerImpl> weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_IMPL_H_
