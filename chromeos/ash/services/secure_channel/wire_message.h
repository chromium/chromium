// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_WIRE_MESSAGE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_WIRE_MESSAGE_H_

#include <memory>
#include <string>

namespace ash::secure_channel {

// Message sent over the wire via SecureChannel. Each message sent using the
// SecureChannel protocol has a feature name (a unique identifier for the client
// sending/receiving a message) and a payload.
//
// A wire message in SecureChannel is serialized to a byte array (in the format
// of a std::string). The first byte is always the protocol version expressed as
// an unsigned 8-bit int. This class supports protocol versions 3 and 4.
//
// v3: One byte version (0x03), followed by 2 bytes of the message size as an
//     unsigned 16-bit int, followed by a stringified JSON object containing a
//     "feature" key whose value is the feature and a "payload" key whose value
//     is the payload.
//
//     [ message version ] [ body length ] [ JSON body ]
//           1 byte            2 bytes      body length
//
// v4: One byte version (0x04), followed by 4 bytes of the message size as an
//     unsigned 32-bit int, followed by a stringified JSON object containing a
//     "feature" key whose value is the feature and a "payload" key whose value
//     is the payload.
//
//     [ message version ] [ body length ] [ JSON body ]
//           1 byte            4 bytes      body length
//
// v3 is deprecated and all new features use v4, but we special-case features
// which were released before v4 and send them via v3 for backward
// compatibility.
class WireMessage {
 public:
  // Creates a WireMessage containing |payload| for feature |feature| and
  // sequence number |sequence_number|.
  WireMessage(const std::string& payload,
              const std::string& feature,
              int sequence_number);

  // Creates a WireMessage containing |payload| for feature |feature|.
  WireMessage(const std::string& payload, const std::string& feature);

  WireMessage(const WireMessage& other);

  WireMessage& operator=(const WireMessage& other);

  // Creates a WireMessage containing |body| (a serialized JSON) as the message
  // body.
  explicit WireMessage(const std::string& body);

  virtual ~WireMessage();

  // Returns the deserialized message from |serialized_message|, or nullptr if
  // the message is malformed. Sets |is_incomplete_message| to true if the
  // message does not have enough data to parse the header, or if the message
  // length encoded in the message header exceeds the size of the
  // |serialized_message|.
  static std::unique_ptr<WireMessage> Deserialize(
      const std::string& serialized_message,
      bool* is_incomplete_message);

  // Returns a serialized representation of |this| message.
  virtual std::string Serialize() const;

  const std::string& payload() const { return payload_; }
  const std::string& feature() const { return feature_; }
  const std::string& body() const { return body_; }
  // Will return -1 if no sequence number was passed to the constructor.
  int sequence_number() const { return sequence_number_; }

 private:
  // The message payload.
  std::string payload_;

  // The feature which sends or intends to receive this message (e.g.,
  // EasyUnlock).
  std::string feature_;

  // The message body. When this is set |payload_| and |feature_| are empty, and
  // vice-versa.
  std::string body_;

  // Sequence number for this message; set to -1 if no sequence number if set.
  int sequence_number_ = -1;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_WIRE_MESSAGE_H_
