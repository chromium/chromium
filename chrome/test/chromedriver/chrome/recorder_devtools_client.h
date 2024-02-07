// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_RECORDER_DEVTOOLS_CLIENT_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_RECORDER_DEVTOOLS_CLIENT_H_

#include <vector>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"

class Status;

struct Command {
  Command() = default;
  Command(const std::string& method, const base::Value::Dict& params)
      : method(method) {
    this->params = params.Clone();
  }
  Command(const Command& command) {
    *this = command;
  }
  Command& operator=(const Command& command) {
    method = command.method;
    params = command.params.Clone();
    return *this;
  }
  ~Command() = default;

  std::string method;
  base::Value::Dict params;
};

class RecorderDevToolsClient : public StubDevToolsClient {
 public:
  RecorderDevToolsClient();
  ~RecorderDevToolsClient() override;

  // Overridden from StubDevToolsClient:
  Status SendCommandAndGetResult(const std::string& method,
                                 const base::Value::Dict& params,
                                 base::Value::Dict* result) override;

  std::vector<Command> commands_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_RECORDER_DEVTOOLS_CLIENT_H_
