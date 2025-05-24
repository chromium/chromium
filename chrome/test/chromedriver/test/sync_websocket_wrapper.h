// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_TEST_SYNC_WEBSOCKET_WRAPPER_H_
#define CHROME_TEST_CHROMEDRIVER_TEST_SYNC_WEBSOCKET_WRAPPER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"

class SyncWebSocketWrapper : public SyncWebSocket {
 public:
  explicit SyncWebSocketWrapper(std::unique_ptr<SyncWebSocket> wrapped_socket);

  ~SyncWebSocketWrapper() override;

  void SetId(const std::string& socket_id) override;

  bool IsConnected() override;

  bool Connect(const GURL& url) override;

  bool Send(const std::string& message) override;

  StatusCode ReceiveNextMessage(std::string* message,
                                const Timeout& timeout) override;

  bool HasNextMessage() override;

  void SetNotificationCallback(base::RepeatingClosure callback) override;

 protected:
  std::unique_ptr<SyncWebSocket> wrapped_socket_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_TEST_SYNC_WEBSOCKET_WRAPPER_H_
