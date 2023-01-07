// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_NETWORK_STATE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_NETWORK_STATE_HANDLER_H_

#include "chromeos/ash/components/network/network_state_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) MockNetworkStateHandler
    : public NetworkStateHandler {
 public:
  MockNetworkStateHandler();

  MockNetworkStateHandler(const MockNetworkStateHandler&) = delete;
  MockNetworkStateHandler& operator=(const MockNetworkStateHandler&) = delete;

  ~MockNetworkStateHandler() override;

  // Constructs and initializes an instance for testing.
  static std::unique_ptr<MockNetworkStateHandler> InitializeForTest();

  // NetworkStateHandler overrides
  MOCK_METHOD3(UpdateBlockedWifiNetworks,
               void(bool, bool, const std::vector<std::string>&));

  MOCK_METHOD1(UpdateBlockedCellularNetworks, void(bool));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_MOCK_NETWORK_STATE_HANDLER_H_
