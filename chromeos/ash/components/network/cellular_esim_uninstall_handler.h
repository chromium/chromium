// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_UNINSTALL_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_UNINSTALL_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "dbus/object_path.h"

namespace ash {

class CellularESimProfileHandler;
class CellularInhibitor;
class ManagedCellularPrefHandler;
class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkState;

// Handles Uninstallation of an eSIM profile and it's corresponding network.
//
// Uninstalling and eSIM network involves interacting with both Shill and Hermes
// in the following sequence:
// 1. Disconnect Network with Shill
// 2. Inhibit Cellular Scans
// 3. Request Installed eSIM profiles from Hermes
// 4. Disable eSIM profile through Hermes
// 5. Uninstall eSIM profile in Hermes
// 6. Remove Shill configuration for Network
// 7. Uninhibit Cellular Scans
//
// Uninstallation requests are queued and run in order.
//
// Note: This class doesn't check and remove stale Shill eSIM services anymore
// because it might remove usable eSIM services incorrectly. This may cause some
// issues where some stale networks showing in UI because its Shill
// configuration doesn't get removed properly during the uninstallation.
// TODO(b/210726568)
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimUninstallHandler
    : public NetworkStateHandlerObserver {
 public:
  // TODO(b/271854446): Make these private once the migration has landed.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UninstallESimResult {
    kSuccess = 0,
    kNetworkNotFound = 1,
    kDisconnectFailed = 2,
    kInhibitFailed = 3,
    kRefreshProfilesFailed = 4,
    kDisableProfileFailed = 5,
    kUninstallProfileFailed = 6,
    kRemoveServiceFailed = 7,
    kMaxValue = kRemoveServiceFailed
  };

  // Timeout when waiting for network list change after removing network
  // service. Service removal continues with next service.
  static const base::TimeDelta kNetworkListWaitTimeout;

  CellularESimUninstallHandler();
  CellularESimUninstallHandler(const CellularESimUninstallHandler&) = delete;
  CellularESimUninstallHandler& operator=(const CellularESimUninstallHandler&) =
      delete;
  ~CellularESimUninstallHandler() override;

  void Init(CellularInhibitor* cellular_inhibitor,
            CellularESimProfileHandler* cellular_esim_profile_handler,
            ManagedCellularPrefHandler* managed_cellular_pref_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            NetworkConnectionHandler* network_connection_handler,
            NetworkStateHandler* network_state_handler);

  // Callback that returns true or false to indicate the success or failure of
  // an uninstallation request.
  using UninstallRequestCallback = base::OnceCallback<void(bool success)>;

  // Uninstalls an ESim profile and network with given |iccid| and
  // |esim_profile_path| that is installed in Euicc with the given
  // |euicc_path|.
  void UninstallESim(const std::string& iccid,
                     const dbus::ObjectPath& esim_profile_path,
                     const dbus::ObjectPath& euicc_path,
                     UninstallRequestCallback callback);

  // Resets memory ie. Removes all eSIM profiles on the Euicc with given
  // |euicc_path|.
  void ResetEuiccMemory(const dbus::ObjectPath& euicc_path,
                        UninstallRequestCallback callback);

 private:
  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;
  void OnShuttingDown() override;

  friend class CellularESimUninstallHandlerTest;

  enum class UninstallState {
    kIdle,
    kCheckingNetworkState,
    kDisconnectingNetwork,
    kInhibitingShill,
    kRequestingInstalledProfiles,
    kDisablingProfile,
    kUninstallingProfile,
    kRemovingShillService,
    kWaitingForNetworkListUpdate,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const UninstallState& step);

  // Represents ESim uninstallation request parameters. Requests are queued and
  // processed one at a time. |esim_profile_path| and |euicc_path| are nullopt
  // for stale eSIM service removal requests. These requests skip directly to
  // Shill configuration removal.
  struct UninstallRequest {
    UninstallRequest(const std::optional<std::string>& iccid,
                     const std::optional<dbus::ObjectPath>& esim_profile_path,
                     const std::optional<dbus::ObjectPath>& euicc_path,
                     bool reset_euicc,
                     UninstallRequestCallback callback);
    ~UninstallRequest();
    std::optional<std::string> iccid;
    std::optional<dbus::ObjectPath> esim_profile_path;
    std::optional<dbus::ObjectPath> euicc_path;
    bool reset_euicc;
    base::flat_set<std::string> removed_service_paths;
    UninstallRequestCallback callback;
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const UninstallRequest& request);

  void ProcessPendingUninstallRequests();
  void TransitionToUninstallState(UninstallState next_state);
  void CompleteCurrentRequest(UninstallESimResult result);

  std::string GetIdForCurrentRequest() const;
  const NetworkState* GetNetworkStateForCurrentRequest() const;

  void CheckActiveNetworkState();

  void AttemptNetworkDisconnect(const NetworkState* network);
  void OnDisconnectSuccess();
  void OnDisconnectFailure(const std::string& error_name);

  void AttemptShillInhibit();
  void OnShillInhibit(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);

  void AttemptRequestInstalledProfiles();
  void OnRefreshProfileListResult(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);

  void AttemptDisableProfile();
  void OnDisableProfile(HermesResponseStatus status);

  void AttemptUninstallProfile();
  void OnUninstallProfile(const base::flat_set<std::string>& removed_iccids,
                          HermesResponseStatus status);

  void AttemptRemoveShillService();
  void OnRemoveServiceSuccess(const std::string& removed_service_path);
  void OnRemoveServiceFailure(const std::string& error_name);
  void OnNetworkListWaitTimeout();

  std::optional<dbus::ObjectPath> GetEnabledCellularESimProfilePath();
  NetworkStateHandler::NetworkStateList GetESimCellularNetworks() const;
  const NetworkState* GetNextResetServiceToRemove() const;
  base::flat_set<std::string> GetAllIccidsOnEuicc(
      const dbus::ObjectPath& euicc_path);

  UninstallState state_ = UninstallState::kIdle;
  base::circular_deque<std::unique_ptr<UninstallRequest>> uninstall_requests_;

  base::OneShotTimer network_list_wait_timer_;

  raw_ptr<CellularInhibitor> cellular_inhibitor_ = nullptr;
  raw_ptr<CellularESimProfileHandler> cellular_esim_profile_handler_ = nullptr;
  raw_ptr<ManagedCellularPrefHandler, DanglingUntriaged>
      managed_cellular_pref_handler_ = nullptr;
  raw_ptr<NetworkConfigurationHandler> network_configuration_handler_ = nullptr;
  raw_ptr<NetworkConnectionHandler> network_connection_handler_ = nullptr;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  size_t last_service_count_removal_for_testing_ = 0;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<CellularESimUninstallHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_UNINSTALL_HANDLER_H_
