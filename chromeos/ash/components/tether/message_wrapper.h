// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_MESSAGE_WRAPPER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_MESSAGE_WRAPPER_H_

#include <google/protobuf/message_lite.h>
#include <memory>

#include "chromeos/ash/components/tether/proto/tether.pb.h"

namespace ash {

namespace tether {

// Wraps a message sent between devices. To send a message over the wire,
// construct a MessageWrapper by passing the message's proto as a constructor
// parameter, then call |ToRawMessage()| and send the resulting string. When a
// message has been received, pass it to |FromRawMessage()| to create a
// MessageWrapper object, then use |GetMessageType()| to determine the type of
// message that has been received. To access the wrapped proto for a received
// message, call |GetProto()| and cast the result to the associated message
// class.
class MessageWrapper {
 public:
  // Creates a MessageWrapper from a raw string received from a remote device.
  // Returns |nullptr| if the received message is not a valid tethering message.
  static std::unique_ptr<MessageWrapper> FromRawMessage(
      const std::string& message);

  MessageWrapper(const ConnectTetheringRequest& request);
  MessageWrapper(const ConnectTetheringResponse& response);
  MessageWrapper(const DisconnectTetheringRequest& request);
  MessageWrapper(const KeepAliveTickle& tickle);
  MessageWrapper(const KeepAliveTickleResponse& response);
  MessageWrapper(const TetherAvailabilityRequest& request);
  MessageWrapper(const TetherAvailabilityResponse& response);

  MessageWrapper(const MessageWrapper&) = delete;
  MessageWrapper& operator=(const MessageWrapper&) = delete;

  ~MessageWrapper();

  google::protobuf::MessageLite* GetProto() const;
  MessageType GetMessageType() const;

  // To send a message to a remote device, use ToRawMessage() and send the
  // result over the wire.
  std::string ToRawMessage() const;

 protected:
  MessageWrapper(const MessageType& type,
                 std::unique_ptr<google::protobuf::MessageLite> proto);

 private:
  MessageType type_;
  std::unique_ptr<google::protobuf::MessageLite> proto_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_MESSAGE_WRAPPER_H_
