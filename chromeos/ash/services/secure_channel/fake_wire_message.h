// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_WIRE_MESSAGE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_WIRE_MESSAGE_H_

#include <memory>
#include <string>

#include "chromeos/ash/services/secure_channel/wire_message.h"

namespace ash::secure_channel {

class FakeWireMessage : public WireMessage {
 public:
  FakeWireMessage(const std::string& payload, const std::string& feature);

  FakeWireMessage(const FakeWireMessage&) = delete;
  FakeWireMessage& operator=(const FakeWireMessage&) = delete;

  // WireMessage:
  std::string Serialize() const override;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_WIRE_MESSAGE_H_
