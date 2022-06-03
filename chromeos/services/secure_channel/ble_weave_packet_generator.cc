// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_weave_packet_generator.h"

#include <string.h>
#include <algorithm>
#ifdef OS_WIN
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "base/check_op.h"

namespace chromeos {

namespace secure_channel {

namespace weave {

BluetoothLowEnergyWeavePacketGenerator::BluetoothLowEnergyWeavePacketGenerator()
    : max_packet_size_(kDefaultMaxPacketSize), next_packet_counter_(0) {}

BluetoothLowEnergyWeavePacketGenerator::
    ~BluetoothLowEnergyWeavePacketGenerator() {}

Packet BluetoothLowEnergyWeavePacketGenerator::CreateConnectionRequest() {
  Packet packet(kMinConnectionRequestSize, 0);

  SetPacketTypeBit(PacketType::CONTROL, &packet);
  // Since it only make sense for connection request to be the 0th packet,
  // resets the packet counter.
  next_packet_counter_ = 1;
  SetControlCommand(ControlCommand::CONNECTION_REQUEST, &packet);
  SetShortField(1, kWeaveVersion, &packet);
  SetShortField(3, kWeaveVersion, &packet);
  SetShortField(5, kSelectMaxPacketSize, &packet);

  return packet;
}

Packet BluetoothLowEnergyWeavePacketGenerator::CreateConnectionResponse() {
  Packet packet(kMinConnectionResponseSize, 0);

  SetPacketTypeBit(PacketType::CONTROL, &packet);
  // Since it only make sense for connection response to be the 0th packet,
  // resets the next packet counter.
  next_packet_counter_ = 1;
  SetControlCommand(ControlCommand::CONNECTION_RESPONSE, &packet);
  SetShortField(1, kWeaveVersion, &packet);
  SetShortField(3, max_packet_size_, &packet);

  return packet;
}

Packet BluetoothLowEnergyWeavePacketGenerator::CreateConnectionClose(
    ReasonForClose reason_for_close) {
  Packet packet(kMaxConnectionCloseSize, 0);

  SetPacketTypeBit(PacketType::CONTROL, &packet);
  SetPacketCounter(&packet);
  SetControlCommand(ControlCommand::CONNECTION_CLOSE, &packet);
  SetShortField(1, reason_for_close, &packet);

  return packet;
}

void BluetoothLowEnergyWeavePacketGenerator::SetMaxPacketSize(uint16_t size) {
  DCHECK(size >= kDefaultMaxPacketSize);
  max_packet_size_ = size;
}

std::vector<Packet> BluetoothLowEnergyWeavePacketGenerator::EncodeDataMessage(
    std::string message) {
  DCHECK(!message.empty());

  // The first byte of a packet is used by the uWeave protocol,
  // hence the payload is 1 byte smaller than the packet size.
  uint32_t packet_payload_size = max_packet_size_ - 1;

  uint32_t message_length = message.length();
  // (packet_payload_size - 1) is used to enforce rounding up.
  uint32_t num_packets =
      (message_length + (packet_payload_size - 1)) / packet_payload_size;

  std::vector<Packet> weave_message(num_packets);

  const char* byte_message = message.c_str();

  for (uint32_t i = 0; i < num_packets; ++i) {
    Packet* packet = &weave_message[i];
    uint32_t begin = packet_payload_size * i;
    uint32_t end = std::min(begin + packet_payload_size, message_length);

    packet->push_back(0);

    SetPacketTypeBit(PacketType::DATA, packet);
    SetPacketCounter(packet);

    for (uint32_t j = begin; j < end; ++j) {
      packet->push_back(byte_message[j]);
    }
  }

  // Guaranteed to have at least one packet since message is not empty.
  SetDataFirstBit(&weave_message[0]);
  SetDataLastBit(&weave_message[num_packets - 1]);

  return weave_message;
}

void BluetoothLowEnergyWeavePacketGenerator::SetShortField(uint32_t byte_offset,
                                                           uint16_t val,
                                                           Packet* packet) {
  DCHECK(packet);
  DCHECK_LT(byte_offset, packet->size());
  DCHECK_LT(byte_offset + 1, packet->size());

  uint16_t network_val = htons(val);
  uint8_t* network_val_ptr = reinterpret_cast<uint8_t*>(&network_val);

  packet->at(byte_offset) = network_val_ptr[0];
  packet->at(byte_offset + 1) = network_val_ptr[1];
}

void BluetoothLowEnergyWeavePacketGenerator::SetPacketTypeBit(PacketType type,
                                                              Packet* packet) {
  DCHECK(packet);
  DCHECK(!packet->empty());

  // Type bit is the highest bit of the first byte of the packet.
  // So clear the highest bit and set it according to val.
  packet->at(0) = (packet->at(0) & 0x7F) | (type << 7);
}

void BluetoothLowEnergyWeavePacketGenerator::SetControlCommand(
    ControlCommand command,
    Packet* packet) {
  DCHECK(packet);
  DCHECK(!packet->empty());

  // Control Command is the lower 4 bits of the packet's first byte.
  // So clear the lower 4 bites and set it according to val.
  packet->at(0) = (packet->at(0) & 0xF0) | command;
}

void BluetoothLowEnergyWeavePacketGenerator::SetPacketCounter(Packet* packet) {
  DCHECK(packet);
  DCHECK(!packet->empty());
  uint8_t counter = next_packet_counter_ % kMaxPacketCounter;

  // Packet counter is the bits 4, 5, and 6 of the packet's first byte.
  // So clear those bits and set them according to current packet counter
  // modular max packet counter.
  packet->at(0) = (packet->at(0) & 0x8F) | (counter << 4);
  next_packet_counter_++;
}

void BluetoothLowEnergyWeavePacketGenerator::SetDataFirstBit(Packet* packet) {
  DCHECK(packet);
  DCHECK(!packet->empty());

  // First bit is bit 3 of the packet's first byte and set it to 1.
  packet->at(0) = packet->at(0) | (1 << 3);
}

void BluetoothLowEnergyWeavePacketGenerator::SetDataLastBit(Packet* packet) {
  DCHECK(packet);
  DCHECK(!packet->empty());

  // Last bit is the bit 2 of the packet's first byte and set it to 1.
  packet->at(0) = packet->at(0) | (1 << 2);
}

}  // namespace weave

}  // namespace secure_channel

}  // namespace chromeos
