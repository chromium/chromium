// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstring>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "chromecast/net/fake_stream_socket.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

namespace {

const char kIpAddress1[] = "192.168.0.1";
const uint16_t kPort1 = 10001;
const char kIpAddress2[] = "192.168.0.2";
const uint16_t kPort2 = 10002;

net::IPAddress IpLiteralToIpAddress(const std::string& ip_literal) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(ip_literal));
  return ip_address;
}

void Callback(int error_code) {}

}  // namespace

class FakeStreamSocketTest : public ::testing::Test {
 public:
  FakeStreamSocketTest()
      : endpoint_1_(IpLiteralToIpAddress(kIpAddress1), kPort1),
        socket_1_(endpoint_1_),
        endpoint_2_(IpLiteralToIpAddress(kIpAddress2), kPort2),
        socket_2_(endpoint_2_) {}
  ~FakeStreamSocketTest() override {}

  base::test::TaskEnvironment task_environment_;
  net::IPEndPoint endpoint_1_;
  FakeStreamSocket socket_1_;
  net::IPEndPoint endpoint_2_;
  FakeStreamSocket socket_2_;
};

TEST_F(FakeStreamSocketTest, GetLocalAddress) {
  net::IPEndPoint local_address;
  ASSERT_EQ(net::OK, socket_1_.GetLocalAddress(&local_address));
  EXPECT_EQ(endpoint_1_, local_address);
}

TEST_F(FakeStreamSocketTest, GetPeerAddressWithoutPeer) {
  net::IPEndPoint peer_address;
  EXPECT_EQ(net::ERR_SOCKET_NOT_CONNECTED,
            socket_1_.GetPeerAddress(&peer_address));
}

TEST_F(FakeStreamSocketTest, GetPeerAddressWithPeer) {
  socket_1_.SetPeer(&socket_2_);
  socket_2_.SetPeer(&socket_1_);
  net::IPEndPoint peer_address;
  ASSERT_EQ(net::OK, socket_1_.GetPeerAddress(&peer_address));
  EXPECT_EQ(endpoint_2_, peer_address);
}

TEST_F(FakeStreamSocketTest, ReadAndWriteWithoutPeer) {
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  EXPECT_EQ(net::ERR_IO_PENDING,
            socket_1_.Read(io_buffer.get(), 1, base::BindOnce(&Callback)));
  EXPECT_EQ(net::ERR_SOCKET_NOT_CONNECTED,
            socket_1_.Write(io_buffer.get(), 1, base::BindOnce(&Callback),
                            TRAFFIC_ANNOTATION_FOR_TESTS));
}

TEST_F(FakeStreamSocketTest, ReadAndWriteWithPeer) {
  socket_1_.SetPeer(&socket_2_);
  socket_2_.SetPeer(&socket_1_);
  const std::string kData("DATA");
  auto send_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  ASSERT_EQ(
      static_cast<int>(kData.size()),
      socket_1_.Write(send_buffer.get(), kData.size(),
                      base::BindOnce(&Callback), TRAFFIC_ANNOTATION_FOR_TESTS));
  auto receive_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  ASSERT_EQ(static_cast<int>(kData.size()),
            socket_2_.Read(receive_buffer.get(), kData.size(),
                           base::BindOnce(&Callback)));
  EXPECT_EQ(0, std::memcmp(kData.data(), receive_buffer->data(), kData.size()));
}

TEST_F(FakeStreamSocketTest, ReadAndWritePending) {
  socket_1_.SetPeer(&socket_2_);
  socket_2_.SetPeer(&socket_1_);
  const std::string kData("DATA");
  auto receive_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kData.size());
  ASSERT_EQ(net::ERR_IO_PENDING,
            socket_2_.Read(receive_buffer.get(), kData.size(),
                           base::BindOnce(&Callback)));
  auto send_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  ASSERT_EQ(
      static_cast<int>(kData.size()),
      socket_1_.Write(send_buffer.get(), kData.size(),
                      base::BindOnce(&Callback), TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(0, std::memcmp(kData.data(), receive_buffer->data(), kData.size()));
}

TEST_F(FakeStreamSocketTest, ReadAndWriteLargeData) {
  socket_1_.SetPeer(&socket_2_);
  socket_2_.SetPeer(&socket_1_);
  // Send 1 MB of data between sockets.
  const std::string kData("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef");
  auto send_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  const int kWriteCount = 1024 * 1024 / kData.size();
  for (int i = 0; i < kWriteCount; i++) {
    ASSERT_EQ(static_cast<int>(kData.size()),
              socket_1_.Write(send_buffer.get(), kData.size(),
                              base::BindOnce(&Callback),
                              TRAFFIC_ANNOTATION_FOR_TESTS));
  }
  auto receive_buffer = base::MakeRefCounted<net::IOBufferWithSize>(1024);
  for (int i = 0; i < 1024; i++) {
    ASSERT_EQ(1024, socket_2_.Read(receive_buffer.get(), 1024,
                                   base::BindOnce(&Callback)));
  }
}

}  // namespace chromecast
