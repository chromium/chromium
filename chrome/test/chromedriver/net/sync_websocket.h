// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_SYNC_WEBSOCKET_H_
#define CHROME_TEST_CHROMEDRIVER_NET_SYNC_WEBSOCKET_H_

#include <string>

#include "base/functional/callback.h"

class GURL;
class Timeout;

// Proxy for using a WebSocket running on a background thread synchronously.
class SyncWebSocket {
 public:
  enum class StatusCode { kOk = 0, kTimeout, kDisconnected };

  virtual ~SyncWebSocket() = default;

  virtual void SetId(const std::string& socket_id) {}

  // Return true if connected, otherwise return false.
  virtual bool IsConnected() = 0;

  // Connects to the WebSocket server. Returns true on success.
  virtual bool Connect(const GURL& url) = 0;

  // Sends message. Returns true on success.
  virtual bool Send(const std::string& message) = 0;

  // Receives next message and modifies the message on success. Returns
  // StatusCode::kTimedout if no message is received within |timeout|.
  // Returns StatusCode::kDisconnected if the socket is closed.
  virtual StatusCode ReceiveNextMessage(
      std::string* message,
      const Timeout& timeout) = 0;

  // Returns whether there are any messages that have been received and not yet
  // handled by ReceiveNextMessage.
  virtual bool HasNextMessage() = 0;

  // Set the callback to be executed if there any messages available.
  // The callback is called in the thread where the socket was created.
  // Sporadic calls of the callback are permitted.
  virtual void SetNotificationCallback(base::RepeatingClosure callback) {}
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_SYNC_WEBSOCKET_H_
