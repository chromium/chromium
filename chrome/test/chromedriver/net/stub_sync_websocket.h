// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_STUB_SYNC_WEBSOCKET_H_
#define CHROME_TEST_CHROMEDRIVER_NET_STUB_SYNC_WEBSOCKET_H_

#include <limits>
#include <queue>
#include <string>
#include <unordered_map>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"

// Proxy for using a WebSocket running on a background thread synchronously.
class StubSyncWebSocket : public SyncWebSocket {
 public:
  using CommandHandler = base::RepeatingCallback<
      bool(int, const base::Value::Dict&, base::Value::Dict&)>;

  StubSyncWebSocket();

  ~StubSyncWebSocket() override;

  // Return true if connected, otherwise return false.
  bool IsConnected() override;

  // Connects to the WebSocket server. Returns true on success.
  bool Connect(const GURL& url) override;

  // Sends message. Returns true on success.
  bool Send(const std::string& message) override;

  // Receives next message and modifies the message on success. Returns
  // StatusCode::kTimedout if no message is received within |timeout|.
  // Returns StatusCode::kDisconnected if the socket is closed.
  SyncWebSocket::StatusCode ReceiveNextMessage(std::string* message,
                                               const Timeout& timeout) override;

  // Returns whether there are any messages that have been received and not yet
  // handled by ReceiveNextMessage.
  bool HasNextMessage() override;

  void AddCommandHandler(const std::string& method, CommandHandler handler);

  void EnqueueResponse(const std::string& message);

  void Disconnect();

  base::RepeatingClosure DisconnectClosure();

  void NotifyOnEmptyQueue(base::RepeatingClosure callback);

  void SetResponseLimit(int count);

 protected:
  void GenerateDefaultResponse(int cmd_id, base::Value::Dict& response);

  void EnqueueHandshakeResponse(int cmd_id, const std::string& method);

  bool PopMessage(std::string* dest);

  bool connected_ = false;
  bool handshake_add_script_handled_ = false;
  bool handshake_runtime_eval_handled_ = false;
  bool handshake_page_enable_handled_ = false;
  bool connect_complete_ = false;
  int response_limit_ = std::numeric_limits<int>::max();
  std::queue<std::string> queued_response_;
  std::unordered_map<std::string, CommandHandler> command_handlers_;
  base::RepeatingClosure on_empty_queue_;
  // must be the last member
  base::WeakPtrFactory<StubSyncWebSocket> weak_ptr_factory_{this};
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_STUB_SYNC_WEBSOCKET_H_
