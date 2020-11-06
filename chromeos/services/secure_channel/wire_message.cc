// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/wire_message.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/components/multidevice/logging/logging.h"

// The wire messages have a simple format:
// [ message version ] [ body length ] [ JSON body ]
//       1 byte            2 bytes      body length
// When sending encrypted messages, the JSON body contains two fields: an
// optional |permit_id| field and a required |payload| field.
//
// For non-encrypted messages, the message itself is the JSON body, and it
// doesn't have a |payload| field.

namespace chromeos {

namespace secure_channel {

namespace {

// The length of the message header, in bytes.
const size_t kHeaderLength = 3;

// The protocol version of the message format.
const int kMessageFormatVersionThree = 3;

const char kPayloadKey[] = "payload";
const char kFeatureKey[] = "feature";

// The default feature value. This is the default for backward compatibility
// reasons; previously, the protocol did not transmit the feature in the
// message, but because EasyUnlock was the only feature used, it didn't matter.
// So, if a message is received without a feature, it is assumed to be
// EasyUnlock by default.
const char kDefaultFeature[] = "easy_unlock";

// Parses the |serialized_message|'s header. Returns |true| iff the message has
// a valid header, is complete, and is well-formed according to the header. Sets
// |is_incomplete_message| to true iff the message does not have enough data to
// parse the header, or if the message length encoded in the message header
// exceeds the size of the |serialized_message|.
bool ParseHeader(const std::string& serialized_message,
                 bool* is_incomplete_message) {
  *is_incomplete_message = false;
  if (serialized_message.size() < kHeaderLength) {
    *is_incomplete_message = true;
    return false;
  }

  static_assert(kHeaderLength > 2, "kHeaderLength too small");
  size_t version = serialized_message[0];
  if (version != kMessageFormatVersionThree) {
    PA_LOG(WARNING) << "Error: Invalid message version. Got " << version
                    << ", expected " << kMessageFormatVersionThree;
    return false;
  }

  uint16_t expected_body_length =
      (static_cast<uint8_t>(serialized_message[1]) << 8) |
      (static_cast<uint8_t>(serialized_message[2]) << 0);
  size_t expected_message_length = kHeaderLength + expected_body_length;
  if (serialized_message.size() < expected_message_length) {
    *is_incomplete_message = true;
    return false;
  }
  if (serialized_message.size() != expected_message_length) {
    PA_LOG(WARNING) << "Error: Invalid message length. Got "
                    << serialized_message.size() << ", expected "
                    << expected_message_length;
    return false;
  }

  return true;
}

}  // namespace

WireMessage::~WireMessage() {}

// static
std::unique_ptr<WireMessage> WireMessage::Deserialize(
    const std::string& serialized_message,
    bool* is_incomplete_message) {
  if (!ParseHeader(serialized_message, is_incomplete_message))
    return nullptr;

  std::unique_ptr<base::Value> body_value = base::JSONReader::ReadDeprecated(
      serialized_message.substr(kHeaderLength));
  if (!body_value || !body_value->is_dict()) {
    PA_LOG(WARNING) << "Error: Unable to parse message as JSON.";
    return nullptr;
  }

  base::DictionaryValue* body;
  bool success = body_value->GetAsDictionary(&body);
  DCHECK(success);

  std::string payload_base64;
  if (!body->GetString(kPayloadKey, &payload_base64)) {
    // The body is a valid JSON, but it doesn't contain a |payload| field. It
    // must be a non-encrypted message.
    return base::WrapUnique(
        new WireMessage(serialized_message.substr(kHeaderLength)));
  }

  if (payload_base64.empty()) {
    PA_LOG(WARNING) << "Error: Missing payload.";
    return nullptr;
  }

  std::string payload;
  if (!base::Base64UrlDecode(payload_base64,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &payload)) {
    PA_LOG(WARNING) << "Error: Invalid base64 encoding for payload.";
    return nullptr;
  }

  std::string feature;
  if (!body->GetString(kFeatureKey, &feature) || feature.empty()) {
    feature = std::string(kDefaultFeature);
  }

  return base::WrapUnique(new WireMessage(payload, feature));
}

std::string WireMessage::Serialize() const {
  std::string json_body;
  if (body_.empty()) {
    if (payload_.empty()) {
      PA_LOG(ERROR) << "Failed to serialize empty wire message.";
      return std::string();
    }

    // Create JSON body containing permit id and payload.
    base::DictionaryValue body;

    std::string base64_payload;
    base::Base64UrlEncode(payload_,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &base64_payload);
    body.SetString(kPayloadKey, base64_payload);
    body.SetString(kFeatureKey, feature_);

    if (!base::JSONWriter::Write(body, &json_body)) {
      PA_LOG(ERROR) << "Failed to convert WireMessage body to JSON: " << body;
      return std::string();
    }
  } else {
    json_body = body_;
  }

  // Create header containing version and payload size.
  size_t body_size = json_body.size();
  if (body_size > std::numeric_limits<uint16_t>::max()) {
    PA_LOG(ERROR) << "Can not create WireMessage because body size exceeds "
                  << "16-bit unsigned integer: " << body_size;
    return std::string();
  }

  uint8_t header[] = {
      static_cast<uint8_t>(kMessageFormatVersionThree),
      static_cast<uint8_t>((body_size >> 8) & 0xFF),
      static_cast<uint8_t>(body_size & 0xFF),
  };
  static_assert(sizeof(header) == kHeaderLength, "Malformed header.");

  std::string header_string(kHeaderLength, 0);
  std::memcpy(&header_string[0], header, kHeaderLength);
  return header_string + json_body;
}

WireMessage::WireMessage(const std::string& payload,
                         const std::string& feature,
                         int sequence_number)
    : payload_(payload), feature_(feature), sequence_number_(sequence_number) {}

WireMessage::WireMessage(const std::string& payload, const std::string& feature)
    : payload_(payload), feature_(feature) {}

WireMessage::WireMessage(const std::string& body) : body_(body) {}

WireMessage::WireMessage(const WireMessage& other)
    : payload_(other.payload_),
      feature_(other.feature_),
      body_(other.body_),
      sequence_number_(other.sequence_number_) {}

WireMessage& WireMessage::operator=(const WireMessage& other) = default;

}  // namespace secure_channel

}  // namespace chromeos
