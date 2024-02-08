// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_OBSERVER_H_

#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"

namespace ash::secure_channel {

// Test double implementation of ClientChannel::Observer.
class FakeClientChannelObserver : public ClientChannel::Observer {
 public:
  FakeClientChannelObserver();

  FakeClientChannelObserver(const FakeClientChannelObserver&) = delete;
  FakeClientChannelObserver& operator=(const FakeClientChannelObserver&) =
      delete;

  ~FakeClientChannelObserver() override;

  // ClientChannel::Observer:
  void OnDisconnected() override;
  void OnNearbyConnectionStateChagned(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;

  void OnMessageReceived(const std::string& payload) override;

  bool is_disconnected() const { return is_disconnected_; }

  const std::vector<std::string>& received_messages() const {
    return received_messages_;
  }

  mojom::NearbyConnectionStep nearby_connection_step() {
    return nearby_connection_step_;
  }

  mojom::NearbyConnectionStepResult nearby_connection_step_result() {
    return nearby_connection_step_result_;
  }

 private:
  bool is_disconnected_ = false;
  std::vector<std::string> received_messages_;
  mojom::NearbyConnectionStep nearby_connection_step_;
  mojom::NearbyConnectionStepResult nearby_connection_step_result_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_OBSERVER_H_
