// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/ipc_network_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webrtc/net_address_utils.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace sharing {

namespace {

rtc::AdapterType ConvertConnectionTypeToAdapterType(
    net::NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return rtc::ADAPTER_TYPE_UNKNOWN;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return rtc::ADAPTER_TYPE_ETHERNET;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return rtc::ADAPTER_TYPE_WIFI;
    case net::NetworkChangeNotifier::CONNECTION_2G:
    case net::NetworkChangeNotifier::CONNECTION_3G:
    case net::NetworkChangeNotifier::CONNECTION_4G:
    case net::NetworkChangeNotifier::CONNECTION_5G:
      return rtc::ADAPTER_TYPE_CELLULAR;
    default:
      return rtc::ADAPTER_TYPE_UNKNOWN;
  }
}

}  // namespace

IpcNetworkManager::IpcNetworkManager(
    const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager,
    std::unique_ptr<webrtc::MdnsResponderInterface> mdns_responder)
    : p2p_socket_manager_(socket_manager),
      mdns_responder_(std::move(mdns_responder)) {
  DCHECK(p2p_socket_manager_.is_bound());
  p2p_socket_manager_->StartNetworkNotifications(
      network_notification_client_receiver_.BindNewPipeAndPassRemote());
}

IpcNetworkManager::~IpcNetworkManager() {
  DCHECK(!start_count_);
}

void IpcNetworkManager::StartUpdating() {
  if (network_list_received_) {
    // Post a task to avoid reentrancy.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&IpcNetworkManager::SendNetworksChangedSignal,
                                  weak_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "IpcNetworkManager::StartUpdating called; still waiting for "
               "network list from browser process.";
  }
  ++start_count_;
}

void IpcNetworkManager::StopUpdating() {
  DCHECK_GT(start_count_, 0);
  --start_count_;
}

void IpcNetworkManager::NetworkListChanged(
    const net::NetworkInterfaceList& list,
    const net::IPAddress& default_ipv4_local_address,
    const net::IPAddress& default_ipv6_local_address) {
  // Update flag if network list received for the first time.
  if (!network_list_received_) {
    VLOG(1) << "IpcNetworkManager received network list from browser process "
               "for the first time.";
    network_list_received_ = true;
  }

  // Default addresses should be set only when they are in the filtered list of
  // network addresses.
  bool use_default_ipv4_address = false;
  bool use_default_ipv6_address = false;

  // rtc::Network uses these prefix_length to compare network
  // interfaces discovered.
  std::vector<std::unique_ptr<rtc::Network>> networks;
  for (auto it = list.begin(); it != list.end(); it++) {
    rtc::IPAddress ip_address = webrtc::NetIPAddressToRtcIPAddress(it->address);
    DCHECK(!ip_address.IsNil());

    rtc::IPAddress prefix = rtc::TruncateIP(ip_address, it->prefix_length);
    rtc::AdapterType adapter_type =
        ConvertConnectionTypeToAdapterType(it->type);
    // If the adapter type is unknown, try to guess it using WebRTC's string
    // matching rules.
    if (adapter_type == rtc::ADAPTER_TYPE_UNKNOWN) {
      adapter_type = rtc::GetAdapterTypeFromName(it->name.c_str());
    }
    auto network = CreateNetwork(it->name, it->name, prefix, it->prefix_length,
                                 adapter_type);
    network->set_default_local_address_provider(this);
    network->set_mdns_responder_provider(this);

    rtc::InterfaceAddress iface_addr;
    if (it->address.IsIPv4()) {
      use_default_ipv4_address |= (default_ipv4_local_address == it->address);
      iface_addr = rtc::InterfaceAddress(ip_address);
    } else {
      DCHECK(it->address.IsIPv6());
      iface_addr = rtc::InterfaceAddress(ip_address, it->ip_address_attributes);

      // Only allow non-link-local, non-loopback, non-deprecated IPv6 addresses
      // which don't contain MAC.
      if (rtc::IPIsMacBased(iface_addr) ||
          (it->ip_address_attributes & net::IP_ADDRESS_ATTRIBUTE_DEPRECATED) ||
          rtc::IPIsLinkLocal(iface_addr) || rtc::IPIsLoopback(iface_addr)) {
        continue;
      }

      use_default_ipv6_address |= (default_ipv6_local_address == it->address);
    }
    network->AddIP(iface_addr);
    networks.push_back(std::move(network));
  }

  // Update the default local addresses.
  rtc::IPAddress ipv4_default;
  rtc::IPAddress ipv6_default;
  if (use_default_ipv4_address) {
    ipv4_default =
        webrtc::NetIPAddressToRtcIPAddress(default_ipv4_local_address);
  }
  if (use_default_ipv6_address) {
    ipv6_default =
        webrtc::NetIPAddressToRtcIPAddress(default_ipv6_local_address);
  }
  set_default_local_addresses(ipv4_default, ipv6_default);

  bool changed = false;
  NetworkManager::Stats stats;
  MergeNetworkList(std::move(networks), &changed, &stats);
  if (changed)
    SignalNetworksChanged();
}

webrtc::MdnsResponderInterface* IpcNetworkManager::GetMdnsResponder() const {
  return mdns_responder_.get();
}

void IpcNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace sharing
