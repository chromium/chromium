// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_STUB_DEVTOOLS_CLIENT_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_STUB_DEVTOOLS_CLIENT_H_

#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"

namespace base {
class DictionaryValue;
}

class Status;

class StubDevToolsClient : public DevToolsClient {
 public:
  explicit StubDevToolsClient(const std::string& id);
  StubDevToolsClient();
  ~StubDevToolsClient() override;

  // Overridden from DevToolsClient:
  const std::string& GetId() override;
  bool WasCrashed() override;
  Status ConnectIfNecessary() override;
  Status SendCommand(
      const std::string& method,
      const base::DictionaryValue& params) override;
  Status SendCommandFromWebSocket(const std::string& method,
                                  const base::DictionaryValue& params,
                                  const int client_command_id) override;
  Status SendCommandWithTimeout(
      const std::string& method,
      const base::DictionaryValue& params,
      const Timeout* timeout) override;
  Status SendAsyncCommand(
      const std::string& method,
      const base::DictionaryValue& params) override;
  Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      std::unique_ptr<base::DictionaryValue>* result) override;
  Status SendCommandAndGetResultWithTimeout(
      const std::string& method,
      const base::DictionaryValue& params,
      const Timeout* timeout,
      std::unique_ptr<base::DictionaryValue>* result) override;
  Status SendCommandAndIgnoreResponse(
      const std::string& method,
      const base::DictionaryValue& params) override;
  void AddListener(DevToolsEventListener* listener) override;
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override;
  Status HandleReceivedEvents() override;
  void SetDetached() override;
  void SetOwner(WebViewImpl* owner) override;

 protected:
  const std::string id_;
  std::list<DevToolsEventListener*> listeners_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_STUB_DEVTOOLS_CLIENT_H_
