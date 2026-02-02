// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/data_models/boarding_pass.h"

#include <string_view>

#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/wallet/core/browser/data_models/wallet_barcode.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace wallet {
namespace {

bool IsLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Returns the flight date based on the Day of Year (referred to as "Julian
// date") from the barcode. Since the barcode only contains the day (1-366) and
// not the year, the year is inferred based on the current date, assuming a +/-
// 1 month window around the turn of the year.
std::optional<base::Time> GetFlightDate(
    const std::string& date_of_flight_julian) {
  int julian_day;
  if (!base::StringToInt(date_of_flight_julian, &julian_day)) {
    return std::nullopt;
  }

  // Julian day must be within 1-366 (inclusive of leap year extra day).
  if (julian_day < 1 || julian_day > 366) {
    return std::nullopt;
  }

  base::Time::Exploded now_exploded;
  base::Time::Now().UTCExplode(&now_exploded);

  int year = now_exploded.year;
  int current_month = now_exploded.month;

  // The flight date is in January, but the current month is December. This
  // suggests that the flight will happen next year. The converse is used to
  // assume the previous year, for a boarding pass which recently expired.
  // Use local time to determine the current year as perceived by the user.
  // We assume a +/- 1 month window for year inference, which is common
  // practice for Wallet integrated flight dates.
  constexpr int kJanuaryThreshold = 31;
  constexpr int kDecemberThreshold = 334;  // Roughly Dec 1st.
  if (julian_day <= kJanuaryThreshold && current_month == 12) {
    year++;
  } else if (julian_day > kDecemberThreshold && current_month == 1) {
    year--;
  }

  if (julian_day == 366 && !IsLeapYear(year)) {
    return std::nullopt;
  }

  base::Time::Exploded start_of_year_exploded = {};
  start_of_year_exploded.year = year;
  start_of_year_exploded.month = 1;
  start_of_year_exploded.day_of_month = 1;

  // Perform date calculation in UTC to avoid DST discontinuities.
  base::Time start_of_year;
  if (!base::Time::FromUTCExploded(start_of_year_exploded, &start_of_year)) {
    return std::nullopt;
  }

  return start_of_year + base::Days(julian_day - 1);
}

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

// Removes leading zeros from a string, e.g., "007" becomes "7". If the string
// is "000", it becomes "0".
std::string RemoveLeadingZeros(std::string_view s) {
  std::string_view trimmed = base::TrimString(s, "0", base::TRIM_LEADING);
  return (trimmed.empty() && !s.empty()) ? "0" : std::string(trimmed);
}

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

  // Field 3: Passenger Name (Length 20)
  std::string passenger_name = value.GetStripped(kNameLength);

  // Field 4: Electronic Ticket Indicator (Length 1) - Skipping.
  value.Skip(kElectronicTicketIndicatorLength);
  BoardingPass pass;
  // First Leg Data:
  // Field 5: PNR Code (Length 7) - Skipping.
  value.Skip(kPnrCodeLength);
  pass.passenger_name = passenger_name;
  // The order matters because the fields in BCBP are fixed length and each have
  // specified position.
  pass.origin = value.GetStripped(kOriginLength);
  pass.destination = value.GetStripped(kDestinationLength);
  pass.airline = value.GetStripped(kAirlineLength);
  pass.flight_code = RemoveLeadingZeros(value.GetStripped(kFlightCodeLength));
  std::optional<base::Time> date =
      GetFlightDate(value.GetStripped(kDateLength));
  // The flight date is a mandatory field. If it's invalid or missing, the
  // boarding pass is malformed and cannot be parsed.
  if (!date) {
    return std::nullopt;
  }
  pass.date = std::move(*date);
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
