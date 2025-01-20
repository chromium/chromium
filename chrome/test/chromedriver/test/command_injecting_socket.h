// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_TEST_COMMAND_INJECTING_SOCKET_H_
#define CHROME_TEST_CHROMEDRIVER_TEST_COMMAND_INJECTING_SOCKET_H_

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/values.h"
#include "chrome/test/chromedriver/test/sync_websocket_wrapper.h"

class CommandInjectingSocket : public SyncWebSocketWrapper {
 public:
  explicit CommandInjectingSocket(
      std::unique_ptr<SyncWebSocket> wrapped_socket);

  StatusCode ReceiveNextMessage(std::string* message,
                                const Timeout& timeout) override;

  bool IsSaturated() const;

  bool Send(const std::string& message) override;

  void SetMethod(std::string method);

  void SetParams(base::Value::Dict params);

  void SetSessionId(std::string session_id);

  void SetSkipCount(int count);

 protected:
  bool InterceptResponse(const std::string& message);

  base::Value::Dict params_;
  std::string method_;
  std::string session_id_;
  int next_cmd_id = 1000'000'000;
  int skip_count_ = 1000'000'000;
};

#endif  // CHROME_TEST_CHROMEDRIVER_TEST_COMMAND_INJECTING_SOCKET_H_
