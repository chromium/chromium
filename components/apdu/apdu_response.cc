// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/apdu/apdu_response.h"

#include <utility>

#include "base/numerics/safe_conversions.h"

namespace apdu {

// static
std::optional<ApduResponse> ApduResponse::CreateFromMessage(
    base::span<const uint8_t> data) {
  // Invalid message size, data is appended by status byte.
  if (data.size() < 2) {
    return std::nullopt;
  }

  uint16_t status_bytes = data[data.size() - 2] << 8;
  status_bytes |= data[data.size() - 1];

  return ApduResponse(std::vector<uint8_t>(data.begin(), data.end() - 2),
                      static_cast<Status>(status_bytes));
}

ApduResponse::ApduResponse(std::vector<uint8_t> data, Status response_status)
    : data_(std::move(data)), response_status_(response_status) {}

ApduResponse::ApduResponse(ApduResponse&& that) = default;

ApduResponse& ApduResponse::operator=(ApduResponse&& that) = default;

ApduResponse::~ApduResponse() = default;

std::vector<uint8_t> ApduResponse::GetEncodedResponse() const {
  std::vector<uint8_t> encoded_response = data_;
  encoded_response.push_back(
      base::strict_cast<uint16_t>(response_status_) >> 8 & 0xff);
  encoded_response.push_back(base::strict_cast<uint16_t>(response_status_) &
                             0xff);
  return encoded_response;
}

}  // namespace apdu
