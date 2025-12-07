// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/data_models/boarding_pass.h"

#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/wallet/core/browser/data_models/wallet_barcode.h"

namespace wallet {

namespace {

// Encapsulates a string value and provides safe sequential access to it.
// Ensures that no access is made past the string buffer.
class StringStream {
 public:
  explicit StringStream(std::string_view value) : value_(value) {}

  // Returns 'count' chars from the stream.
  std::string_view Get(int count) {
    if (count < 0) {
      error_ = true;
      return std::string_view();
    }

    if (position_ + count > value_.size()) {
      error_ = true;
      return std::string_view();
    }
    std::string_view result = value_.substr(position_, count);
    position_ += count;
    return result;
  }

  // Returns 'count' chars from the stream, trimming whitespace.
  std::string GetStripped(int count) {
    std::string result;
    base::TrimWhitespaceASCII(Get(count), base::TRIM_ALL, &result);
    return result;
  }

  // Returns 'count' chars from the stream, converted to int.
  // Returns -1 on error.
  int GetInt(int count) {
    std::string input = GetStripped(count);
    if (error_) {
      return -1;
    }
    int result = -1;
    if (!base::StringToInt(input, &result)) {
      error_ = true;
      return -1;
    }
    return result;
  }

  // Skips 'count' chars. Skipping past the value size is not an error on its
  // own, but the subsequent Get() will fail.
  void Skip(int count) {
    if (count < 0) {
      error_ = true;
      return;
    }
    position_ += count;
  }

  bool HasError() const { return error_; }

 private:
  const std::string_view value_;
  size_t position_ = 0;
  bool error_ = false;
};

// See
// https://tinkrmind.files.wordpress.com/2017/09/bcbp-implementation-guide-5th-edition-june-2016.pdf
// for more information on boarding pass fields spec.
constexpr int kFormatIndicatorLength = 1;
constexpr int kLegsCountLength = 1;
constexpr int kNameLength = 20;
constexpr int kElectronicTicketIndicatorLength = 1;
constexpr int kPnrCodeLength = 7;
constexpr int kOriginLength = 3;
constexpr int kDestinationLength = 3;
constexpr int kAirlineLength = 3;
constexpr int kFlightCodeLength = 5;
constexpr int kDateLength = 3;

}  // namespace

// static
std::optional<BoardingPass> BoardingPass::FromBarcode(
    const WalletBarcode& barcode) {
  StringStream value(barcode.raw_value);

  // Field 1: Format Indicator (Length 1)
  if (value.Get(kFormatIndicatorLength) != "M") {
    return std::nullopt;
  }

  // Field 2: Number of Flight Legs (Length 1)
  const int legs_count = value.GetInt(kLegsCountLength);
  // The IATA spec allows 1-9 legs, but we restrict to 4 to align with
  // legacy ticketing (max 4 coupons/ticket). More legs usually require
  // multiple boarding passes.
  if (value.HasError() || legs_count < 1 || legs_count > 4) {
    return std::nullopt;
  }

  // Field 3: Passenger Name (Length 20) - Skipping.
  // Field 4: Electronic Ticket Indicator (Length 1) - Skipping.
  value.Skip(kNameLength + kElectronicTicketIndicatorLength);

  BoardingPass pass;
  // First Leg Data:
  // Field 5: PNR Code (Length 7) - Skipping.
  value.Skip(kPnrCodeLength);
  pass.origin = value.GetStripped(kOriginLength);
  pass.destination = value.GetStripped(kDestinationLength);
  pass.airline = value.GetStripped(kAirlineLength);
  pass.flight_code = value.GetStripped(kFlightCodeLength);
  pass.date = value.GetStripped(kDateLength);
  pass.barcode = barcode;

  if (value.HasError()) {
    return std::nullopt;
  }

  return pass;
}

BoardingPass::BoardingPass() = default;
BoardingPass::BoardingPass(const BoardingPass&) = default;
BoardingPass& BoardingPass::operator=(const BoardingPass&) = default;
BoardingPass::BoardingPass(BoardingPass&&) = default;
BoardingPass& BoardingPass::operator=(BoardingPass&&) = default;
BoardingPass::~BoardingPass() = default;

}  // namespace wallet
