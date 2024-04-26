// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_IPC_NETWORK_MANAGER_H_
#define CHROME_SERVICES_SHARING_WEBRTC_IPC_NETWORK_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "third_party/webrtc/rtc_base/mdns_responder_interface.h"
#include "third_party/webrtc/rtc_base/network.h"

namespace net {
class IPAddress;
}  // namespace net

namespace sharing {

// IpcNetworkManager is a NetworkManager for libjingle that gets a
// list of network interfaces from the browser.
// TODO(crbug.com/40115622): reuse code from blink instead.
class IpcNetworkManager : public rtc::NetworkManagerBase,
                          public network::mojom::P2PNetworkNotificationClient {
 public:
  IpcNetworkManager(
      const mojo::SharedRemote<network::mojom::P2PSocketManager>&
          socket_manager,
      std::unique_ptr<webrtc::MdnsResponderInterface> mdns_responder);
  IpcNetworkManager(const IpcNetworkManager&) = delete;
  IpcNetworkManager& operator=(const IpcNetworkManager&) = delete;
  ~IpcNetworkManager() override;

  // rtc:::NetworkManagerBase:
  void StartUpdating() override;
  void StopUpdating() override;
  webrtc::MdnsResponderInterface* GetMdnsResponder() const override;

  // network::mojom::P2PNetworkNotificationClient:
  void NetworkListChanged(
      const net::NetworkInterfaceList& list,
      const net::IPAddress& default_ipv4_local_address,
      const net::IPAddress& default_ipv6_local_address) override;

 private:
  void SendNetworksChangedSignal();

  mojo::SharedRemote<network::mojom::P2PSocketManager> p2p_socket_manager_;
  std::unique_ptr<webrtc::MdnsResponderInterface> mdns_responder_;
  int start_count_ = 0;
  bool network_list_received_ = false;

  mojo::Receiver<network::mojom::P2PNetworkNotificationClient>
      network_notification_client_receiver_{this};

  base::WeakPtrFactory<IpcNetworkManager> weak_factory_{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_IPC_NETWORK_MANAGER_H_
