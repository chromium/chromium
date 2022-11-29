// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_OVERRIDE_MANAGER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_OVERRIDE_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

class DevToolsClient;
struct NetworkConditions;
class Status;

// Overrides the network conditions, if requested, for the duration of the
// given |DevToolsClient|'s lifetime.
class NetworkConditionsOverrideManager : public DevToolsEventListener {
 public:
  explicit NetworkConditionsOverrideManager(DevToolsClient* client);

  NetworkConditionsOverrideManager(const NetworkConditionsOverrideManager&) =
      delete;
  NetworkConditionsOverrideManager& operator=(
      const NetworkConditionsOverrideManager&) = delete;

  ~NetworkConditionsOverrideManager() override;

  Status OverrideNetworkConditions(const NetworkConditions& network_conditions);

  // Overridden from DevToolsEventListener:
  Status OnConnected(DevToolsClient* client) override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

 private:
  Status ApplyOverrideIfNeeded();
  Status ApplyOverride(const NetworkConditions* network_conditions);

  raw_ptr<DevToolsClient> client_;
  raw_ptr<const NetworkConditions> overridden_network_conditions_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_NETWORK_CONDITIONS_OVERRIDE_MANAGER_H_
