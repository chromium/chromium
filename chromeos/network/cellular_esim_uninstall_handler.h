// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_ESIM_UNINSTALL_HANDLER_H_
#define CHROMEOS_NETWORK_CELLULAR_ESIM_UNINSTALL_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/values.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "dbus/object_path.h"

namespace chromeos {

class CellularInhibitor;
class NetworkState;
class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkStateHandler;

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
// This class queues Uninstallation requests and runs them in order.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimUninstallHandler {
 public:
  CellularESimUninstallHandler();
  CellularESimUninstallHandler(const CellularESimUninstallHandler&) = delete;
  CellularESimUninstallHandler& operator=(const CellularESimUninstallHandler&) =
      delete;
  ~CellularESimUninstallHandler();

  void Init(CellularInhibitor* cellular_inhibitor,
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

 private:
  enum class UninstallState {
    kIdle,
    kDisconnectingNetwork,
    kInhibitingShill,
    kRequestingInstalledProfiles,
    kDisablingProfile,
    kUninstallingProfile,
    kRemovingShillService,
    kSuccess,
    kFailure,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const UninstallState& step);

  // Represents ESim uninstallation request parameters. Requests are queued and
  // processed one at a time.
  struct UninstallRequest {
    UninstallRequest(const std::string& iccid,
                     const dbus::ObjectPath& esim_profile_path,
                     const dbus::ObjectPath& euicc_path,
                     UninstallRequestCallback callback);
    ~UninstallRequest();
    std::string iccid;
    dbus::ObjectPath esim_profile_path;
    dbus::ObjectPath euicc_path;
    UninstallRequestCallback callback;
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock = nullptr;
  };

  void ProcessUninstallRequest();
  void TransitionToUninstallState(UninstallState next_state);
  void AttemptNetworkDisconnectIfRequired();
  void AttemptShillInhibit();
  void OnShillInhibit(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void AttemptRequestInstalledProfiles();
  void AttemptDisableProfileIfRequired();
  void AttemptUninstallProfile();
  void AttemptRemoveShillService();
  void TransitionUninstallStateOnHermesSuccess(UninstallState next_state,
                                               HermesResponseStatus status);
  void TransitionUninstallStateOnSuccessBoolean(UninstallState next_state,
                                                bool success);
  void OnNetworkHandlerError(const std::string& error_name,
                             std::unique_ptr<base::DictionaryValue> error_data);
  const NetworkState* GetNetworkStateForIccid(const std::string& iccid);

  UninstallState state_ = UninstallState::kIdle;
  const NetworkState* curr_request_network_state_ = nullptr;
  base::queue<std::unique_ptr<UninstallRequest>> uninstall_requests_;

  CellularInhibitor* cellular_inhibitor_ = nullptr;
  NetworkConfigurationHandler* network_configuration_handler_ = nullptr;
  NetworkConnectionHandler* network_connection_handler_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;

  base::WeakPtrFactory<CellularESimUninstallHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_PROFILE_UNINSTALLER_H_