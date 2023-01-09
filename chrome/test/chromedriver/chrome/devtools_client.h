// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"

class DevToolsEventListener;
class Timeout;
class Status;
class WebViewImpl;

// A DevTools client of a single DevTools debugger.
class DevToolsClient {
 public:
  typedef base::RepeatingCallback<Status(bool* is_condition_met)>
      ConditionalFunc;

  virtual ~DevToolsClient() = default;

  virtual const std::string& GetId() = 0;

  // Session id used to annotate the CDP commands.
  virtual const std::string& SessionId() const = 0;

  // Session id used for CDP traffic tunneling
  virtual const std::string& TunnelSessionId() const = 0;

  // Set the session id used for CDP traffic tunneling
  virtual Status SetTunnelSessionId(std::string session_id) = 0;

  // Start a BiDi Server in the connected target
  // Precondition: IsMainPage()
  // Precondition: IsConnected()
  // Precondition: BiDi tunnel for CDP traffic is not set.
  virtual Status StartBidiServer(std::string bidi_mapper_script) = 0;

  virtual bool WasCrashed() = 0;

  virtual bool IsNull() const = 0;

  virtual bool IsConnected() const = 0;

  // Connect to DevTools if the DevToolsClient is disconnected.
  virtual Status Connect() = 0;

  virtual Status PostBidiCommand(base::Value::Dict command) = 0;

  virtual Status SendCommand(const std::string& method,
                             const base::Value::Dict& params) = 0;

  virtual Status SendCommandFromWebSocket(const std::string& method,
                                          const base::Value::Dict& params,
                                          const int client_command_id) = 0;

  virtual Status SendCommandWithTimeout(const std::string& method,
                                        const base::Value::Dict& params,
                                        const Timeout* timeout) = 0;

  virtual Status SendAsyncCommand(const std::string& method,
                                  const base::Value::Dict& params) = 0;

  virtual Status SendCommandAndGetResult(const std::string& method,
                                         const base::Value::Dict& params,
                                         base::Value::Dict* result) = 0;

  virtual Status SendCommandAndGetResultWithTimeout(
      const std::string& method,
      const base::Value::Dict& params,
      const Timeout* timeout,
      base::Value::Dict* result) = 0;

  virtual Status SendCommandAndIgnoreResponse(
      const std::string& method,
      const base::Value::Dict& params) = 0;

  // Adds a listener. This must only be done when the client is disconnected.
  virtual void AddListener(DevToolsEventListener* listener) = 0;

  // Handles events until the given function reports the condition is met
  // and there are no more received events to handle. If the given
  // function ever returns an error, returns immediately with the error.
  // If the condition is not met within |timeout|, kTimeout status
  // is returned eventually. If |timeout| is 0, this function will not block.
  virtual Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                                   const Timeout& timeout) = 0;

  // Handles events that have been received but not yet handled.
  virtual Status HandleReceivedEvents() = 0;

  // Indicate that we've been detached from the DevTools target.
  virtual void SetDetached() = 0;

  // Set the owning WebViewImpl, if any.
  virtual void SetOwner(WebViewImpl* owner) = 0;

  virtual WebViewImpl* GetOwner() const = 0;

  virtual DevToolsClient* GetRootClient() = 0;

  virtual DevToolsClient* GetParentClient() const = 0;

  virtual bool IsMainPage() const = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_H_
