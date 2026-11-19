// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_FCM_TEST_UTIL_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_FCM_TEST_UTIL_H_

#include <stdint.h>

#include <memory>
#include <string>

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace net {
class StreamSocket;
class TCPClientSocket;
}  // namespace net

namespace fakedms {

// Test helpers for FCM client sockets. They block until the operation is done.
bool WriteAllBytesToSocket(net::StreamSocket& socket, std::string data);
// Reads one packet. Returns false if its tag is not `tag` or parsing fails.
bool ReadFcmPacket(net::StreamSocket& socket,
                   uint8_t tag,
                   google::protobuf::MessageLite* message);
// Connects to 127.0.0.1:`port` and completes the FCM login handshake.
std::unique_ptr<net::TCPClientSocket> ConnectAndLoginFcmClient(
    int port,
    uint64_t android_id);

}  // namespace fakedms

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_FCM_TEST_UTIL_H_
