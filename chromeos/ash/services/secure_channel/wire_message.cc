// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/wire_message.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::secure_channel {

namespace {

// The number of bytes used to represent the protocol version in the message
// header.
const size_t kNumBytesInHeaderProtocolVersion = 1u;

// The number of bytes used to represent the message body size.
const size_t kV3NumBytesInHeaderSize = 2u;  // 16-bit int
const size_t kV4NumBytesInHeaderSize = 4u;  // 32-bit int

// Supported Protocol versions.
const uint8_t kV3HeaderVersion = 0x03;
const uint8_t kV4HeaderVersion = 0x04;

// JSON keys for feature and payload.
const char kFeatureKey[] = "feature";
const char kPayloadKey[] = "payload";

// The default feature value. This is the default for backward compatibility
// reasons; previously, the protocol did not transmit the feature in the
// message, but because EasyUnlock was the only feature used, it didn't matter.
// So, if a message is received without a feature, it is assumed to be
// EasyUnlock by default.
const char kDefaultFeature[] = "easy_unlock";

// Features which were launched before protocol v4 was introduced. If we try to
// serialize a WireMessage whose feature is one of these, we should use the v3
// protocol.
const char* const kV3Features[] = {
    // Authentication protocol (used for all features).
    "auth"

    // Smart Lock.
    "easy_unlock",

    // Instant Tethering.
    "magic_tether",
};

std::unique_ptr<WireMessage> DeserializeJsonMessageBody(
    const std::string& serialized_message_body) {
  std::optional<base::Value> body_value =
      base::JSONReader::Read(serialized_message_body);
  if (!body_value || !body_value->is_dict()) {
    PA_LOG(WARNING) << "Unable to parse message as JSON.";
    return nullptr;
  }

  const base::Value::Dict& body = body_value->GetDict();
  const std::string* payload_base64 = body.FindString(kPayloadKey);
  if (!payload_base64) {
    // Legacy case: Message without a payload.
    return base::WrapUnique(new WireMessage(serialized_message_body));
  }

  if (payload_base64->empty()) {
    PA_LOG(WARNING) << "Message contains empty payload.";
    return nullptr;
  }

  std::string payload;
  if (!base::Base64UrlDecode(*payload_base64,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &payload)) {
    PA_LOG(WARNING) << "Payload contains invalid base64 encoding.";
    return nullptr;
  }

  const std::string* feature = body.FindString(kFeatureKey);
  if (!feature || feature->empty()) {
    return base::WrapUnique(new WireMessage(payload, kDefaultFeature));
  }

  return base::WrapUnique(new WireMessage(payload, *feature));
}

std::unique_ptr<WireMessage> DeserializeV3OrV4Message(
    const std::string& serialized_message,
    bool* is_incomplete_message,
    bool is_v3) {
  const size_t kHeaderSize =
      kNumBytesInHeaderProtocolVersion +
      (is_v3 ? kV3NumBytesInHeaderSize : kV4NumBytesInHeaderSize);

  if (serialized_message.size() < kHeaderSize) {
    PA_LOG(ERROR) << "Message was shorter than expected. "
                  << "Size: " << serialized_message.size() << ", "
                  << "Version: " << (is_v3 ? 3 : 4);
    *is_incomplete_message = true;
    return nullptr;
  }

  // Reads the expected body size, starting after the protocol message portion
  // of the header. Because this value is received over the network, we must
  // convert from big endian to host byte order.
  auto reader =
      base::SpanReader(base::as_byte_span(serialized_message)
                           .subspan(kNumBytesInHeaderProtocolVersion));

  size_t expected_message_length;
  if (is_v3) {
    uint16_t body_length;
    if (!reader.ReadU16BigEndian(body_length)) {
      PA_LOG(ERROR) << "Failed to read v3 message length.";
      *is_incomplete_message = true;
      return nullptr;
    }
    expected_message_length = kHeaderSize + body_length;
  } else {
    uint32_t body_length;
    if (!reader.ReadU32BigEndian(body_length)) {
      PA_LOG(ERROR) << "Failed to read v4 message length.";
      *is_incomplete_message = true;
      return nullptr;
    }
    expected_message_length = kHeaderSize + body_length;
  }

  size_t message_length = serialized_message.size();
  if (message_length != expected_message_length) {
    PA_LOG(ERROR) << "Message length does not match expectation. "
                  << "Size: " << serialized_message.size() << ", "
                  << "Expected size: " << expected_message_length << ", "
                  << "Version: " << (is_v3 ? 3 : 4);
    *is_incomplete_message = message_length < expected_message_length;
    return nullptr;
  }

  *is_incomplete_message = false;
  return DeserializeJsonMessageBody(serialized_message.substr(kHeaderSize));
}

}  // namespace

WireMessage::~WireMessage() = default;

// static
std::unique_ptr<WireMessage> WireMessage::Deserialize(
    const std::string& serialized_message,
    bool* is_incomplete_message) {
  if (serialized_message.empty()) {
    PA_LOG(ERROR) << "Attempted to deserialize empty message.";
    *is_incomplete_message = true;
    return nullptr;
  }

  // The first byte of the message is the protocol version as an unsigned 8-bit
  // integer.
  uint8_t protocol_version = serialized_message[0];

  if (protocol_version == kV3HeaderVersion ||
      protocol_version == kV4HeaderVersion) {
    return DeserializeV3OrV4Message(serialized_message, is_incomplete_message,
                                    protocol_version == kV3HeaderVersion);
  }

  PA_LOG(ERROR) << "Received message with unknown version " << protocol_version;
  *is_incomplete_message = false;
  return nullptr;
}

std::string WireMessage::Serialize() const {
  std::string json_body;
  if (body_.empty()) {
    if (payload_.empty()) {
      PA_LOG(ERROR) << "Failed to serialize empty wire message.";
      return std::string();
    }

    // Create JSON body containing feature and payload.
    base::Value::Dict body;

    std::string base64_payload;
    base::Base64UrlEncode(payload_,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &base64_payload);
    body.Set(kPayloadKey, base64_payload);
    body.Set(kFeatureKey, feature_);

    if (!base::JSONWriter::Write(body, &json_body)) {
      PA_LOG(ERROR) << "Failed to convert WireMessage body to JSON: " << body;
      return std::string();
    }
  } else {
    json_body = body_;
  }

  bool use_v3_encoding = body_.empty() || feature_.empty() ||
                         base::Contains(kV3Features, feature_);

  size_t body_size = json_body.size();
  if (use_v3_encoding && body_size > std::numeric_limits<uint16_t>::max()) {
    PA_LOG(ERROR) << "Can not create WireMessage because body size exceeds "
                  << "16-bit unsigned integer: " << body_size;
    return std::string();
  }

  const size_t kHeaderSize =
      kNumBytesInHeaderProtocolVersion +
      (use_v3_encoding ? kV3NumBytesInHeaderSize : kV4NumBytesInHeaderSize);

  std::string header_string(kHeaderSize, 0);
  base::SpanWriter writer(base::as_writable_byte_span(header_string));
  if (use_v3_encoding) {
    writer.WriteU8BigEndian(kV3HeaderVersion);
    writer.WriteU16BigEndian(static_cast<uint16_t>(body_size));
  } else {
    writer.WriteU8BigEndian(kV4HeaderVersion);
    writer.WriteU32BigEndian(static_cast<uint32_t>(body_size));
  }

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

}  // namespace ash::secure_channel
