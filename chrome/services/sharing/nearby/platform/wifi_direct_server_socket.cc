// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_server_socket.h"

namespace nearby::chrome {

WifiDirectServerSocket::WifiDirectServerSocket(mojo::PlatformHandle handle)
    : handle_(std::move(handle)) {}

WifiDirectServerSocket::~WifiDirectServerSocket() = default;

// api::WifiDirectServerSocket
std::string WifiDirectServerSocket::GetIPAddress() const {
  NOTIMPLEMENTED();
  return std::string();
}

int WifiDirectServerSocket::GetPort() const {
  NOTIMPLEMENTED();
  return -1;
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectServerSocket::Accept() {
  NOTIMPLEMENTED();
  return nullptr;
}

Exception WifiDirectServerSocket::Close() {
  NOTIMPLEMENTED();
  return {Exception::kSuccess};
}
}  // namespace nearby::chrome
