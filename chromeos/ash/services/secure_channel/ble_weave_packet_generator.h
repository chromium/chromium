// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_WEAVE_PACKET_GENERATOR_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_WEAVE_PACKET_GENERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "chromeos/ash/services/secure_channel/ble_weave_defines.h"

namespace ash::secure_channel::weave {

// Generates the messages sent using the uWeave protocol.
class BluetoothLowEnergyWeavePacketGenerator {
 public:
  BluetoothLowEnergyWeavePacketGenerator();
  virtual ~BluetoothLowEnergyWeavePacketGenerator();

  virtual Packet CreateConnectionRequest();
  virtual Packet CreateConnectionResponse();
  virtual Packet CreateConnectionClose(ReasonForClose reason_for_close);

  // Packet size must be greater than or equal to 20.
  virtual void SetMaxPacketSize(uint16_t size);

  // Will crash if message is empty.
  virtual std::vector<Packet> EncodeDataMessage(std::string message);

 private:
  void SetShortField(uint32_t byte_offset, uint16_t val, Packet* packet);
  void SetPacketTypeBit(PacketType val, Packet* packet);
  void SetControlCommand(ControlCommand val, Packet* packet);
  void SetPacketCounter(Packet* packet);
  void SetDataFirstBit(Packet* packet);
  void SetDataLastBit(Packet* packet);

  // The default max packet length is 20 unless SetDataPacketLength() is called
  // and specified otherwise.
  uint16_t max_packet_size_;

  // Counter for the number of packets sent, starting at 0.
  uint8_t next_packet_counter_;
};

}  // namespace ash::secure_channel::weave

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_WEAVE_PACKET_GENERATOR_H_
