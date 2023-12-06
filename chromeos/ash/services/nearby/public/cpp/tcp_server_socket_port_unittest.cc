// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace nearby {
namespace {

TEST(TcpServerSocketPortTest, FromInt) {
  // Can't convert input to uint16_t.
  EXPECT_FALSE(TcpServerSocketPort::FromInt(-1));

  // Outside of restricted range.
  EXPECT_FALSE(TcpServerSocketPort::FromInt(
      static_cast<int>(TcpServerSocketPort::kMin) - 1));
  EXPECT_FALSE(TcpServerSocketPort::FromInt(
      static_cast<int>(TcpServerSocketPort::kMax) + 1));

  // Inside restricted range.
  std::optional<TcpServerSocketPort> port =
      TcpServerSocketPort::FromInt(TcpServerSocketPort::kMin);
  EXPECT_TRUE(port);
  EXPECT_EQ(TcpServerSocketPort::kMin, port->port());
  port = TcpServerSocketPort::FromInt(TcpServerSocketPort::kMax);
  EXPECT_TRUE(port);
  EXPECT_EQ(TcpServerSocketPort::kMax, port->port());
}

TEST(TcpServerSocketPortTest, FromUInt16) {
  // Outside of restricted range. Note: kMax is currently the maximum uint16_t,
  // so we can't test exceeding that bound.
  EXPECT_FALSE(TcpServerSocketPort::FromUInt16(TcpServerSocketPort::kMin - 1));

  // Inside restricted range.
  std::optional<TcpServerSocketPort> port =
      TcpServerSocketPort::FromUInt16(TcpServerSocketPort::kMin);
  EXPECT_TRUE(port);
  EXPECT_EQ(TcpServerSocketPort::kMin, port->port());
  port = TcpServerSocketPort::FromUInt16(TcpServerSocketPort::kMax);
  EXPECT_TRUE(port);
  EXPECT_EQ(TcpServerSocketPort::kMax, port->port());
}

TEST(TcpServerSocketPortTest, Random) {
  // Random value is inside restricted range.
  TcpServerSocketPort port = TcpServerSocketPort::Random();
  EXPECT_GT(port.port(), TcpServerSocketPort::kMin);
  EXPECT_LT(port.port(), TcpServerSocketPort::kMax);
}

}  // namespace
}  // namespace nearby
}  // namespace ash
