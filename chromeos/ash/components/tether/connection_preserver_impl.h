// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_CONNECTION_PRESERVER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_CONNECTION_PRESERVER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/tether/active_host.h"
#include "chromeos/ash/components/tether/connection_preserver.h"
#include "chromeos/ash/components/tether/host_connection.h"

namespace ash {

class NetworkStateHandler;

namespace tether {

class TetherHostResponseRecorder;

// Concrete implementation of ConnectionPreserver.
class ConnectionPreserverImpl : public ConnectionPreserver,
                                public HostConnection::PayloadListener,
                                public ActiveHost::Observer {
 public:
  // The maximum duration of time that a BLE Connection should be preserved.
  // A preserved BLE Connection will be torn down if not used within this time.
  // If the connection is used for a host connection before this time runs out,
  // the Connection will be torn down.
  static constexpr const uint32_t kTimeoutSeconds = 60;

  ConnectionPreserverImpl(
      HostConnection::Factory* host_connection_factory,
      NetworkStateHandler* network_state_handler,
      ActiveHost* active_host,
      TetherHostResponseRecorder* tether_host_response_recorder);

  ConnectionPreserverImpl(const ConnectionPreserverImpl&) = delete;
  ConnectionPreserverImpl& operator=(const ConnectionPreserverImpl&) = delete;

  ~ConnectionPreserverImpl() override;

  // ConnectionPreserver:
  void HandleSuccessfulTetherAvailabilityResponse(
      const std::string& device_id) override;

 protected:
  // HostConnection::PayloadListener:
  void OnMessageReceived(std::unique_ptr<MessageWrapper> message) override;

  void OnConnectionAttemptFinished(
      std::unique_ptr<HostConnection> host_connection);
  void OnDisconnected();

  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  friend class ConnectionPreserverImplTest;

  bool IsConnectedToInternet();
  // Between |preserved_connection_device_id_| and |device_id|, return which is
  // the "preferred" preserved Connection, i.e., which is higher priority.
  std::string GetPreferredPreservedConnectionDeviceId(
      const std::string& device_id);
  void SetPreservedConnection(const std::string& device_id);
  void RemovePreservedConnectionIfPresent();

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer_for_test);

  raw_ptr<HostConnection::Factory> host_connection_factory_;
  raw_ptr<NetworkStateHandler> network_state_handler_;
  raw_ptr<ActiveHost> active_host_;
  raw_ptr<TetherHostResponseRecorder> tether_host_response_recorder_;

  std::unique_ptr<base::OneShotTimer> preserved_connection_timer_;

  std::string preserved_connection_device_id_;

  std::unique_ptr<HostConnection> preserved_host_connection_;

  base::WeakPtrFactory<ConnectionPreserverImpl> weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_CONNECTION_PRESERVER_IMPL_H_
