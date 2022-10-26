// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_MESSAGE_RECEIVER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_MESSAGE_RECEIVER_H_

#include <string>
#include <vector>

#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace ash::secure_channel {

// Test MessageReceiver implementation.
class FakeMessageReceiver : public mojom::MessageReceiver {
 public:
  FakeMessageReceiver();

  FakeMessageReceiver(const FakeMessageReceiver&) = delete;
  FakeMessageReceiver& operator=(const FakeMessageReceiver&) = delete;

  ~FakeMessageReceiver() override;

  const std::vector<std::string>& received_messages() {
    return received_messages_;
  }

 private:
  // mojom::MessageReceiver:
  void OnMessageReceived(const std::string& message) override;

  std::vector<std::string> received_messages_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_MESSAGE_RECEIVER_H_
