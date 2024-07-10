// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_SHARED_REMOTES_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_SHARED_REMOTES_H_

#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/mdns.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc_signaling_messenger.mojom.h"
#include "chromeos/ash/services/wifi_direct/public/mojom/wifi_direct_manager.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/mojom/p2p.mojom.h"

namespace nearby {

// Container for the SharedRemote objects required by Nearby Connections and
// Nearby Presence.
struct NearbySharedRemotes {
  static NearbySharedRemotes* GetInstance();
  static void SetInstance(NearbySharedRemotes* instance);

  NearbySharedRemotes();
  ~NearbySharedRemotes();
  NearbySharedRemotes(const NearbySharedRemotes&) = delete;
  NearbySharedRemotes& operator=(const NearbySharedRemotes&) = delete;

  mojo::SharedRemote<bluetooth::mojom::Adapter> bluetooth_adapter;
  mojo::SharedRemote<network::mojom::P2PSocketManager> socket_manager;
  mojo::SharedRemote<::sharing::mojom::MdnsResponderFactory>
      mdns_responder_factory;
  mojo::SharedRemote<::sharing::mojom::WebRtcSignalingMessenger>
      webrtc_signaling_messenger;
  mojo::SharedRemote<::sharing::mojom::IceConfigFetcher> ice_config_fetcher;
  mojo::SharedRemote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config;
  mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>
      firewall_hole_factory;
  mojo::SharedRemote<::sharing::mojom::TcpSocketFactory> tcp_socket_factory;
  mojo::SharedRemote<::sharing::mojom::MdnsManager> mdns_manager;
  mojo::SharedRemote<
      ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
      nearby_presence_credential_storage;
  mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>
      wifi_direct_manager;
  mojo::SharedRemote<sharing::mojom::FirewallHoleFactory>
      wifi_direct_firewall_hole_factory;
};

}  // namespace nearby

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_SHARED_REMOTES_H_
