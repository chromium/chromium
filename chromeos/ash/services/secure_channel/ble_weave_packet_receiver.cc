// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/services/secure_channel/ble_weave_packet_receiver.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::secure_channel::weave {

namespace {

const uint16_t kMaxInitControlPacketSize = 20;
const uint16_t kMaxPacketSizeLowerBound = 20;

}  // namespace

BluetoothLowEnergyWeavePacketReceiver::BluetoothLowEnergyWeavePacketReceiver(
    ReceiverType receiver_type)
    : receiver_type_(receiver_type),
      next_packet_counter_(0),
      state_(State::CONNECTING),
      reason_for_close_(ReasonForClose::CLOSE_WITHOUT_ERROR),
      reason_to_close_(ReasonForClose::CLOSE_WITHOUT_ERROR),
      receiver_error_(ReceiverError::NO_ERROR_DETECTED) {
  SetMaxPacketSize(kMaxPacketSizeLowerBound);
}

BluetoothLowEnergyWeavePacketReceiver::
    ~BluetoothLowEnergyWeavePacketReceiver() {}

BluetoothLowEnergyWeavePacketReceiver::State
BluetoothLowEnergyWeavePacketReceiver::GetState() {
  return state_;
}

uint16_t BluetoothLowEnergyWeavePacketReceiver::GetMaxPacketSize() {
  // max_packet_size_ is well defined in every state.
  return max_packet_size_;
}

ReasonForClose BluetoothLowEnergyWeavePacketReceiver::GetReasonForClose() {
  DCHECK(state_ == State::CONNECTION_CLOSED);
  return reason_for_close_;
}

ReasonForClose BluetoothLowEnergyWeavePacketReceiver::GetReasonToClose() {
  return reason_to_close_;
}

std::string BluetoothLowEnergyWeavePacketReceiver::GetDataMessage() {
  DCHECK(state_ == State::DATA_READY);
  return std::string(data_message_.begin(), data_message_.end());
}

BluetoothLowEnergyWeavePacketReceiver::ReceiverError
BluetoothLowEnergyWeavePacketReceiver::GetReceiverError() {
  return receiver_error_;
}

BluetoothLowEnergyWeavePacketReceiver::State
BluetoothLowEnergyWeavePacketReceiver::ReceivePacket(const Packet& packet) {
  if (state_ == State::ERROR_DETECTED) {
    PA_LOG(ERROR) << "Received message in ERROR state.";
  } else if (packet.empty()) {
    PA_LOG(ERROR) << "Received empty packet. Empty packet is not a valid uWeave"
                  << " packet.";
    MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                     ReceiverError::EMPTY_PACKET);
  } else {
    VerifyPacketCounter(packet);

    switch (state_) {
      case State::CONNECTING:
        ReceiveFirstPacket(packet);
        break;
      case State::WAITING:
      case State::RECEIVING_DATA:
        ReceiveNonFirstPacket(packet);
        break;
      case State::DATA_READY:
        data_message_.clear();
        ReceiveNonFirstPacket(packet);
        break;
      case State::CONNECTION_CLOSED:
        PA_LOG(ERROR) << "Received message in ConnectionClosed state.";
        MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                         ReceiverError::RECEIVED_PACKET_IN_CONNECTION_CLOSED);
        break;
      case State::ERROR_DETECTED:
        // Counter not verified.
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
  return state_;
}

void BluetoothLowEnergyWeavePacketReceiver::ReceiveFirstPacket(
    const Packet& packet) {
  DCHECK(!packet.empty());
  DCHECK(state_ == State::CONNECTING);

  if (GetPacketType(packet) != PacketType::CONTROL) {
    PA_LOG(ERROR) << "Received data packets when not connected.";
    MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                     ReceiverError::RECEIVED_DATA_IN_CONNECTING);
    return;
  }

  uint8_t command = GetControlCommand(packet);
  switch (command) {
    case ControlCommand::CONNECTION_REQUEST:
      if (receiver_type_ == ReceiverType::SERVER) {
        ReceiveConnectionRequest(packet);
      } else {
        PA_LOG(ERROR) << "Server received connection response instead of "
                      << "request.";
        MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                         ReceiverError::SERVER_RECEIVED_CONNECTION_RESPONSE);
      }
      break;
    case ControlCommand::CONNECTION_RESPONSE:
      if (receiver_type_ == ReceiverType::CLIENT) {
        ReceiveConnectionResponse(packet);
      } else {
        PA_LOG(ERROR) << "Client received connection request instead of "
                      << "response.";
        MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                         ReceiverError::CLIENT_RECEIVED_CONNECTION_REQUEST);
      }
      break;
    case ControlCommand::CONNECTION_CLOSE:
      PA_LOG(ERROR) << "Received connection close when not even connected.";
      MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                       ReceiverError::RECEIVED_CONNECTION_CLOSE_IN_CONNECTING);
      break;
    default:
      PA_LOG(ERROR) << "Received unrecognized control packet command: "
                    << base::NumberToString(command);
      MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                       ReceiverError::UNRECOGNIZED_CONTROL_COMMAND);
      break;
  }
}

void BluetoothLowEnergyWeavePacketReceiver::ReceiveNonFirstPacket(
    const Packet& packet) {
  DCHECK(!packet.empty());

  uint8_t command;
  bool expect_first_packet = state_ != State::RECEIVING_DATA;

  switch (GetPacketType(packet)) {
    case PacketType::CONTROL:
      command = GetControlCommand(packet);
      if (command == ControlCommand::CONNECTION_CLOSE) {
        ReceiveConnectionClose(packet);
      } else {
        PA_LOG(ERROR) << "Received invalid command "
                      << base::NumberToString(command)
                      << " during data transaction";
        MoveToErrorState(
            ReasonForClose::UNKNOWN_ERROR,
            ReceiverError::INVALID_CONTROL_COMMAND_IN_DATA_TRANSACTION);
      }
      break;
    case PacketType::DATA:
      if (packet.size() > GetConceptualMaxPacketSize()) {
        PA_LOG(ERROR) << "Received packet with size: " << packet.size()
                      << ". It is greater than maximum packet size "
                      << GetConceptualMaxPacketSize();
        MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                         ReceiverError::INVALID_DATA_PACKET_SIZE);
      } else if (!AreLowerTwoBitsCleared(packet)) {
        PA_LOG(ERROR) << "Lower two bits of data packet header are not clear "
                      << "as expected.";
        MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                         ReceiverError::DATA_HEADER_LOW_BITS_NOT_CLEARED);
      } else if (expect_first_packet != IsFirstDataPacket(packet)) {
        PA_LOG(ERROR) << "First bit of data packet is set incorrectly to: "
                      << IsFirstDataPacket(packet);
        MoveToErrorState(ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE,
                         ReceiverError::INCORRECT_DATA_FIRST_BIT);
      } else {
        AppendData(packet, 1);
        if (IsLastDataPacket(packet)) {
          state_ = State::DATA_READY;
        } else {
          state_ = State::RECEIVING_DATA;
        }
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void BluetoothLowEnergyWeavePacketReceiver::ReceiveConnectionRequest(
    const Packet& packet) {
  DCHECK(!packet.empty());
  DCHECK(state_ == State::CONNECTING);

  if (packet.size() < kMinConnectionRequestSize ||
      packet.size() > kMaxInitControlPacketSize) {
    PA_LOG(ERROR) << "Received invalid connection request packet size: "
                  << packet.size();
    MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                     ReceiverError::INVALID_CONNECTION_REQUEST_SIZE);
    return;
  }

  uint16_t packet_size = GetShortField(packet, 5);
  // Packet size of 0 means the server can observe the ATT_MTU and select an
  // appropriate packet size;
  if (packet_size != kSelectMaxPacketSize &&
      packet_size < kMaxPacketSizeLowerBound) {
    PA_LOG(ERROR) << "Received requested max packet size of: " << packet_size
                  << ". Client must support at least "
                  << kMaxPacketSizeLowerBound << " bytes per packet.";
    MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                     ReceiverError::INVALID_REQUESTED_MAX_PACKET_SIZE);
    return;
  }
  SetMaxPacketSize(packet_size);

  uint16_t min_version = GetShortField(packet, 1);
  uint16_t max_version = GetShortField(packet, 3);
  if (kWeaveVersion < min_version || kWeaveVersion > max_version) {
    PA_LOG(ERROR) << "Server does not support client version range.";
    MoveToErrorState(ReasonForClose::NO_COMMON_VERSION_SUPPORTED,
                     ReceiverError::NOT_SUPPORTED_REQUESTED_VERSION);
    return;
  }

  if (packet.size() > kMinConnectionRequestSize) {
    AppendData(packet, kMinConnectionRequestSize);
    state_ = State::DATA_READY;
  } else {
    state_ = State::WAITING;
  }
}

void BluetoothLowEnergyWeavePacketReceiver::ReceiveConnectionResponse(
    const Packet& packet) {
  DCHECK(!packet.empty());
  DCHECK(state_ == State::CONNECTING);

  if (packet.size() < kMinConnectionResponseSize ||
      packet.size() > kMaxInitControlPacketSize) {
    PA_LOG(ERROR) << "Received invalid connection response packet size: "
                  << packet.size();
    MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                     ReceiverError::INVALID_CONNECTION_RESPONSE_SIZE);
    return;
  }

  uint16_t selected_packet_size = GetShortField(packet, 3);
  if (selected_packet_size < kMaxPacketSizeLowerBound) {
    PA_LOG(ERROR) << "Received selected max packet size of: "
                  << selected_packet_size << ". Server must support at least "
                  << kMaxPacketSizeLowerBound << " bytes per packet.";
    MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                     ReceiverError::INVALID_SELECTED_MAX_PACKET_SIZE);
    return;
  }
  SetMaxPacketSize(selected_packet_size);

  uint16_t selected_version = GetShortField(packet, 1);
  if (selected_version != kWeaveVersion) {
    PA_LOG(ERROR) << "Client does not support server selected version.";
    MoveToErrorState(ReasonForClose::NO_COMMON_VERSION_SUPPORTED,
                     ReceiverError::NOT_SUPPORTED_SELECTED_VERSION);
    return;
  }

  if (packet.size() > kMinConnectionResponseSize) {
    AppendData(packet, kMinConnectionResponseSize);
    state_ = State::DATA_READY;
  } else {
    state_ = State::WAITING;
  }
}

void BluetoothLowEnergyWeavePacketReceiver::ReceiveConnectionClose(
    const Packet& packet) {
  DCHECK(!packet.empty());

  uint16_t reason;

  if (packet.size() > kMaxConnectionCloseSize) {
    PA_LOG(ERROR) << "Received invalid connection close packet size: "
                  << packet.size();
    MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                     ReceiverError::INVALID_CONNECTION_CLOSE_SIZE);
    return;
  } else if (packet.size() < kMaxConnectionCloseSize) {
    reason = ReasonForClose::UNKNOWN_ERROR;
  } else {
    reason = GetShortField(packet, 1);
  }

  switch (reason) {
    case ReasonForClose::CLOSE_WITHOUT_ERROR:
    case ReasonForClose::UNKNOWN_ERROR:
    case ReasonForClose::NO_COMMON_VERSION_SUPPORTED:
    case ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE:
    case ReasonForClose::APPLICATION_ERROR:
      reason_for_close_ = static_cast<ReasonForClose>(reason);
      state_ = State::CONNECTION_CLOSED;
      break;
    default:
      PA_LOG(ERROR) << "Received invalid reason for close: " << reason;
      MoveToErrorState(ReasonForClose::UNKNOWN_ERROR,
                       ReceiverError::UNRECOGNIZED_REASON_FOR_CLOSE);
      break;
  }
}

void BluetoothLowEnergyWeavePacketReceiver::AppendData(const Packet& packet,
                                                       uint32_t byte_offset) {
  DCHECK(!packet.empty());

  // Append to data_message_ bytes 1 through end of the packet.
  data_message_.insert(data_message_.end(), packet.begin() + byte_offset,
                       packet.end());
}

uint16_t BluetoothLowEnergyWeavePacketReceiver::GetShortField(
    const Packet& packet,
    uint32_t byte_offset) {
  DCHECK_LT(byte_offset, packet.size());
  DCHECK_LT(byte_offset + 1, packet.size());

  uint16_t received;
  uint8_t* received_ptr = reinterpret_cast<uint8_t*>(&received);
  received_ptr[0] = packet[byte_offset];
  received_ptr[1] = packet[byte_offset + 1];

  return ntohs(received);
}

uint8_t BluetoothLowEnergyWeavePacketReceiver::GetPacketType(
    const Packet& packet) {
  DCHECK(!packet.empty());
  // Packet type is stored in the highest bit of the first byte.
  return (packet[0] >> 7) & 1;
}

uint8_t BluetoothLowEnergyWeavePacketReceiver::GetControlCommand(
    const Packet& packet) {
  DCHECK(!packet.empty());
  // Control command is stored in the lower 4 bits of the first byte.
  return packet[0] & 0x0F;
}

void BluetoothLowEnergyWeavePacketReceiver::VerifyPacketCounter(
    const Packet& packet) {
  DCHECK(!packet.empty());
  DCHECK(state_ != State::ERROR_DETECTED);

  // Packet counter is bits 4, 5, and 6 of the first byte.
  uint8_t count = (packet[0] >> 4) & 7;

  if (count == (next_packet_counter_ % kMaxPacketCounter)) {
    next_packet_counter_++;
  } else {
    PA_LOG(ERROR) << "Received invalid packet counter: "
                  << base::NumberToString(count);
    MoveToErrorState(ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE,
                     ReceiverError::PACKET_OUT_OF_SEQUENCE);
  }
}

bool BluetoothLowEnergyWeavePacketReceiver::IsFirstDataPacket(
    const Packet& packet) {
  DCHECK(!packet.empty());
  // Bit 3 determines whether the packet is the first packet of the message.
  return (packet[0] >> 3) & 1;
}

bool BluetoothLowEnergyWeavePacketReceiver::IsLastDataPacket(
    const Packet& packet) {
  DCHECK(!packet.empty());
  // Bit 2 determines whether the packet is the last packet of the message.
  return (packet[0] >> 2) & 1;
}

bool BluetoothLowEnergyWeavePacketReceiver::AreLowerTwoBitsCleared(
    const Packet& packet) {
  DCHECK(!packet.empty());
  return (packet[0] & 3) == 0;
}

void BluetoothLowEnergyWeavePacketReceiver::MoveToErrorState(
    ReasonForClose reason_to_close,
    ReceiverError receiver_error) {
  state_ = State::ERROR_DETECTED;
  reason_to_close_ = reason_to_close;
  receiver_error_ = receiver_error;
}

void BluetoothLowEnergyWeavePacketReceiver::SetMaxPacketSize(
    uint16_t packet_size) {
  DCHECK(packet_size == kSelectMaxPacketSize ||
         packet_size >= kMaxPacketSizeLowerBound);
  max_packet_size_ = packet_size;
}

uint16_t BluetoothLowEnergyWeavePacketReceiver::GetConceptualMaxPacketSize() {
  if (!max_packet_size_)
    return kMaxPacketSizeLowerBound;
  return max_packet_size_;
}

}  // namespace ash::secure_channel::weave
