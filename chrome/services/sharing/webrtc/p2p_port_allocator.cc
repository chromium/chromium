// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/p2p_port_allocator.h"

#include <memory>
#include <utility>

#include "base/check.h"

namespace sharing {

P2PPortAllocator::P2PPortAllocator(rtc::NetworkManager* network_manager,
                                   rtc::PacketSocketFactory* socket_factory,
                                   const Config& config)
    : cricket::BasicPortAllocator(network_manager, socket_factory),
      config_(config) {
  DCHECK(network_manager);
  DCHECK(socket_factory);
  uint32_t flags = 0;
  if (!config_.enable_multiple_routes) {
    flags |= cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION;
  }
  if (!config_.enable_default_local_candidate) {
    flags |= cricket::PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE;
  }
  if (!config_.enable_nonproxied_udp) {
    flags |= cricket::PORTALLOCATOR_DISABLE_UDP |
             cricket::PORTALLOCATOR_DISABLE_STUN |
             cricket::PORTALLOCATOR_DISABLE_UDP_RELAY;
  }
  set_flags(flags);
  set_allow_tcp_listen(false);
}

P2PPortAllocator::~P2PPortAllocator() = default;

}  // namespace sharing
