// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_DISCONNECTOR_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_DISCONNECTOR_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "chromeos/ash/services/secure_channel/secure_channel.h"
#include "chromeos/ash/services/secure_channel/secure_channel_disconnector.h"

namespace ash::secure_channel {

// Test SecureChannelDisconnector implementation.
class FakeSecureChannelDisconnector : public SecureChannelDisconnector {
 public:
  FakeSecureChannelDisconnector();

  FakeSecureChannelDisconnector(const FakeSecureChannelDisconnector&) = delete;
  FakeSecureChannelDisconnector& operator=(
      const FakeSecureChannelDisconnector&) = delete;

  ~FakeSecureChannelDisconnector() override;

  const base::flat_set<std::unique_ptr<SecureChannel>>& handled_channels() {
    return handled_channels_;
  }

  bool WasChannelHandled(SecureChannel* secure_channel);

 private:
  // SecureChannelDisconnector:
  void DisconnectSecureChannel(
      std::unique_ptr<SecureChannel> channel_to_disconnect) override;

  base::flat_set<std::unique_ptr<SecureChannel>> handled_channels_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_DISCONNECTOR_H_
