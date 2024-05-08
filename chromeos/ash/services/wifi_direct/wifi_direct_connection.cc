// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/wifi_direct/wifi_direct_connection.h"

namespace ash::wifi_direct {

using WifiDirectConnectionMetadata =
    WifiP2PController::WifiDirectConnectionMetadata;

namespace {

mojom::WifiDirectConnectionPropertiesPtr GetMojoProperties(
    const WifiDirectConnectionMetadata& metadata) {
  auto properties = mojom::WifiDirectConnectionProperties::New();
  properties->frequency = metadata.frequency;
  properties->ipv4_address = metadata.ipv4_address;
  return properties;
}

}  // namespace

// static
WifiDirectConnection::InstanceWithPendingRemotePair
WifiDirectConnection::Create(const WifiDirectConnectionMetadata& metadata,
                             base::OnceClosure disconnect_handler) {
  // Use base::WrapUnique(new WifiDirectConnection(...)) instead of
  // std::make_unique<WifiDirectConnection> to access a private constructor.
  std::unique_ptr<WifiDirectConnection> wifi_direct_connection =
      base::WrapUnique(new WifiDirectConnection(metadata));

  return std::make_pair(
      std::move(wifi_direct_connection),
      wifi_direct_connection->CreateRemote(std::move(disconnect_handler)));
}

WifiDirectConnection::WifiDirectConnection(
    const WifiDirectConnectionMetadata& metadata)
    : metadata_(metadata) {
  CHECK(WifiP2PController::IsInitialized());
}

WifiDirectConnection::~WifiDirectConnection() = default;

void WifiDirectConnection::GetProperties(GetPropertiesCallback callback) {
  std::move(callback).Run(GetMojoProperties(metadata_));
}

void WifiDirectConnection::AssociateSocket(mojo::PlatformHandle socket,
                                           AssociateSocketCallback callback) {
  WifiP2PController::Get()->TagSocket(metadata_.network_id, socket.TakeFD(),
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

bool WifiDirectConnection::IsOwner() const {
  return metadata_.is_owner;
}

void WifiDirectConnection::FlushForTesting() {
  receiver_.FlushForTesting();  // IN-TEST
}

}  // namespace ash::wifi_direct
