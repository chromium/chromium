// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_WEAVE_DEFINES_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_WEAVE_DEFINES_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace ash::secure_channel::weave {

enum PacketType { DATA = 0x00, CONTROL = 0x01 };

// Identify the action intended by the control packet.
enum ControlCommand {
  CONNECTION_REQUEST = 0x00,
  CONNECTION_RESPONSE = 0x01,
  CONNECTION_CLOSE = 0x02
};

// Sent with the ConnectionClose control packet.
// Identify why the client/server wished to close the connection.
enum ReasonForClose {
  CLOSE_WITHOUT_ERROR = 0x00,
  UNKNOWN_ERROR = 0x01,
  NO_COMMON_VERSION_SUPPORTED = 0x02,
  RECEIVED_PACKET_OUT_OF_SEQUENCE = 0x03,
  APPLICATION_ERROR = 0x80
};

typedef std::vector<uint8_t> Packet;

const uint16_t kMinConnectionRequestSize = 7;
const uint16_t kMinConnectionResponseSize = 5;
const uint16_t kMaxConnectionCloseSize = 3;
const uint16_t kDefaultMaxPacketSize = 20;
const uint16_t kWeaveVersion = 1;
// Defer selecting the max packet size to the server.
const uint16_t kSelectMaxPacketSize = 0;
const uint8_t kMaxPacketCounter = 8;

// Used only for tests.
const uint8_t kByteDefaultMaxPacketSize = 20;
const uint8_t kByteWeaveVersion = 1;
const uint8_t kByteSelectMaxPacketSize = 0;
const uint8_t kEmptyUpperByte = 0;

}  // namespace ash::secure_channel::weave

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_WEAVE_DEFINES_H_
