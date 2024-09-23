// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_P2P_SOCKET_CLIENT_DELEGATE_H_
#define CHROME_SERVICES_SHARING_WEBRTC_P2P_SOCKET_CLIENT_DELEGATE_H_

#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/p2p_socket_type.h"

namespace sharing {

// TODO(crbug.com/40115622): reuse code from blink instead.
class P2PSocketClientDelegate {
 public:
  virtual ~P2PSocketClientDelegate() {}

  // Called after the socket has been opened with the local endpoint address
  // as argument. Please note that in the presence of multiple interfaces,
  // you should not rely on the local endpoint address if possible.
  virtual void OnOpen(const net::IPEndPoint& local_address,
                      const net::IPEndPoint& remote_address) = 0;

  // Called once for each Send() call after the send is complete.
  virtual void OnSendComplete(
      const network::P2PSendPacketMetrics& send_metrics) = 0;

  // Called if an non-retryable error occurs.
  virtual void OnError() = 0;

  // Called when data is received on the socket.
  virtual void OnDataReceived(const net::IPEndPoint& address,
                              base::span<const uint8_t> data,
                              const base::TimeTicks& timestamp) = 0;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_P2P_SOCKET_CLIENT_DELEGATE_H_
