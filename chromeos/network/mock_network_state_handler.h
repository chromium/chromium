// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MOCK_NETWORK_STATE_HANDLER_H_
#define CHROMEOS_NETWORK_MOCK_NETWORK_STATE_HANDLER_H_

#include "chromeos/network/network_state_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) MockNetworkStateHandler
    : public NetworkStateHandler {
 public:
  MockNetworkStateHandler();

  MockNetworkStateHandler(const MockNetworkStateHandler&) = delete;
  MockNetworkStateHandler& operator=(const MockNetworkStateHandler&) = delete;

  virtual ~MockNetworkStateHandler();

  // Constructs and initializes an instance for testing.
  static std::unique_ptr<MockNetworkStateHandler> InitializeForTest();

  // NetworkStateHandler overrides
  MOCK_METHOD3(UpdateBlockedWifiNetworks,
               void(bool, bool, const std::vector<std::string>&));

  MOCK_METHOD1(UpdateBlockedCellularNetworks, void(bool));
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_MOCK_NETWORK_STATE_HANDLER_H_