// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APDU_APDU_RESPONSE_H_
#define COMPONENTS_APDU_APDU_RESPONSE_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"

namespace apdu {

// APDU responses are defined as part of ISO 7816-4. Serialized responses
// consist of a data field of varying length, up to a maximum 65536, and a
// two byte status field.
class COMPONENT_EXPORT(APDU) ApduResponse {
 public:
  // Status bytes are specified in ISO 7816-4.
  enum class Status : uint16_t {
    SW_NO_ERROR = 0x9000,
    SW_CONDITIONS_NOT_SATISFIED = 0x6985,
    SW_COMMAND_NOT_ALLOWED = 0x6986,
    SW_WRONG_DATA = 0x6A80,
    SW_WRONG_LENGTH = 0x6700,
    SW_INS_NOT_SUPPORTED = 0x6D00,
  };

  // Create a APDU response from the serialized message.
  static std::optional<ApduResponse> CreateFromMessage(
      base::span<const uint8_t> data);

  ApduResponse(std::vector<uint8_t> data, Status response_status);
  ApduResponse(ApduResponse&& that);
  ApduResponse& operator=(ApduResponse&& that);

  ApduResponse(const ApduResponse&) = delete;
  ApduResponse& operator=(const ApduResponse&) = delete;

  ~ApduResponse();

  std::vector<uint8_t> GetEncodedResponse() const;

  const std::vector<uint8_t>& data() const { return data_; }
  Status status() const { return response_status_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ApduTest, TestDeserializeResponse);

  std::vector<uint8_t> data_;
  Status response_status_;
};

}  // namespace apdu

#endif  // COMPONENTS_APDU_APDU_RESPONSE_H_
