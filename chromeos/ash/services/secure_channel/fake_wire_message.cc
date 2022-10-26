// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_wire_message.h"

#include <memory>
#include <string>

#include "chromeos/ash/services/secure_channel/wire_message.h"

namespace ash::secure_channel {

FakeWireMessage::FakeWireMessage(const std::string& payload,
                                 const std::string& feature)
    : WireMessage(payload, feature) {}

std::string FakeWireMessage::Serialize() const {
  return feature() + "," + payload();
}

}  // namespace ash::secure_channel
