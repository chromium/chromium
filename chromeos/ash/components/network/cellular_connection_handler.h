// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_CONNECTION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_CONNECTION_HANDLER_H_

#include <memory>
#include <optional>

#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "dbus/object_path.h"

namespace ash {

namespace cellular_setup {
class ESimTestBase;
}

class CellularESimProfileHandler;
class CellularInhibitor;
class NetworkState;

// Prepares cellular networks for connection. Before we can connect to a
// cellular network, the network must be backed by Shill and must have its
// Connectable property set to true (meaning that it is the selected SIM
// profile in its slot).
//
// For pSIM networks, Chrome OS only supports a single physical SIM slot, so
// pSIM networks should always have their Connectable properties set to true as
// long as they are backed by Shill. Shill is expected to create a Service for
// each pSIM, so the only thing that needs to be done is to wait for that pSIM
// Service to be created by Shill.
//
// For eSIM networks, it is possible that there are multiple eSIM profiles on a
// single EUICC; in this case, Connectable == false refers to a disabled eSIM
// profile which must be enabled via Hermes before a connection can succeed. The
// steps for an eSIM network are:
//   (1) Check to see if the profile is already enabled; if so, return early.
//   (2) Inhibit cellular scans.
//   (3) Request installed profiles from Hermes.
//   (4) Enable the relevant profile.
//   (5) Uninhibit cellular scans.
//   (6) Wait until the associated NetworkState becomes connectable.
//   (7) Wait until Shill auto connected if the sim slot is switched.
//
// Note that if this class receives multiple connection requests, it processes
// them in FIFO order.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularConnectionHandler
    : public NetworkStateHandlerObserver {
 public:
  // TODO(b/271854446): Make these private once the migration has landed.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PrepareCellularConnectionResult {
    kSuccess = 0,
    kCouldNotFindNetworkWithIccid = 1,
    kInhibitFailed = 2,
    kCouldNotFindRelevantEuicc = 3,
    kRefreshProfilesFailed = 4,
    kCouldNotFindRelevantESimProfile = 5,
    kEnableProfileFailed = 6,
    kTimeoutWaitingForConnectable = 7,
    kMaxValue = kTimeoutWaitingForConnectable
  };

  CellularConnectionHandler();
  CellularConnectionHandler(const CellularConnectionHandler&) = delete;
  CellularConnectionHandler& operator=(const CellularConnectionHandler&) =
      delete;
  ~CellularConnectionHandler() override;

  void Init(NetworkStateHandler* network_state_handler,
            CellularInhibitor* cellular_inhibitor,
            CellularESimProfileHandler* cellular_esim_profile_handler);

  // Success callback which receives the network's service path as the first
  // parameter and a boolean indicates whether the network is autoconnected
  // as the second parameter.
  typedef base::OnceCallback<void(const std::string&, bool)> SuccessCallback;

  // Error callback which receives the network's service path as the first
  // parameter and an error name as the second parameter. If no service path is
  // available (e.g., if no network with the given ICCID was found), an empty
  // string is passed as the first parameter.
  typedef base::OnceCallback<void(const std::string&, const std::string&)>
      ErrorCallback;

  // Prepares an existing network (i.e., one which has *not* just been
  // installed) for a connection. Upon success, the network will be backed by
  // Shill and will be connectable.
  void PrepareExistingCellularNetworkForConnection(
      const std::string& iccid,
      SuccessCallback success_callback,
      ErrorCallback error_callback);

  // Prepares a newly-installed eSIM profile for connection. This should be
  // called immediately after installation succeeds so that the profile is
  // enabled in Hermes. Upon success, the network will be backed by Shill and
  // will be connectable.
  void PrepareNewlyInstalledCellularNetworkForConnection(
      const dbus::ObjectPath& euicc_path,
      const dbus::ObjectPath& profile_path,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      SuccessCallback success_callback,
      ErrorCallback error_callback);

 private:
  friend class CellularESimInstallerTest;
  friend class CellularPolicyHandlerTest;
  friend class ManagedNetworkConfigurationHandlerTest;
  friend class cellular_setup::ESimTestBase;

  struct ConnectionRequestMetadata {
    ConnectionRequestMetadata(const std::string& iccid,
                              SuccessCallback success_callback,
                              ErrorCallback error_callback);
    ConnectionRequestMetadata(
        const dbus::ObjectPath& euicc_path,
        const dbus::ObjectPath& profile_path,
        std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
        SuccessCallback success_callback,
        ErrorCallback error_callback);
    ~ConnectionRequestMetadata();

    std::optional<std::string> iccid;
    std::optional<dbus::ObjectPath> euicc_path;
    std::optional<dbus::ObjectPath> profile_path;
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
    // A boolean indicating that if the connection switches the SIM profile and
    // requires enabling the profile first.
    bool did_connection_require_enabling_profile = false;
    SuccessCallback success_callback;
    ErrorCallback error_callback;
  };

  enum class ConnectionState {
    kIdle,
    kCheckingServiceStatus,
    kInhibitingScans,
    kRequestingProfilesBeforeEnabling,
    kEnablingProfile,
    kWaitingForConnectable,
    kWaitingForShillAutoConnect,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const ConnectionState& step);

  // Timeout waiting for a cellular network to auto connect after switch
  // profile.
  static const base::TimeDelta kWaitingForAutoConnectTimeout;
  static std::optional<std::string> ResultToErrorString(
      PrepareCellularConnectionResult result);

  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void NetworkIdentifierTransitioned(const std::string& old_service_path,
                                     const std::string& new_service_path,
                                     const std::string& old_guid,
                                     const std::string& new_guid) override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  void ProcessRequestQueue();
  void TransitionToConnectionState(ConnectionState state);

  // Invokes the success or error callback, depending on |result| and
  // |auto_connected|.
  void CompleteConnectionAttempt(PrepareCellularConnectionResult result,
                                 bool auto_connected);

  const NetworkState* GetNetworkStateForCurrentOperation() const;
  std::optional<dbus::ObjectPath> GetEuiccPathForCurrentOperation() const;
  std::optional<dbus::ObjectPath> GetProfilePathForCurrentOperation() const;

  void CheckServiceStatus();
  void OnInhibitScanResult(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void RequestInstalledProfiles();
  void OnRefreshProfileListResult(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void EnableProfile();
  void OnEnableCarrierProfileResult(HermesResponseStatus status);

  void UninhibitScans(const std::optional<std::string>& error_before_uninhibit);
  void OnUninhibitScanResult(
      const std::optional<std::string>& error_before_uninhibit,
      bool success);
  void HandleNetworkPropertiesUpdate();
  void CheckForConnectable();
  void OnWaitForConnectableTimeout();
  void StartWaitingForShillAutoConnect();
  void CheckForAutoConnected();
  void OnWaitForAutoConnectTimeout();

  base::OneShotTimer timer_;

  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  raw_ptr<CellularInhibitor> cellular_inhibitor_ = nullptr;
  raw_ptr<CellularESimProfileHandler, DanglingUntriaged>
      cellular_esim_profile_handler_ = nullptr;

  ConnectionState state_ = ConnectionState::kIdle;
  base::queue<std::unique_ptr<ConnectionRequestMetadata>> request_queue_;

  base::WeakPtrFactory<CellularConnectionHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_CONNECTION_HANDLER_H_
