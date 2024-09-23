// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/apdu/apdu_command.h"

#include "base/check_op.h"

namespace apdu {

namespace {

// APDU command data length is 2 bytes encoded in big endian order.
uint16_t ParseMessageLength(base::span<const uint8_t> message, size_t offset) {
  DCHECK_GE(message.size(), offset + 2);
  return (message[offset] << 8) | message[offset + 1];
}

}  // namespace

std::optional<ApduCommand> ApduCommand::CreateFromMessage(
    base::span<const uint8_t> message) {
  if (message.size() < kApduMinHeader || message.size() > kApduMaxLength)
    return std::nullopt;

  uint8_t cla = message[0];
  uint8_t ins = message[1];
  uint8_t p1 = message[2];
  uint8_t p2 = message[3];

  size_t response_length = 0;
  std::vector<uint8_t> data;

  switch (message.size()) {
    // No data present; no expected response.
    case kApduMinHeader:
      break;
    // Invalid encoding sizes.
    case kApduMinHeader + 1:
    case kApduMinHeader + 2:
      return std::nullopt;
    // No data present; response expected.
    case kApduMinHeader + 3:
      // Fifth byte must be 0.
      if (message[4] != 0) {
        return std::nullopt;
      }
      response_length = ParseMessageLength(message, kApduCommandLengthOffset);
      // Special case where response length of 0x0000 corresponds to 65536
      // as defined in ISO7816-4.
      if (response_length == 0) {
        response_length = kApduMaxResponseLength;
      }
      break;
    default:
      // Fifth byte must be 0.
      if (message[4] != 0) {
        return std::nullopt;
      }
      auto data_length = ParseMessageLength(message, kApduCommandLengthOffset);

      if (message.size() == data_length + kApduCommandDataOffset) {
        // No response expected.
        data.insert(data.end(), message.begin() + kApduCommandDataOffset,
                    message.end());
      } else if (message.size() == data_length + kApduCommandDataOffset + 2) {
        // Maximum response size is stored in final 2 bytes.
        data.insert(data.end(), message.begin() + kApduCommandDataOffset,
                    message.end() - 2);
        auto response_length_offset = kApduCommandDataOffset + data_length;
        response_length = ParseMessageLength(message, response_length_offset);
        // Special case where response length of 0x0000 corresponds to 65536
        // as defined in ISO7816-4.
        if (response_length == 0) {
          response_length = kApduMaxResponseLength;
        }
      } else {
        return std::nullopt;
      }
      break;
  }

  return ApduCommand(cla, ins, p1, p2, response_length, std::move(data));
}

ApduCommand::ApduCommand() = default;

ApduCommand::ApduCommand(uint8_t cla,
                         uint8_t ins,
                         uint8_t p1,
                         uint8_t p2,
                         size_t response_length,
                         std::vector<uint8_t> data)
    : cla_(cla),
      ins_(ins),
      p1_(p1),
      p2_(p2),
      response_length_(response_length),
      data_(std::move(data)) {}

ApduCommand::ApduCommand(ApduCommand&& that) = default;

ApduCommand& ApduCommand::operator=(ApduCommand&& that) = default;

ApduCommand::~ApduCommand() = default;

std::vector<uint8_t> ApduCommand::GetEncodedCommand() const {
  std::vector<uint8_t> encoded = {cla_, ins_, p1_, p2_};

  // If data exists, request size (Lc) is encoded in 3 bytes, with the first
  // byte always being null, and the other two bytes being a big-endian
  // representation of the request size. If data length is 0, response size (Le)
  // will be prepended with a null byte.
  if (!data_.empty()) {
    size_t data_length = data_.size();

    encoded.push_back(0x0);
    if (data_length > kApduMaxDataLength) {
      data_length = kApduMaxDataLength;
    }
    encoded.push_back((data_length >> 8) & 0xff);
    encoded.push_back(data_length & 0xff);
    encoded.insert(encoded.end(), data_.begin(), data_.begin() + data_length);
  } else if (response_length_ > 0) {
    encoded.push_back(0x0);
  }

  if (response_length_ > 0) {
    size_t response_length = response_length_;
    if (response_length > kApduMaxResponseLength) {
      response_length = kApduMaxResponseLength;
    }
    // A zero value represents a response length of 65,536 bytes.
    encoded.push_back((response_length >> 8) & 0xff);
    encoded.push_back(response_length & 0xff);
  }
  return encoded;
}

}  // namespace apdu
