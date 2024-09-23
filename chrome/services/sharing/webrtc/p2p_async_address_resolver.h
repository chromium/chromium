// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_P2P_ASYNC_ADDRESS_RESOLVER_H_
#define CHROME_SERVICES_SHARING_WEBRTC_P2P_ASYNC_ADDRESS_RESOLVER_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/ip_address.h"
#include "services/network/public/mojom/p2p.mojom.h"

namespace sharing {

// P2PAsyncAddressResolver performs DNS hostname resolution. It's used
// to resolve addresses of STUN and relay servers.
// TODO(crbug.com/40115622): reuse code from blink instead.
class P2PAsyncAddressResolver {
 public:
  using DoneCallback =
      base::OnceCallback<void(const std::vector<net::IPAddress>&)>;

  explicit P2PAsyncAddressResolver(
      const mojo::SharedRemote<network::mojom::P2PSocketManager>&
          socket_manager);
  P2PAsyncAddressResolver(const P2PAsyncAddressResolver&) = delete;
  P2PAsyncAddressResolver& operator=(const P2PAsyncAddressResolver&) = delete;
  ~P2PAsyncAddressResolver();

  // Start address resolve process.
  void Start(const rtc::SocketAddress& addr,
             std::optional<int> address_family,
             DoneCallback done_callback);
  // Clients must unregister before exiting for cleanup.
  void Cancel();

 private:
  enum State {
    STATE_CREATED,
    STATE_SENT,
    STATE_FINISHED,
  };

  void OnResponse(const std::vector<net::IPAddress>& address);

  mojo::SharedRemote<network::mojom::P2PSocketManager> socket_manager_;

  THREAD_CHECKER(thread_checker_);

  State state_;
  DoneCallback done_callback_;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_P2P_ASYNC_ADDRESS_RESOLVER_H_
