// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_MANAGER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_MANAGER_H_

#include <vector>
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"

namespace chromeos {
namespace secure_channel {

class FakeConnectionManager : public secure_channel::ConnectionManager {
 public:
  FakeConnectionManager();
  ~FakeConnectionManager() override;

  using ConnectionManager::NotifyMessageReceived;

  void SetStatus(Status status);
  const std::vector<std::string>& sent_messages() const {
    return sent_messages_;
  }
  size_t num_attempt_connection_calls() const {
    return num_attempt_connection_calls_;
  }

  size_t num_disconnect_calls() const { return num_disconnect_calls_; }

 private:
  // ConnectionManager:
  Status GetStatus() const override;
  void AttemptNearbyConnection() override;
  void Disconnect() override;
  void SendMessage(const std::string& payload) override;

  Status status_;
  std::vector<std::string> sent_messages_;
  size_t num_attempt_connection_calls_ = 0;
  size_t num_disconnect_calls_ = 0;
};

}  // namespace secure_channel
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_MANAGER_H_
