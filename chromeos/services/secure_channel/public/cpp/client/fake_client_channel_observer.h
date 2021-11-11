// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_OBSERVER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_OBSERVER_H_

#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"

namespace chromeos {

namespace secure_channel {

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

  void OnMessageReceived(const std::string& payload) override;

  bool is_disconnected() const { return is_disconnected_; }

  const std::vector<std::string>& received_messages() const {
    return received_messages_;
  }

 private:
  bool is_disconnected_ = false;
  std::vector<std::string> received_messages_;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CLIENT_CHANNEL_OBSERVER_H_
