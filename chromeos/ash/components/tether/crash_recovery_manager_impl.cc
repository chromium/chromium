// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/crash_recovery_manager_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/tether/host_scan_cache.h"

namespace ash {

namespace tether {

// static
CrashRecoveryManagerImpl::Factory*
    CrashRecoveryManagerImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<CrashRecoveryManager> CrashRecoveryManagerImpl::Factory::Create(
    NetworkStateHandler* network_state_handler,
    ActiveHost* active_host,
    HostScanCache* host_scan_cache) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(network_state_handler, active_host,
                                             host_scan_cache);
  }

  return base::WrapUnique(new CrashRecoveryManagerImpl(
      network_state_handler, active_host, host_scan_cache));
}

// static
void CrashRecoveryManagerImpl::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

CrashRecoveryManagerImpl::Factory::~Factory() = default;

CrashRecoveryManagerImpl::CrashRecoveryManagerImpl(
    NetworkStateHandler* network_state_handler,
    ActiveHost* active_host,
    HostScanCache* host_scan_cache)
    : network_state_handler_(network_state_handler),
      active_host_(active_host),
      host_scan_cache_(host_scan_cache) {}

CrashRecoveryManagerImpl::~CrashRecoveryManagerImpl() = default;

void CrashRecoveryManagerImpl::RestorePreCrashStateIfNecessary(
    base::OnceClosure on_restoration_finished) {
  ActiveHost::ActiveHostStatus status = active_host_->GetActiveHostStatus();
  std::string active_host_device_id = active_host_->GetActiveHostDeviceId();
  std::string wifi_network_guid = active_host_->GetWifiNetworkGuid();
  std::string tether_network_guid = active_host_->GetTetherNetworkGuid();

  if (status == ActiveHost::ActiveHostStatus::DISCONNECTED) {
    // There was no active Tether session, so either the last TetherComponent
    // shutdown occurred normally (i.e., without a crash), or it occurred due
    // to a crash and there was no active host at the time of the crash.
    std::move(on_restoration_finished).Run();
    return;
  }

  if (status == ActiveHost::ActiveHostStatus::CONNECTING) {
    // If a connection attempt was in progress when the crash occurred, abandon
    // the connection attempt. Set ActiveHost back to DISCONNECTED; the user can
    // attempt another connection if desired.
    // TODO(khorimoto): Explore whether we should attempt to restore an
    // in-progress connection attempt. This is a low-priority edge case which is
    // difficult to solve.
    PA_LOG(WARNING) << "Browser crashed while Tether connection attempt was in "
                    << "progress. Abandoning connection attempt.";
    active_host_->SetActiveHostDisconnected();
    std::move(on_restoration_finished).Run();
    return;
  }

  RestoreConnectedState(std::move(on_restoration_finished),
                        active_host_device_id, tether_network_guid,
                        wifi_network_guid);
}

void CrashRecoveryManagerImpl::RestoreConnectedState(
    base::OnceClosure on_restoration_finished,
    const std::string& active_host_device_id,
    const std::string& tether_network_guid,
    const std::string& wifi_network_guid) {
  // Since the host was connected, both a Wi-Fi and Tether network GUID are
  // expected to be present.
  DCHECK(!wifi_network_guid.empty() && !tether_network_guid.empty());

  if (!host_scan_cache_->ExistsInCache(tether_network_guid)) {
    // If a crash occurred, HostScanCache is expected to have restored its state
    // via its persistent scan results. If that did not happen correctly, there
    // is no way to restore the active host, so abandon the connection. Note
    // that this is not an expected error condition.
    PA_LOG(ERROR)
        << "Browser crashed while a Tether connection was active, "
        << "but the scan result for the active host was lost. Setting "
        << "the active host to DISCONNECTED.";
    active_host_->SetActiveHostDisconnected();
    std::move(on_restoration_finished).Run();
    return;
  }

  // Since the associated scan result exists in the cache, it is expected to be
  // present in the network stack.
  const NetworkState* tether_state =
      network_state_handler_->GetNetworkStateFromGuid(tether_network_guid);
  DCHECK(tether_state);

  const NetworkState* wifi_state =
      network_state_handler_->GetNetworkStateFromGuid(wifi_network_guid);
  if (!wifi_state || !wifi_state->IsConnectedState()) {
    // If the Wi-Fi network corresponding to the Tether hotspot is not present
    // or is no longer connected, then this device is no longer truly connected
    // to the active host.
    PA_LOG(ERROR) << "Browser crashed while a Tether connection was active, "
                  << "but underlying Wi-Fi network corresponding to the Tether "
                  << "connection is no longer present. Setting the active host "
                  << "to DISCONNECTED.";
    active_host_->SetActiveHostDisconnected();
    std::move(on_restoration_finished).Run();
    return;
  }

  // Because the NetworkState object representing the Wi-Fi network was lost
  // during the crash, the association between it and the Tether NetworkState
  // has been broken. Restore it now.
  DCHECK(wifi_state->tether_guid().empty());
  network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
      tether_network_guid, wifi_network_guid);

  active_host_->GetActiveHost(base::BindOnce(
      &CrashRecoveryManagerImpl::OnActiveHostFetched,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_restoration_finished)));
}

void CrashRecoveryManagerImpl::OnActiveHostFetched(
    base::OnceClosure on_restoration_finished,
    ActiveHost::ActiveHostStatus active_host_status,
    std::optional<multidevice::RemoteDeviceRef> active_host,
    const std::string& tether_network_guid,
    const std::string& wifi_network_guid) {
  DCHECK(ActiveHost::ActiveHostStatus::CONNECTED == active_host_status);
  DCHECK(active_host_);

  // Even though the active host has not actually changed, fire an active host
  // changed update so that classes listening on ActiveHost (e.g.,
  // ActiveHostNetworkStateUpdater and KeepAliveScheduler) will be notified
  // that there is an active connection.
  //
  // Note: SendActiveHostChangedUpdate() is a protected function of ActiveHost
  // which is only invoked here because CrashRecoveryManagerImpl is a friend
  // class.
  // It is invoked directly here because we are sending out a "fake" change
  // event which has equal old and new values.
  active_host_->SendActiveHostChangedUpdate(
      ActiveHost::ActiveHostStatus::CONNECTED /* old_status */,
      active_host ? active_host->GetDeviceId()
                  : std::string() /* old_active_host_id */,
      tether_network_guid /* old_tether_network_guid */,
      wifi_network_guid /* old_wifi_network_guid */,
      ActiveHost::ActiveHostStatus::CONNECTED /* new_status */,
      active_host /* new_active_host */,
      tether_network_guid /* new_tether_network_guid */,
      wifi_network_guid /* new_wifi_network_guid */);
  std::move(on_restoration_finished).Run();
}

}  // namespace tether

}  // namespace ash
