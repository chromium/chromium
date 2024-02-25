// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APDU_APDU_COMMAND_H_
#define COMPONENTS_APDU_APDU_COMMAND_H_

#include <cinttypes>
#include <optional>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"

namespace apdu {

// APDU commands are defined as part of ISO 7816-4. Commands can be serialized
// into either short length encodings, where the maximum data length is 256
// bytes, or an extended length encoding, where the maximum data length is 65536
// bytes. This class implements only the extended length encoding. Serialized
// commands consist of a CLA byte, denoting the class of instruction, an INS
// byte, denoting the instruction code, P1 and P2, each one byte denoting
// instruction parameters, a length field (Lc), a data field of length Lc, and
// a maximum expected response length (Le).
class COMPONENT_EXPORT(APDU) ApduCommand {
 public:
  // Constructs an APDU command from the serialized message data.
  static std::optional<ApduCommand> CreateFromMessage(
      base::span<const uint8_t> message);

  ApduCommand();
  ApduCommand(uint8_t cla,
              uint8_t ins,
              uint8_t p1,
              uint8_t p2,
              size_t response_length,
              std::vector<uint8_t> data);
  ApduCommand(ApduCommand&& that);
  ApduCommand& operator=(ApduCommand&& that);

  ApduCommand(const ApduCommand&) = delete;
  ApduCommand& operator=(const ApduCommand&) = delete;

  ~ApduCommand();

  // Returns serialized message data.
  std::vector<uint8_t> GetEncodedCommand() const;

  void set_cla(uint8_t cla) { cla_ = cla; }
  void set_ins(uint8_t ins) { ins_ = ins; }
  void set_p1(uint8_t p1) { p1_ = p1; }
  void set_p2(uint8_t p2) { p2_ = p2; }
  void set_data(std::vector<uint8_t> data) { data_ = std::move(data); }
  void set_response_length(size_t response_length) {
    response_length_ = response_length;
  }

  uint8_t cla() const { return cla_; }
  uint8_t ins() const { return ins_; }
  uint8_t p1() const { return p1_; }
  uint8_t p2() const { return p2_; }
  size_t response_length() const { return response_length_; }
  const std::vector<uint8_t>& data() const { return data_; }

  static constexpr size_t kApduMaxResponseLength = 65536;

 private:
  FRIEND_TEST_ALL_PREFIXES(ApduTest, TestDeserializeBasic);
  FRIEND_TEST_ALL_PREFIXES(ApduTest, TestDeserializeComplex);
  FRIEND_TEST_ALL_PREFIXES(ApduTest, TestSerializeEdgeCases);

  static constexpr size_t kApduMinHeader = 4;
  static constexpr size_t kApduMaxHeader = 7;
  static constexpr size_t kApduCommandDataOffset = 7;
  static constexpr size_t kApduCommandLengthOffset = 5;

  // As defined in ISO7816-4, extended length APDU request data is limited to
  // 16 bits in length with a maximum value of 65535. Response data length is
  // also limited to 16 bits in length with a value of 0x0000 corresponding to
  // a length of 65536.
  static constexpr size_t kApduMaxDataLength = 65535;
  static constexpr size_t kApduMaxLength =
      kApduMaxDataLength + kApduMaxHeader + 2;

  uint8_t cla_ = 0;
  uint8_t ins_ = 0;
  uint8_t p1_ = 0;
  uint8_t p2_ = 0;
  size_t response_length_ = 0;
  std::vector<uint8_t> data_;
};

}  // namespace apdu

#endif  // COMPONENTS_APDU_APDU_COMMAND_H_
