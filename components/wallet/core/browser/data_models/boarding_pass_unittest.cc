// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/data_models/boarding_pass.h"

#include <string_view>

#include "base/time/time.h"
#include "base/time/time_override.h"
#include "components/wallet/core/browser/data_models/wallet_barcode.h"
#include "components/wallet/core/browser/data_models/wallet_pass.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

namespace {

std::optional<BoardingPass> ParseBoardingPass(
    std::string_view barcode_raw_value) {
  WalletBarcode barcode;
  barcode.raw_value = std::string(barcode_raw_value);
  barcode.format = WalletBarcodeFormat::PDF417;
  return BoardingPass::FromBarcode(barcode);
}

BoardingPass CreateBoardingPass(std::string_view barcode_raw_value,
                                std::string_view origin,
                                std::string_view destination,
                                std::string_view airline,
                                std::string_view flight_code,
                                std::string_view date,
                                std::string_view passenger_name) {
  BoardingPass pass;
  pass.origin = std::string(origin);
  pass.destination = std::string(destination);
  pass.airline = std::string(airline);
  pass.flight_code = std::string(flight_code);
  EXPECT_TRUE(base::Time::FromUTCString(std::string(date).c_str(), &pass.date));
  pass.passenger_name = std::string(passenger_name);
  pass.barcode = WalletBarcode{std::string(barcode_raw_value),
                               WalletBarcodeFormat::PDF417};
  return pass;
}

}  // namespace

TEST(BoardingPassTest, ParseBoardingPass_ValidBCBP) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time t;
        EXPECT_TRUE(base::Time::FromUTCString("2023-06-15 12:00:00", &t));
        return t;
      },
      nullptr, nullptr);

  std::string raw =
      "M1PASSENGER NAME      EABCDEFGSFOJFKUA 1234 123Y12A 00001100";
  EXPECT_EQ(ParseBoardingPass(raw),
            CreateBoardingPass(raw, "SFO", "JFK", "UA", "1234", "2023-05-03",
                               "PASSENGER NAME"));
}

TEST(BoardingPassTest, ParseBoardingPass_YearRollover) {
  // Case 1: Current Dec 2023, Flight Jan (Day 10). Should be 2024.
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time t;
          EXPECT_TRUE(base::Time::FromUTCString("2023-12-15 12:00:00", &t));
          return t;
        },
        nullptr, nullptr);

    // Julian 010 -> Jan 10.
    std::string raw = "M1NameNameNameNameNameE1234567ORGDSTAIRFLIGT010";
    EXPECT_EQ(ParseBoardingPass(raw),
              CreateBoardingPass(raw, "ORG", "DST", "AIR", "FLIGT",
                                 "2024-01-10", "NameNameNameNameName"));
  }

  // Case 2: Current Jan 2024, Flight Dec (Day 360). Should be 2023.
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time t;
          EXPECT_TRUE(base::Time::FromUTCString("2024-01-15 12:00:00", &t));
          return t;
        },
        nullptr, nullptr);

    // Julian 360 -> Dec 26 (in non-leap 2023).
    std::string raw = "M1NameNameNameNameNameE1234567ORGDSTAIRFLIGT360";
    EXPECT_EQ(ParseBoardingPass(raw),
              CreateBoardingPass(raw, "ORG", "DST", "AIR", "FLIGT",
                                 "2023-12-26", "NameNameNameNameName"));
  }
}

TEST(BoardingPassTest, ParseBoardingPass_LeapYear) {
  // Case 1: Leap year 2024. Julian 366 is valid (Dec 31).
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time t;
          EXPECT_TRUE(base::Time::FromUTCString("2024-02-15 12:00:00", &t));
          return t;
        },
        nullptr, nullptr);

    std::string raw = "M1NameNameNameNameNameE1234567ORGDSTAIRFLIGT366";
    EXPECT_EQ(ParseBoardingPass(raw),
              CreateBoardingPass(raw, "ORG", "DST", "AIR", "FLIGT",
                                 "2024-12-31", "NameNameNameNameName"));
  }

  // Case 2: Non-leap year 2023. Julian 366 is invalid.
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time t;
          EXPECT_TRUE(base::Time::FromUTCString("2023-02-15 12:00:00", &t));
          return t;
        },
        nullptr, nullptr);

    std::string raw = "M1NameNameNameNameNameE1234567ORGDSTAIRFLIGT366";
    EXPECT_EQ(ParseBoardingPass(raw), std::nullopt);
  }
}

TEST(BoardingPassTest, ParseBoardingPass_InvalidBCBP) {
  EXPECT_EQ(ParseBoardingPass("InvalidBarcode"), std::nullopt);
}

TEST(BoardingPassTest, ParseEmpty) {
  EXPECT_EQ(ParseBoardingPass(""), std::nullopt);
}

TEST(BoardingPassTest, ParseBadMagicInitial) {
  EXPECT_EQ(ParseBoardingPass(
                "X1SCHUMANN/CLARA      EX37469 NUEAYTXQ 0167 118Y006D0010 33a"),
            std::nullopt);
}

TEST(BoardingPassTest, ParseBadLegCount) {
  // BCBP requires at least 1 leg.
  EXPECT_EQ(ParseBoardingPass(
                "M0SCHUMANN/CLARA      EX37469 NUEAYTXQ 0167 118Y006D0010 33a"),
            std::nullopt);
}

TEST(BoardingPassTest, ParseInvalidLegCountFormat) {
  // Legs count must be numeric.
  EXPECT_EQ(ParseBoardingPass(
                "MASCHUMANN/CLARA      EX37469 NUEAYTXQ 0167 118Y006D0010 33a"),
            std::nullopt);
}

TEST(BoardingPassTest, ParseTooShort) {
  EXPECT_EQ(ParseBoardingPass("M1SOME MORE STUFF BUT NOT ENOUGH"),
            std::nullopt);
}

TEST(BoardingPassTest, ParseInvalidDate) {
  // Invalid Julian date: 000
  EXPECT_EQ(ParseBoardingPass(
                "M1PASSENGER NAME      EABCDEFGSFOJFKUA 1234 000Y12A 00001100"),
            std::nullopt);
  // Invalid Julian date: 367
  EXPECT_EQ(ParseBoardingPass(
                "M1PASSENGER NAME      EABCDEFGSFOJFKUA 1234 367Y12A 00001100"),
            std::nullopt);
  // Invalid Julian date: non-numeric
  EXPECT_EQ(ParseBoardingPass(
                "M1PASSENGER NAME      EABCDEFGSFOJFKUA 1234 ABCY12A 00001100"),
            std::nullopt);
}

TEST(BoardingPassTest, TestGenericTwoLegs) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time t;
        EXPECT_TRUE(base::Time::FromUTCString("2023-06-15 12:00:00", &t));
        return t;
      },
      nullptr, nullptr);

  // Only the first leg is parsed.
  std::string raw =
      "M2MOZART/WOLFGANG AMADE4CWX3W PPTCDGAF 0077 137Y022J0048 3004CWX3W "
      "CDGTLSAF 7788 138Y001A0001 300";
  EXPECT_EQ(ParseBoardingPass(raw),
            CreateBoardingPass(raw, "PPT", "CDG", "AF", "77", "2023-05-17",
                               "MOZART/WOLFGANG AMAD"));
}

TEST(BoardingPassTest, ParseBoardingPass_FlightCodeLeadingZeros) {
  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        base::Time t;
        EXPECT_TRUE(base::Time::FromUTCString("2023-06-15 12:00:00", &t));
        return t;
      },
      nullptr, nullptr);

  // Flight codes with leading zeros should have them removed.
  {
    std::string raw =
        "M1PASSENGER NAME      EABCDEFGSFOJFKUA 0007 123Y12A 00001100";
    EXPECT_EQ(ParseBoardingPass(raw),
              CreateBoardingPass(raw, "SFO", "JFK", "UA", "7", "2023-05-03",
                                 "PASSENGER NAME"));
  }
  {
    std::string raw =
        "M1PASSENGER NAME      EABCDEFGSFOJFKUA 0707 123Y12A 00001100";
    EXPECT_EQ(ParseBoardingPass(raw),
              CreateBoardingPass(raw, "SFO", "JFK", "UA", "707", "2023-05-03",
                                 "PASSENGER NAME"));
  }
  {
    std::string raw =
        "M1PASSENGER NAME      EABCDEFGSFOJFKUA 0000 123Y12A 00001100";
    EXPECT_EQ(ParseBoardingPass(raw),
              CreateBoardingPass(raw, "SFO", "JFK", "UA", "0", "2023-05-03",
                                 "PASSENGER NAME"));
  }
}

}  // namespace wallet
