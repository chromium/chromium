// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMAND_LISTENER_PROXY_H_
#define CHROME_TEST_CHROMEDRIVER_COMMAND_LISTENER_PROXY_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/test/chromedriver/command_listener.h"

class CommandListenerProxy : public CommandListener {
 public:
  CommandListenerProxy(const CommandListenerProxy&) = delete;
  CommandListenerProxy& operator=(const CommandListenerProxy&) = delete;

  ~CommandListenerProxy() override;

  // |command_listener| must not be null.
  explicit CommandListenerProxy(CommandListener* command_listener);

  // Forwards commands to |command_listener_|.
  Status BeforeCommand(const std::string& command_name) override;

 private:
  // This dangling raw_ptr occurred in:
  // chromedriver_unittests: CommandsTest.SuccessNotifyingCommandListeners
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/1425111/test-results?q=ExactID%3Aninja%3A%2F%2Fchrome%2Ftest%2Fchromedriver%3Achromedriver_unittests%2FCommandsTest.SuccessNotifyingCommandListeners+VHash%3A2f0b3a347eef5911&sortby=&groupby=
  const raw_ptr<CommandListener, FlakyDanglingUntriaged> command_listener_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_COMMAND_LISTENER_PROXY_H_
