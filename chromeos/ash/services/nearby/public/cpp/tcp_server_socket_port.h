// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_TCP_SERVER_SOCKET_PORT_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_TCP_SERVER_SOCKET_PORT_H_

#include <cstdint>
#include <optional>

namespace ash {
namespace nearby {

// A TCP server socket port number used by the Nearby Connections WifiLan
// medium. The port number is guaranteed to be in the interval [kMin, kMax]. We
// restrict the range of port numbers so as not to interfere with the lower port
// numbers used by core components.
class TcpServerSocketPort {
 public:
  // This range agree with the Nearby Connections WifiLan implementation
  // on GmsCore.
  static constexpr uint16_t kMin = 49152;
  static constexpr uint16_t kMax = 65535;

  // Creates a TcpServerSocketPort from the input |port| value. Returns nullopt
  // if |port| is not in the interval [kMin, kMax].
  static std::optional<TcpServerSocketPort> FromInt(int port);
  static std::optional<TcpServerSocketPort> FromUInt16(uint16_t port);

  // Creates a TcpServerSocketPort with a random port number in the range [kMin,
  // kMax].
  static TcpServerSocketPort Random();

  // Creates a TcpServerSocketPort with a random port number in the range [kMin,
  // kMax]. Note: We need a public default constructor in order to support mojo
  // type mapping. Do not use directly; prefer TcpServerSocketPort::Random().
  TcpServerSocketPort();

  ~TcpServerSocketPort();

  uint16_t port() const { return port_; }

 private:
  explicit TcpServerSocketPort(uint16_t port);

  uint16_t port_;
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_TCP_SERVER_SOCKET_PORT_H_
