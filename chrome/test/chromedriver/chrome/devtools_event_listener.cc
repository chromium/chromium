// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

#include "chrome/test/chromedriver/chrome/status.h"

DevToolsEventListener::~DevToolsEventListener() {}

bool DevToolsEventListener::ListensToConnections() const {
  return true;
}

Status DevToolsEventListener::OnConnected(DevToolsClient* client) {
  return Status(kOk);
}

Status DevToolsEventListener::OnEvent(DevToolsClient* client,
                                      const std::string& method,
                                      const base::Value::Dict& params) {
  return Status(kOk);
}

Status DevToolsEventListener::OnCommandSuccess(DevToolsClient* client,
                                               const std::string& method,
                                               const base::Value::Dict* result,
                                               const Timeout& command_timeout) {
  return Status(kOk);
}

bool DevToolsEventListener::subscribes_to_browser() {
  return false;
}
