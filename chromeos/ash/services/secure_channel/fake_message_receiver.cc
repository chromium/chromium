// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_message_receiver.h"

namespace ash::secure_channel {

FakeMessageReceiver::FakeMessageReceiver() = default;

FakeMessageReceiver::~FakeMessageReceiver() = default;

void FakeMessageReceiver::OnMessageReceived(const std::string& message) {
  received_messages_.push_back(message);
}

}  // namespace ash::secure_channel
