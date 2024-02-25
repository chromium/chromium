// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/mojom/tcp_server_socket_port_mojom_traits.h"

namespace mojo {

bool StructTraits<sharing::mojom::TcpServerSocketPortDataView,
                  ash::nearby::TcpServerSocketPort>::
    Read(sharing::mojom::TcpServerSocketPortDataView port,
         ash::nearby::TcpServerSocketPort* out) {
  // FromUInt16() validates the port range, returning nullopt if the port number
  // is invalid.
  std::optional<ash::nearby::TcpServerSocketPort> p =
      ash::nearby::TcpServerSocketPort::FromUInt16(port.port());
  if (!p)
    return false;

  *out = *p;

  return true;
}

}  // namespace mojo
