// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/data_models/boarding_pass.h"

#include <string_view>

#include "components/wallet/core/browser/data_models/wallet_barcode.h"
#include "components/wallet/core/browser/data_models/walletable_pass.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

namespace {

struct ExpectedBoardingPass {
  std::string origin;
  std::string destination;
  std::string airline;
  std::string flight_code;
  std::string date;
};

void TestValue(std::string_view raw_value,
               const ExpectedBoardingPass& expected) {
  WalletBarcode barcode;
  barcode.raw_value = std::string(raw_value);
  barcode.format = WalletBarcodeFormat::PDF417;

  std::optional<BoardingPass> result = BoardingPass::FromBarcode(barcode);

  ASSERT_TRUE(result.has_value()) << "Failed to parse: " << raw_value;
  EXPECT_EQ(result->origin, expected.origin);
  EXPECT_EQ(result->destination, expected.destination);
  EXPECT_EQ(result->airline, expected.airline);
  EXPECT_EQ(result->flight_code, expected.flight_code);
  EXPECT_EQ(result->date, expected.date);
}

void TestFailure(std::string_view raw_value) {
  WalletBarcode barcode;
  barcode.raw_value = std::string(raw_value);
  barcode.format = WalletBarcodeFormat::PDF417;

  std::optional<BoardingPass> result = BoardingPass::FromBarcode(barcode);
  EXPECT_FALSE(result.has_value()) << "Should fail to parse: " << raw_value;
}

}  // namespace

TEST(BoardingPassTest, ParseBoardingPass_ValidBCBP) {
  TestValue("M1PASSENGER NAME      EABCDEFGSFOJFKUA 1234 123Y12A 00001100",
            ExpectedBoardingPass{.origin = "SFO",
                                 .destination = "JFK",
                                 .airline = "UA",
                                 .flight_code = "1234",
                                 .date = "123"});
}

TEST(BoardingPassTest, ParseBoardingPass_InvalidBCBP) {
  TestFailure("InvalidBarcode");
}

TEST(BoardingPassTest, ParseEmpty) {
  TestFailure("");
}

TEST(BoardingPassTest, ParseBadMagicInitial) {
  TestFailure("X1SCHUMANN/CLARA      EX37469 NUEAYTXQ 0167 118Y006D0010 33a");
}

TEST(BoardingPassTest, ParseBadLegCount) {
  // BCBP requires at least 1 leg.
  TestFailure("M0SCHUMANN/CLARA      EX37469 NUEAYTXQ 0167 118Y006D0010 33a");
}

TEST(BoardingPassTest, ParseInvalidLegCountFormat) {
  // Legs count must be numeric.
  TestFailure("MASCHUMANN/CLARA      EX37469 NUEAYTXQ 0167 118Y006D0010 33a");
}

TEST(BoardingPassTest, ParseTooShort) {
  TestFailure("M1SOME MORE STUFF BUT NOT ENOUGH");
}

TEST(BoardingPassTest, TestGenericTwoLegs) {
  // Only the first leg is parsed.
  TestValue(
      "M2MOZART/WOLFGANG AMADE4CWX3W PPTCDGAF 0077 137Y022J0048 3004CWX3W "
      "CDGTLSAF 7788 138Y001A0001 300",
      ExpectedBoardingPass{.origin = "PPT",
                           .destination = "CDG",
                           .airline = "AF",
                           .flight_code = "0077",
                           .date = "137"});
}

}  // namespace wallet
