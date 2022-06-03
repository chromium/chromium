// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_WIRE_MESSAGE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_WIRE_MESSAGE_H_

#include <memory>
#include <string>

#include "chromeos/services/secure_channel/wire_message.h"

namespace chromeos {

namespace secure_channel {

class FakeWireMessage : public WireMessage {
 public:
  FakeWireMessage(const std::string& payload, const std::string& feature);

  FakeWireMessage(const FakeWireMessage&) = delete;
  FakeWireMessage& operator=(const FakeWireMessage&) = delete;

  // WireMessage:
  std::string Serialize() const override;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_WIRE_MESSAGE_H_
