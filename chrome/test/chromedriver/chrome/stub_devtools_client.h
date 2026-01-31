// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_STUB_DEVTOOLS_CLIENT_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_STUB_DEVTOOLS_CLIENT_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"

class Status;

class StubDevToolsClient : public DevToolsClient {
 public:
  explicit StubDevToolsClient(const std::string& id);
  StubDevToolsClient();
  ~StubDevToolsClient() override;

  // Overridden from DevToolsClient:
  const std::string& GetId() override;
  const std::string& SessionId() const override;
  const std::string& TunnelSessionId() const override;
  Status SetTunnelSessionId(std::string session_id) override;
  Status StartBidiServer(std::string bidi_mapper_script,
                         bool enable_unsafe_extension_debugging) override;
  bool IsNull() const override;
  bool WasCrashed() override;
  bool IsConnected() const override;
  bool IsDialogOpen() const override;
  bool AutoAcceptsBeforeunload() const override;
  void SetAutoAcceptBeforeunload(bool value) override;
  Status PostBidiCommand(base::DictValue command) override;
  Status SendCommand(const std::string& method,
                     const base::DictValue& params) override;
  Status SendCommandFromWebSocket(const std::string& method,
                                  const base::DictValue& params,
                                  const int client_command_id) override;
  Status SendCommandWithTimeout(const std::string& method,
                                const base::DictValue& params,
                                const Timeout* timeout) override;
  Status SendAsyncCommand(const std::string& method,
                          const base::DictValue& params) override;
  Status SendCommandAndGetResult(const std::string& method,
                                 const base::DictValue& params,
                                 base::DictValue* result) override;
  Status SendCommandAndGetResultWithTimeout(const std::string& method,
                                            const base::DictValue& params,
                                            const Timeout* timeout,
                                            base::DictValue* result) override;
  Status SendCommandAndIgnoreResponse(const std::string& method,
                                      const base::DictValue& params) override;
  void AddListener(DevToolsEventListener* listener) override;
  void RemoveListener(DevToolsEventListener* listener) override;
  Status HandleEventsUntil(const ConditionalFunc& conditional_func,
                           const Timeout& timeout) override;
  Status HandleReceivedEvents() override;
  void SetDetached() override;
  void SetOwner(WebViewImpl* owner) override;
  WebViewImpl* GetOwner() const override;
  DevToolsClient* GetParentClient() const override;
  bool IsMainPage() const override;
  bool IsTabTarget() const override;
  Status SendRaw(const std::string& message) override;
  bool HasMessageForAnySession() const override;

  Status AttachTo(DevToolsClient* parent) override;
  void RegisterSessionHandler(const std::string& session_id,
                              DevToolsClient* client) override;
  void UnregisterSessionHandler(const std::string& session_id) override;
  Status OnConnected() override;
  Status ProcessEvent(InspectorEvent event) override;
  Status ProcessCommandResponse(InspectorCommandResponse response) override;
  int NextMessageId() const override;
  int AdvanceNextMessageId() override;
  Status ProcessNextMessage(int expected_id,
                            bool log_timeout,
                            const Timeout& timeout,
                            DevToolsClient* caller) override;
  Status GetDialogMessage(std::string& message) const override;
  Status GetTypeOfDialog(std::string& type) const override;
  Status HandleDialog(bool accept,
                      const std::optional<std::string>& text) override;

 protected:
  const std::string id_;
  std::string session_id_;
  std::string tunnel_session_id_;
  std::list<raw_ptr<DevToolsEventListener, CtnExperimental>> listeners_;
  raw_ptr<WebViewImpl> owner_ = nullptr;
  bool is_connected_ = false;
  bool autoaccept_beforeunload_ = false;
  bool is_tab_ = false;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_STUB_DEVTOOLS_CLIENT_H_
