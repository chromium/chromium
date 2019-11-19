// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_SOCKET_MANAGER_H_
#define CHROME_SERVICES_CUPS_PROXY_SOCKET_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"

#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"

namespace net {
class UnixDomainClientSocket;
}  // namespace net

namespace cups_proxy {

using SocketManagerCallback =
    base::OnceCallback<void(std::unique_ptr<std::vector<uint8_t>>)>;

// This manager proxies IPP requests to the CUPS daemon and asynchronously
// responds with the IPP response. This class must be created and accessed
// from a sequenced context.
class SocketManager {
 public:
  // Factory function.
  static std::unique_ptr<SocketManager> Create(
      CupsProxyServiceDelegate* const delegate);

  // Factory function that allows injected dependencies, for testing.
  static std::unique_ptr<SocketManager> CreateForTesting(
      std::unique_ptr<net::UnixDomainClientSocket> socket,
      CupsProxyServiceDelegate* const delegate);

  virtual ~SocketManager() = default;

  // Attempts to send |request| to the CUPS Daemon, and return its response via
  // |cb|. |cb| will run on the caller's sequence. Note: Can only handle 1
  // inflight request at a time; attempts to proxy more will DCHECK.
  virtual void ProxyToCups(std::vector<uint8_t> request,
                           SocketManagerCallback cb) = 0;
};

}  // namespace cups_proxy

#endif  // CHROME_SERVICES_CUPS_PROXY_SOCKET_MANAGER_H_
