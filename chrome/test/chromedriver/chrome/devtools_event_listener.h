// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_EVENT_LISTENER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_EVENT_LISTENER_H_

#include <string>

namespace base {
class DictionaryValue;
}

class DevToolsClient;
class Status;
class Timeout;

// Receives notification of incoming Blink Inspector messages and connection
// to the DevTools server.
class DevToolsEventListener {
 public:
  virtual ~DevToolsEventListener();

  // Called when a connection is made to the DevTools server.
  virtual Status OnConnected(DevToolsClient* client);

  // Called when an event is received. Should avoid blocking if possible.
  virtual Status OnEvent(DevToolsClient* client,
                         const std::string& method,
                         const base::DictionaryValue& params);

  // Called when a command success response is received.
  virtual Status OnCommandSuccess(DevToolsClient* client,
                                  const std::string& method,
                                  const base::DictionaryValue& result,
                                  const Timeout& command_timeout);

  // True if the listener should be added to the browser-wide |DevToolsClient|
  // in addition to all webview |DevToolsClient|s. False by default. If set to
  // true, listener can use |client|->GetId() to distinguish between browser-
  // wide |DevToolsClient| and webview |DevToolsClient|s.
  virtual bool subscribes_to_browser();
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_EVENT_LISTENER_H_
