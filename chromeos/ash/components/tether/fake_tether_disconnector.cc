// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_tether_disconnector.h"

namespace ash {

namespace tether {

FakeTetherDisconnector::FakeTetherDisconnector() = default;

FakeTetherDisconnector::~FakeTetherDisconnector() = default;

void FakeTetherDisconnector::DisconnectFromNetwork(
    const std::string& tether_network_guid,
    base::OnceClosure success_callback,
    StringErrorCallback error_callback,
    const TetherSessionCompletionLogger::SessionCompletionReason&
        session_completion_reason) {
  last_disconnected_tether_network_guid_ = tether_network_guid;
  last_session_completion_reason_ =
      std::make_unique<TetherSessionCompletionLogger::SessionCompletionReason>(
          session_completion_reason);

  if (disconnection_error_name_.empty())
    std::move(success_callback).Run();
  else
    std::move(error_callback).Run(disconnection_error_name_);
}

}  // namespace tether

}  // namespace ash
