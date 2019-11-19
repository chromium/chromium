// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_BASE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_BASE_H_

#include "base/macros.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace secure_channel {

// Base SecureChannel implementation.
class SecureChannelBase : public mojom::SecureChannel {
 public:
  ~SecureChannelBase() override;

  // Binds a receiver to this implementation. Should be called each time that
  // the service receives a receiver.
  void BindReceiver(mojo::PendingReceiver<mojom::SecureChannel> receiver);

 protected:
  SecureChannelBase();

 private:
  mojo::ReceiverSet<mojom::SecureChannel> receivers_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelBase);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_BASE_H_
