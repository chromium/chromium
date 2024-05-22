// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SERVER_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SERVER_SOCKET_H_

#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "third_party/nearby/src/internal/platform/exception.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_direct.h"

namespace nearby::chrome {

class WifiDirectServerSocket : public api::WifiDirectServerSocket {
 public:
  explicit WifiDirectServerSocket(mojo::PlatformHandle handle);
  WifiDirectServerSocket(const WifiDirectServerSocket&) = delete;
  WifiDirectServerSocket& operator=(const WifiDirectServerSocket&) = delete;
  ~WifiDirectServerSocket() override;

  // api::WifiDirectServerSocket
  std::string GetIPAddress() const override;
  int GetPort() const override;
  std::unique_ptr<api::WifiDirectSocket> Accept() override;
  Exception Close() override;

 private:
  mojo::PlatformHandle handle_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SERVER_SOCKET_H_
