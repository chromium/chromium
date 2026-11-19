// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_FAKE_FCM_SERVER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_FAKE_FCM_SERVER_H_

#include <stdint.h>

#include <atomic>
#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "google_apis/gcm/protocol/mcs.pb.h"

namespace net {
class StreamSocket;
class TCPServerSocket;
}  // namespace net

namespace fakedms {

// Version byte that starts the login messages.
inline constexpr uint8_t kFcmProtocolVersion = 41;

// Serializes `message` as an MCS packet: an optional version byte, `tag`, a
// varint32 size and the payload.
std::string EncodeFcmPacket(uint8_t tag,
                            const google::protobuf::MessageLite& message,
                            bool with_version = false);

// Fake server that simulates a Mobile Connection Server (MCS) endpoint
// for FCM push notifications. It accepts connections, performs MCS handshakes,
// and broadcasts downstream push messages to all the clients.
//
// Threading model:
// - Must be constructed, started, stopped, queried for port(), and destroyed
//   on a single IO-capable sequence.
// - BroadcastDataMessage() and HasLoggedInClients() may be called from any
//   sequence. Cross-sequence broadcasts are posted to the server's sequence.
class FakeFcmServer {
 public:
  FakeFcmServer();
  FakeFcmServer(const FakeFcmServer&) = delete;
  FakeFcmServer& operator=(const FakeFcmServer&) = delete;
  ~FakeFcmServer();

  // Starts listening on 127.0.0.1 on the specified port.
  // If `port` is 0, a free OS-assigned port is used.
  bool Start(int port = 0);

  void Stop();

  int port() const;

  // Broadcasts a push data message to all connected clients that completed
  // the MCS handshake. Safe to call from any sequence; calls from other
  // sequences are posted to the server's sequence.
  void BroadcastDataMessage(const mcs_proto::DataMessageStanza& message);

  // Returns true if at least one client has completed the MCS handshake.
  // Safe to call from any sequence.
  bool HasLoggedInClients() const;

 private:
  class ClientConnection;

  void OnAcceptComplete(int result);
  void AcceptNextConnection();
  bool HandleAcceptResult(int result);
  void RemoveConnection(ClientConnection* connection);
  void OnClientLoggedIn();

  SEQUENCE_CHECKER(sequence_checker_);

  int port_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  std::unique_ptr<net::TCPServerSocket> server_socket_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<net::StreamSocket> accepted_socket_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::set<std::unique_ptr<ClientConnection>, base::UniquePtrComparator>
      connections_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Number of connections that completed the MCS handshake. Written only on
  // `task_runner_`, read from any sequence.
  std::atomic<int> logged_in_count_{0};

  // Set in the constructor and never changed, so that any sequence can read
  // them.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<FakeFcmServer> weak_ptr_;
  base::WeakPtrFactory<FakeFcmServer> weak_ptr_factory_{this};
};

}  // namespace fakedms

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_FAKE_FCM_SERVER_H_
