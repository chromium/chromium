// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/command_listener_proxy.h"
#include "chrome/test/chromedriver/logging.h"

CommandListenerProxy::CommandListenerProxy(
    CommandListener* command_listener) : command_listener_(command_listener) {
  CHECK(command_listener_);
}

CommandListenerProxy::~CommandListenerProxy() { }

Status CommandListenerProxy::BeforeCommand(const std::string& command_name) {
  return command_listener_->BeforeCommand(command_name);
}
