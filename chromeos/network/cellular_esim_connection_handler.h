// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_ESIM_CONNECTION_HANDLER_H_
#define CHROMEOS_NETWORK_CELLULAR_ESIM_CONNECTION_HANDLER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "dbus/object_path.h"

namespace chromeos {

class CellularInhibitor;
class NetworkStateHandler;
class NetworkState;

// Prepares an eSIM profile for connection. In order to connect to an eSIM
// profile, the correct SIM slot must be selected, and the relevant profile must
// be enabled.
//
// This class goes through a series of steps to ensure that this happens:
// (1) Check to see if the profile is already enabled; if so, return early.
// (2) Inhibit cellular scans.
// (3) Request installed profiles from Hermes.
// (4) Enable the relevant profile.
// (5) Request installed profiles again.
// (6) Uninhibit cellular scans.
// (7) Wait until the associated NetworkState becomes connectable.
//
// Note that if this class receies multiple connection requests, it processes
// them in a queue.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimConnectionHandler
    : public NetworkStateHandlerObserver {
 public:
  CellularESimConnectionHandler();
  CellularESimConnectionHandler(const CellularESimConnectionHandler&) = delete;
  CellularESimConnectionHandler& operator=(
      const CellularESimConnectionHandler&) = delete;
  ~CellularESimConnectionHandler() override;

  void Init(NetworkStateHandler* network_state_handler,
            CellularInhibitor* cellular_inhibitor);

  // Enables an eSIM profile to prepare for connecting to it. If successful,
  // this operation causes the service's "connectable" property to be set to
  // true. On error, |error_callback| is invoked.
  void EnableProfileForConnection(
      const std::string& service_path,
      base::OnceClosure success_callback,
      network_handler::ErrorCallback error_callback);

 private:
  struct ConnectionRequestMetadata {
    ConnectionRequestMetadata(const std::string& service_path,
                              base::OnceClosure success_callback,
                              network_handler::ErrorCallback error_callback);
    ~ConnectionRequestMetadata();

    std::string service_path;
    base::OnceClosure success_callback;
    network_handler::ErrorCallback error_callback;
  };

  enum class ConnectionState {
    kIdle,
    kCheckingServiceStatus,
    kInhibitingScans,
    kRequestingProfilesBeforeEnabling,
    kEnablingProfile,
    kRequestingProfilesAfterEnabling,
    kUninhibitingScans,
    kWaitingForConnectable
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const ConnectionState& step);

  // NetworkStateHandlerObserver:
  void NetworkPropertiesUpdated(const NetworkState* network) override;

  void ProcessRequestQueue();
  void TransitionToConnectionState(ConnectionState state);

  // If |error_name| is null, invokes the success callback. Otherwise, invokes
  // the error callback.
  void CompleteConnectionAttempt(const base::Optional<std::string>& error_name);

  const NetworkState* GetNetworkStateForCurrentOperation() const;
  base::Optional<dbus::ObjectPath> GetEuiccPathForCurrentOperation() const;
  base::Optional<dbus::ObjectPath> GetProfilePathForCurrentOperation() const;

  void CheckServiceStatus();
  void OnInhibitScanResult(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void RequestInstalledProfiles();
  void OnRequestInstalledProfilesResult(HermesResponseStatus status);
  void OnEnableCarrierProfileResult(HermesResponseStatus status);

  void UninhibitScans(
      const base::Optional<std::string>& error_before_uninhibit);
  void OnUninhibitScanResult(
      const base::Optional<std::string>& error_before_uninhibit,
      bool success);
  void CheckForConnectable();
  void OnWaitForConnectableTimeout();

  base::OneShotTimer timer_;

  NetworkStateHandler* network_state_handler_ = nullptr;
  CellularInhibitor* cellular_inhibitor_ = nullptr;

  ConnectionState state_ = ConnectionState::kIdle;
  base::queue<std::unique_ptr<ConnectionRequestMetadata>> request_queue_;

  std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock_;

  base::WeakPtrFactory<CellularESimConnectionHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_ESIM_CONNECTION_HANDLER_H_
