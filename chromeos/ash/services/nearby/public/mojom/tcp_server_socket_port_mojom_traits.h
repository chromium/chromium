// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_MOJOM_TCP_SERVER_SOCKET_PORT_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_MOJOM_TCP_SERVER_SOCKET_PORT_MOJOM_TRAITS_H_

#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_server_socket_port.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<sharing::mojom::TcpServerSocketPortDataView,
                    ash::nearby::TcpServerSocketPort> {
  static uint16_t port(const ash::nearby::TcpServerSocketPort& port) {
    return port.port();
  }

  static bool Read(sharing::mojom::TcpServerSocketPortDataView port,
                   ash::nearby::TcpServerSocketPort* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_MOJOM_TCP_SERVER_SOCKET_PORT_MOJOM_TRAITS_H_
