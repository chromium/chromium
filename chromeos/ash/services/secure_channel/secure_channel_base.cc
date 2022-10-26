// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/secure_channel_base.h"

namespace ash::secure_channel {

SecureChannelBase::SecureChannelBase() = default;

SecureChannelBase::~SecureChannelBase() = default;

void SecureChannelBase::BindReceiver(
    mojo::PendingReceiver<mojom::SecureChannel> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace ash::secure_channel
