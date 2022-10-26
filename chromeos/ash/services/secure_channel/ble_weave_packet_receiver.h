// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_WEAVE_PACKET_RECEIVER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_WEAVE_PACKET_RECEIVER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "chromeos/ash/services/secure_channel/ble_weave_defines.h"

namespace ash::secure_channel::weave {

// Receive the messages sent with uWeave protocol.
// Example Usage:
// State state = ReceivePacket(packet);
// switch (state) {
// case ReceiverState::DATA_READY:
//   OnBytesReceived(GetDataMessage());
//   break;
// case ReceiverState::CONNECTION_CLOSED:
//   Disconnect(GetReasonForClose());
//   break;
// case ReceiverState::ERROR:
//   HandleError(GetReasonToClose());
//   break;
// case ReceiverState::CONNECTING:
// case ReceiverState::WAITING:
// case ReceiverState::RECEIVING_DATA:
//   break;
// default:
//   FoundABugInReceiver();
//   break;
// }
class BluetoothLowEnergyWeavePacketReceiver {
 public:
  enum ReceiverType { CLIENT, SERVER };

  // CONNECTING:
  //   The connection hasn't been estabalished. Accept a CONNECTION_REQUEST if
  //   the receiver is a SERVER. Accept a CONNECTION_RESPONSE if the receiver is
  //   a CLIENT. All other packets cause the receiver to move into ERROR state.
  //   The state will transition to WAITING after a request/response if they
  //   do not have extra data. The state will transition to DATA_READY if the
  //   request/response have extra data since the extra data is treated as a
  //   complete data message. This state is never reentered.
  // WAITING:
  //   The reciever is ready but doesn't have any data. It's waiting for packet
  //   to arrive. Will accept all but connection request/response packets. The
  //   first data packet will move the receiver to the RECEIVING_DATA state. A
  //   connection close packet will move the receiver to the CONNECTION_CLOSED
  //   state. This state is also never reentered.
  // RECEIVING_DATA:
  //   The receiver is in middle of receiving a data message consisted of
  //   multiple packets. Will receive only data packets. The last data packet
  //   will move the receiver into DATA_READY state. This state can be entered
  //   once from WAITING and unlimited number of times from DATA_READY.
  // DATA_READY:
  //   The data message is ready to be retrieved. If the data is not retrieved
  //   before the next packet which will cause a transition, the data will be
  //   lost. Move to RECEIVING_DATA on receiving first data packet. Move to
  //   CONNECTION_CLOSED on receiving close. This state can be entered once from
  //   CONNECTING and unlimited number of times from RECEIVING_DATA.
  // CONNECTION_CLOSED:
  //   The connection is closed. Refuse any further messages. Allow the reason
  //   for close to be retrieved.
  // ERROR:
  //   Something bad happened along the way. Allow a reason to close be
  //   retrieved. The reason to close tells the receiver's user what reason to
  //   close the connection in case the user wants to send a CONNECTION_CLOSE.
  enum State {
    CONNECTING = 0x00,
    WAITING = 0x01,
    RECEIVING_DATA = 0x02,
    DATA_READY = 0x03,
    CONNECTION_CLOSED = 0x04,
    ERROR_DETECTED = 0x05
  };

  // The specific error that caused the receiver to move to ERROR state.
  enum ReceiverError {
    NO_ERROR_DETECTED,
    EMPTY_PACKET,
    RECEIVED_PACKET_IN_CONNECTION_CLOSED,
    RECEIVED_DATA_IN_CONNECTING,
    SERVER_RECEIVED_CONNECTION_RESPONSE,
    CLIENT_RECEIVED_CONNECTION_REQUEST,
    RECEIVED_CONNECTION_CLOSE_IN_CONNECTING,
    UNRECOGNIZED_CONTROL_COMMAND,
    INVALID_CONTROL_COMMAND_IN_DATA_TRANSACTION,
    INVALID_DATA_PACKET_SIZE,
    DATA_HEADER_LOW_BITS_NOT_CLEARED,
    INCORRECT_DATA_FIRST_BIT,
    INVALID_CONNECTION_REQUEST_SIZE,
    INVALID_REQUESTED_MAX_PACKET_SIZE,
    NOT_SUPPORTED_REQUESTED_VERSION,
    INVALID_CONNECTION_RESPONSE_SIZE,
    INVALID_SELECTED_MAX_PACKET_SIZE,
    NOT_SUPPORTED_SELECTED_VERSION,
    INVALID_CONNECTION_CLOSE_SIZE,
    UNRECOGNIZED_REASON_FOR_CLOSE,
    PACKET_OUT_OF_SEQUENCE
  };

  explicit BluetoothLowEnergyWeavePacketReceiver(ReceiverType receiver_type);
  virtual ~BluetoothLowEnergyWeavePacketReceiver();

  typedef std::vector<uint8_t> Packet;

  // Get the receiver’s state.
  virtual State GetState();

  // Return the packet size that the receiver parsed out of request/response.
  virtual uint16_t GetMaxPacketSize();

  // Get the reason that receiver received in a connection close packet.
  // It's only defined in CONNECTION_CLOSED state.
  // Will crash unless receiver is in State::CONNECTION_CLOSED.
  virtual ReasonForClose GetReasonForClose();

  // The reason that the receiver decided to enter the ERROR state.
  // This would be the reason that the receiver's want to send a connection
  // close to the other side of the connection.
  // Will crash unless receiver is in State::ERROR.
  virtual ReasonForClose GetReasonToClose();

  // Get a complete data message that's yet received.
  // Will crash unless receiver is in State::DATA_READY.
  // NOTE: if this function is not called in DATA_READY state and the receiver
  // transitions out of that state, the data will be gone!
  virtual std::string GetDataMessage();

  // Get the specific error that caused the receiver to jump into ERROR state.
  // Can be called from any state. Will return NO_ERROR if no error occurred.
  virtual ReceiverError GetReceiverError();

  // Add a packet that's just been received over Connection to the receiver.
  virtual State ReceivePacket(const Packet& packet);

 private:
  void ReceiveFirstPacket(const Packet& packet);
  void ReceiveNonFirstPacket(const Packet& packet);

  void ReceiveConnectionRequest(const Packet& packet);
  void ReceiveConnectionResponse(const Packet& packet);
  void ReceiveConnectionClose(const Packet& packet);
  void AppendData(const Packet& packet, uint32_t byte_offset);

  uint16_t GetShortField(const Packet& packet, uint32_t byte_offset);
  uint8_t GetPacketType(const Packet& packet);
  uint8_t GetControlCommand(const Packet& packet);
  void VerifyPacketCounter(const Packet& packet);
  bool IsFirstDataPacket(const Packet& packet);
  bool IsLastDataPacket(const Packet& packet);
  bool AreLowerTwoBitsCleared(const Packet& packet);

  void MoveToErrorState(ReasonForClose reason_to_close,
                        ReceiverError receiver_error);

  void SetMaxPacketSize(uint16_t packet_size);
  uint16_t GetConceptualMaxPacketSize();

  // Identify whether the receiver is for a client or a server.
  ReceiverType receiver_type_;

  // Max packet size of the connection.
  // Default is 0 which means the server will determine the size by observing
  // ATT_MTU of the client.
  uint16_t max_packet_size_;

  // Expected counter of the next packet received, starting at 0.
  uint8_t next_packet_counter_;

  // Current state of the receiver.
  // Certain functions will only return valid value if the receiver is in the
  // appropriate state.
  State state_;

  // The reason why the connection was closed by the sender if any.
  ReasonForClose reason_for_close_;

  // The reason why the receiver is in an erronous state if any.
  ReasonForClose reason_to_close_;

  // The data message if there is one.
  Packet data_message_;

  // The error the receiver encountered while processing packets.
  // Used for debugging purproses.
  ReceiverError receiver_error_;
};

}  // namespace ash::secure_channel::weave

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_WEAVE_PACKET_RECEIVER_H_
