// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"

namespace ash {
namespace nearby {

namespace {

bool IsPortInRange(uint16_t port) {
  return port >= TcpServerSocketPort::kMin && port <= TcpServerSocketPort::kMax;
}

// Generate random number in [kMin, kMax] inclusive. Note:
// RandGenerator(range) uses the non-inclusive interval [0, range).
uint16_t GenerateRandomPort() {
  return static_cast<uint16_t>(TcpServerSocketPort::kMin +
                               base::RandGenerator(TcpServerSocketPort::kMax -
                                                   TcpServerSocketPort::kMin +
                                                   1));
}

}  // namespace

std::optional<TcpServerSocketPort> TcpServerSocketPort::FromInt(int port) {
  if (!base::IsValueInRangeForNumericType<uint16_t>(port)) {
    LOG(ERROR) << "TcpServerSocketPort::" << __func__ << ": Port " << port
               << " is not uint16.";
    return std::nullopt;
  }

  return TcpServerSocketPort::FromUInt16(static_cast<uint16_t>(port));
}

std::optional<TcpServerSocketPort> TcpServerSocketPort::FromUInt16(
    uint16_t port) {
  if (!IsPortInRange(port)) {
    LOG(ERROR) << "TcpServerSocketPort::" << __func__ << ": Port " << port
               << " is not in the range [" << kMin << "," << kMax << "].";
    return std::nullopt;
  }

  return TcpServerSocketPort(port);
}

TcpServerSocketPort TcpServerSocketPort::Random() {
  return TcpServerSocketPort(GenerateRandomPort());
}

TcpServerSocketPort::TcpServerSocketPort()
    : TcpServerSocketPort(GenerateRandomPort()) {}

TcpServerSocketPort::TcpServerSocketPort(uint16_t port) : port_(port) {
  CHECK(IsPortInRange(port));
}

TcpServerSocketPort::~TcpServerSocketPort() = default;

}  // namespace nearby
}  // namespace ash
