// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/crash_recovery_manager.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

class HostScanCache;

// Concrete CrashRecoveryManager implementation.
class CrashRecoveryManagerImpl : public CrashRecoveryManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<CrashRecoveryManager> NewInstance(
        NetworkStateHandler* network_state_handler,
        ActiveHost* active_host,
        HostScanCache* host_scan_cache);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<CrashRecoveryManager> BuildInstance(
        NetworkStateHandler* network_state_handler,
        ActiveHost* active_host,
        HostScanCache* host_scan_cache);
    virtual ~Factory();

   private:
    static Factory* factory_instance_;
  };

  ~CrashRecoveryManagerImpl() override;

  // CrashRecoveryManager:
  void RestorePreCrashStateIfNecessary(
      const base::Closure& on_restoration_finished) override;

 protected:
  CrashRecoveryManagerImpl(NetworkStateHandler* network_state_handler,
                           ActiveHost* active_host,
                           HostScanCache* host_scan_cache);

 private:
  void RestoreConnectedState(const base::Closure& on_restoration_finished,
                             const std::string& active_host_device_id,
                             const std::string& tether_network_guid,
                             const std::string& wifi_network_guid);
  void OnActiveHostFetched(
      const base::Closure& on_restoration_finished,
      ActiveHost::ActiveHostStatus active_host_status,
      base::Optional<multidevice::RemoteDeviceRef> active_host,
      const std::string& tether_network_guid,
      const std::string& wifi_network_guid);

  NetworkStateHandler* network_state_handler_;
  ActiveHost* active_host_;
  HostScanCache* host_scan_cache_;

  base::WeakPtrFactory<CrashRecoveryManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrashRecoveryManagerImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_IMPL_H_
