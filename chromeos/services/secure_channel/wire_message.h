// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_WIRE_MESSAGE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_WIRE_MESSAGE_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace chromeos {

namespace secure_channel {

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

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_WIRE_MESSAGE_H_
