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
  Command() {}
  Command(const std::string& method, const base::DictionaryValue& params)
      : method(method) {
    this->params.MergeDictionary(&params);
  }
  Command(const Command& command) {
    *this = command;
  }
  Command& operator=(const Command& command) {
    method = command.method;
    params.DictClear();
    params.MergeDictionary(&command.params);
    return *this;
  }
  ~Command() {}

  std::string method;
  base::DictionaryValue params;
};

class RecorderDevToolsClient : public StubDevToolsClient {
 public:
  RecorderDevToolsClient();
  ~RecorderDevToolsClient() override;

  // Overridden from StubDevToolsClient:
  Status SendCommandAndGetResult(const std::string& method,
                                 const base::DictionaryValue& params,
                                 base::Value* result) override;

  std::vector<Command> commands_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_RECORDER_DEVTOOLS_CLIENT_H_
