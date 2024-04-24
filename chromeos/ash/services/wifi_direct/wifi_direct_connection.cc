// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/wifi_direct/wifi_direct_connection.h"

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"

namespace ash::wifi_direct {

// static
WifiDirectConnection::InstanceWithPendingRemotePair
WifiDirectConnection::Create(int shill_id,
                             uint32_t frequency,
                             int network_id,
                             base::OnceClosure disconnect_handler) {
  // Use base::WrapUnique(new WifiDirectConnection(...)) instead of
  // std::make_unique<WifiDirectConnection> to access a private constructor.
  std::unique_ptr<WifiDirectConnection> wifi_direct_connection =
      base::WrapUnique(
          new WifiDirectConnection(shill_id, frequency, network_id));

  return std::make_pair(
      std::move(wifi_direct_connection),
      wifi_direct_connection->CreateRemote(std::move(disconnect_handler)));
}

WifiDirectConnection::WifiDirectConnection(int shill_id,
                                           uint32_t frequency,
                                           int network_id)
    : shill_id_(shill_id), frequency_(frequency), network_id_(network_id) {
  CHECK(WifiP2PController::IsInitialized());
}

WifiDirectConnection::~WifiDirectConnection() = default;

void WifiDirectConnection::GetFrequency(GetFrequencyCallback callback) {
  std::move(callback).Run(frequency_);
}

void WifiDirectConnection::AssociateSocket(mojo::PlatformHandle socket,
                                           AssociateSocketCallback callback) {
  WifiP2PController::Get()->TagSocket(network_id_, socket.TakeFD(),
                                      std::move(callback));
}

mojo::PendingRemote<mojom::WifiDirectConnection>
WifiDirectConnection::CreateRemote(base::OnceClosure disconnect_handler) {
  // Only one mojo::PendingRemote should be created per instance.
  CHECK(!receiver_.is_bound());
  auto pending_remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
  return pending_remote;
}

void WifiDirectConnection::FlushForTesting() {
  receiver_.FlushForTesting();  // IN-TEST
}

}  // namespace ash::wifi_direct
