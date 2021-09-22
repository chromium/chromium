// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_DISCONNECTOR_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_DISCONNECTOR_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chromeos/components/tether/tether_disconnector.h"
#include "chromeos/components/tether/tether_session_completion_logger.h"
#include "chromeos/network/network_connection_handler.h"

namespace chromeos {

namespace tether {

// Test double for TetherDisconnector.
class FakeTetherDisconnector : public TetherDisconnector {
 public:
  FakeTetherDisconnector();

  FakeTetherDisconnector(const FakeTetherDisconnector&) = delete;
  FakeTetherDisconnector& operator=(const FakeTetherDisconnector&) = delete;

  ~FakeTetherDisconnector() override;

  std::string last_disconnected_tether_network_guid() {
    return last_disconnected_tether_network_guid_;
  }

  void set_disconnection_error_name(
      const std::string& disconnection_error_name) {
    disconnection_error_name_ = disconnection_error_name;
  }

  TetherSessionCompletionLogger::SessionCompletionReason*
  last_session_completion_reason() {
    return last_session_completion_reason_.get();
  }

  // TetherDisconnector:
  void DisconnectFromNetwork(
      const std::string& tether_network_guid,
      base::OnceClosure success_callback,
      StringErrorCallback error_callback,
      const TetherSessionCompletionLogger::SessionCompletionReason&
          session_completion_reason) override;

 private:
  std::string last_disconnected_tether_network_guid_;
  std::string disconnection_error_name_;
  std::unique_ptr<TetherSessionCompletionLogger::SessionCompletionReason>
      last_session_completion_reason_;
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_DISCONNECTOR_H_
