// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_OVERRIDE_MANAGER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_OVERRIDE_MANAGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

namespace base {
class DictionaryValue;
}

class DevToolsClient;
struct NetworkConditions;
class Status;

// Overrides the network conditions, if requested, for the duration of the
// given |DevToolsClient|'s lifetime.
class NetworkConditionsOverrideManager : public DevToolsEventListener {
 public:
  explicit NetworkConditionsOverrideManager(DevToolsClient* client);
  ~NetworkConditionsOverrideManager() override;

  Status OverrideNetworkConditions(const NetworkConditions& network_conditions);

  // Overridden from DevToolsEventListener:
  Status OnConnected(DevToolsClient* client) override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override;

 private:
  Status ApplyOverrideIfNeeded();
  Status ApplyOverride(const NetworkConditions* network_conditions);

  DevToolsClient* client_;
  const NetworkConditions* overridden_network_conditions_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConditionsOverrideManager);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_OVERRIDE_MANAGER_H_
