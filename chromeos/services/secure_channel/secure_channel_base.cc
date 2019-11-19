// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/secure_channel_base.h"

namespace chromeos {

namespace secure_channel {

SecureChannelBase::SecureChannelBase() = default;

SecureChannelBase::~SecureChannelBase() = default;

void SecureChannelBase::BindReceiver(
    mojo::PendingReceiver<mojom::SecureChannel> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace secure_channel

}  // namespace chromeos
