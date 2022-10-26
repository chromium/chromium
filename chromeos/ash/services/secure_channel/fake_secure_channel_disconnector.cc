// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_secure_channel_disconnector.h"

namespace ash::secure_channel {

FakeSecureChannelDisconnector::FakeSecureChannelDisconnector() = default;

FakeSecureChannelDisconnector::~FakeSecureChannelDisconnector() = default;

bool FakeSecureChannelDisconnector::WasChannelHandled(
    SecureChannel* secure_channel) {
  for (const auto& channel : handled_channels_) {
    if (channel.get() == secure_channel)
      return true;
  }
  return false;
}

void FakeSecureChannelDisconnector::DisconnectSecureChannel(
    std::unique_ptr<SecureChannel> channel_to_disconnect) {
  handled_channels_.insert(std::move(channel_to_disconnect));
}

}  // namespace ash::secure_channel
