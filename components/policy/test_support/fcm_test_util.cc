// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/fcm_test_util.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "components/policy/test_support/fake_fcm_server.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace fakedms {

namespace {

bool ReadExactBytesFromSocket(net::StreamSocket& socket,
                              base::span<uint8_t> dest) {
  while (!dest.empty()) {
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(dest.size());
    net::TestCompletionCallback callback;
    int rv = callback.GetResult(
        socket.Read(buffer.get(), buffer->size(), callback.callback()));
    if (rv <= 0) {
      return false;
    }
    auto [head, tail] = dest.split_at(static_cast<size_t>(rv));
    head.copy_from(buffer->span().first(head.size()));
    dest = tail;
  }
  return true;
}

bool ReadVarint32FromSocket(net::StreamSocket& socket, uint32_t* value) {
  *value = 0;
  for (int shift = 0; shift < 35; shift += 7) {
    uint8_t byte = 0;
    if (!ReadExactBytesFromSocket(socket, base::span_from_ref(byte))) {
      return false;
    }
    *value |= static_cast<uint32_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool WriteAllBytesToSocket(net::StreamSocket& socket, std::string data) {
  const size_t size = data.size();
  auto buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::StringIOBuffer>(std::move(data)), size);
  while (buffer->BytesRemaining() > 0) {
    net::TestCompletionCallback callback;
    int rv = callback.GetResult(
        socket.Write(buffer.get(), buffer->BytesRemaining(),
                     callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
    if (rv <= 0) {
      return false;
    }
    buffer->DidConsume(rv);
  }
  return true;
}

bool ReadFcmPacket(net::StreamSocket& socket,
                   uint8_t tag,
                   google::protobuf::MessageLite* message) {
  uint8_t read_tag = 0;
  uint32_t size = 0;
  if (!ReadExactBytesFromSocket(socket, base::span_from_ref(read_tag)) ||
      read_tag != tag || !ReadVarint32FromSocket(socket, &size)) {
    return false;
  }
  std::vector<uint8_t> payload(size);
  return ReadExactBytesFromSocket(socket, payload) &&
         message->ParseFromArray(payload.data(), static_cast<int>(size));
}

std::unique_ptr<net::TCPClientSocket> ConnectAndLoginFcmClient(
    int port,
    uint64_t android_id) {
  auto client = std::make_unique<net::TCPClientSocket>(
      net::AddressList(net::IPEndPoint(net::IPAddress::IPv4Localhost(), port)),
      nullptr, nullptr, nullptr, net::NetLogSource(),
      net::handles::kInvalidNetworkHandle);
  net::TestCompletionCallback callback;
  auto login_request =
      gcm::BuildLoginRequest(android_id, 12345, "test-version");
  uint8_t version = 0;
  mcs_proto::LoginResponse login_response;
  if (callback.GetResult(client->Connect(callback.callback())) != net::OK ||
      !WriteAllBytesToSocket(
          *client, EncodeFcmPacket(gcm::kLoginRequestTag, *login_request,
                                   /*with_version=*/true)) ||
      !ReadExactBytesFromSocket(*client, base::span_from_ref(version)) ||
      version != kFcmProtocolVersion ||
      !ReadFcmPacket(*client, gcm::kLoginResponseTag, &login_response)) {
    return nullptr;
  }
  return client;
}

}  // namespace fakedms
