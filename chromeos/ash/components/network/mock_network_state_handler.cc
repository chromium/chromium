// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/mock_network_state_handler.h"

namespace ash {

MockNetworkStateHandler::MockNetworkStateHandler() = default;

MockNetworkStateHandler::~MockNetworkStateHandler() = default;

// static
std::unique_ptr<MockNetworkStateHandler>
MockNetworkStateHandler::InitializeForTest() {
  auto handler = std::make_unique<testing::NiceMock<MockNetworkStateHandler>>();
  handler->InitShillPropertyHandler();
  return handler;
}

}  // namespace ash
