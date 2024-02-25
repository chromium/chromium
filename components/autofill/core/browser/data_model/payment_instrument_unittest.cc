// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payment_instrument.h"

#include "components/autofill/core/browser/data_model/bank_account.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(PaymentInstrumentTest, VerifyFieldValues) {
  PaymentInstrument payment_instrument(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kPix,
           PaymentInstrument::PaymentRail::kUnknown}));

  EXPECT_EQ(100, payment_instrument.instrument_id());
  EXPECT_EQ(u"test_nickname", payment_instrument.nickname());
  EXPECT_EQ(GURL("http://www.example.com"),
            payment_instrument.display_icon_url());
  EXPECT_TRUE(
      payment_instrument.IsSupported(PaymentInstrument::PaymentRail::kPix));
  EXPECT_TRUE(
      payment_instrument.IsSupported(PaymentInstrument::PaymentRail::kUnknown));
}

TEST(PaymentInstrumentTest, IsSupported_ReturnsFalseForUnsupportedPaymentRail) {
  PaymentInstrument payment_instrument(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));

  EXPECT_FALSE(
      payment_instrument.IsSupported(PaymentInstrument::PaymentRail::kPix));
}

TEST(PaymentInstrumentTest, Compare_InstrumentIdSmaller) {
  PaymentInstrument payment_instrument_1(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));
  PaymentInstrument payment_instrument_2(
      200, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));

  EXPECT_EQ(-1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_InstrumentIdGreater) {
  PaymentInstrument payment_instrument_1(
      200, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));
  PaymentInstrument payment_instrument_2(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));

  EXPECT_EQ(1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_NicknameSmaller) {
  std::u16string nickname_1 = u"nikcname_1";
  PaymentInstrument payment_instrument_1(
      100, nickname_1, GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));
  std::u16string nickname_2 = u"nikcname_2";
  PaymentInstrument payment_instrument_2(
      100, nickname_2, GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));

  // Expect output as -1 since "nickname_1" < "nickname_2".
  EXPECT_EQ(-1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_NicknameGreater) {
  std::u16string nickname_1 = u"nikcname_2";
  PaymentInstrument payment_instrument_1(
      100, nickname_1, GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));
  std::u16string nickname_2 = u"nikcname_1";
  PaymentInstrument payment_instrument_2(
      100, nickname_2, GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));

  // Expect output as 1 since "nickname_2" > "nickname_1".
  EXPECT_EQ(1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_DisplayIconUrlSmaller) {
  GURL display_icon_url_1 = GURL("http://www.example1.com");
  PaymentInstrument payment_instrument_1(
      100, u"test_nickname", display_icon_url_1,
      DenseSet<PaymentInstrument::PaymentRail>({}));
  GURL display_icon_url_2 = GURL("http://www.example2.com");
  PaymentInstrument payment_instrument_2(
      100, u"test_nickname", display_icon_url_2,
      DenseSet<PaymentInstrument::PaymentRail>({}));

  // Expect output as -1 since
  // "http://www.example1.com" < "http://www.example2.com"
  EXPECT_EQ(-1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_DisplayIconUrlGreater) {
  GURL display_icon_url_1 = GURL("http://www.example2.com");
  PaymentInstrument payment_instrument_1(
      100, u"test_nickname", display_icon_url_1,
      DenseSet<PaymentInstrument::PaymentRail>({}));
  GURL display_icon_url_2 = GURL("http://www.example1.com");
  PaymentInstrument payment_instrument_2(
      100, u"test_nickname", display_icon_url_2,
      DenseSet<PaymentInstrument::PaymentRail>({}));

  // Expect output as 1 since
  // "http://www.example2.com" > "http://www.example1.com"
  EXPECT_EQ(1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_PaymentRailSmaller) {
  PaymentInstrument payment_instrument_1(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kUnknown}));
  PaymentInstrument payment_instrument_2(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kPix}));

  // Expect output of kUnknown - kPix.
  EXPECT_EQ(-1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_PaymentRailGreater) {
  PaymentInstrument payment_instrument_1(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kPix}));
  PaymentInstrument payment_instrument_2(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kUnknown}));

  // Expect output of  kPix - kUnknown.
  EXPECT_EQ(1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_PaymentInstrument2NoPaymentRails) {
  PaymentInstrument payment_instrument_1(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kPix,
           PaymentInstrument::PaymentRail::kUnknown}));
  PaymentInstrument payment_instrument_2(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));

  // Since payment_instrument2 does not have any rails, we expect
  // payment_instrument1 to be greater than payment_instrument2 and thus
  // return 1.
  EXPECT_EQ(1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_PaymentInstrument1NoPaymentRails) {
  PaymentInstrument payment_instrument_1(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));
  PaymentInstrument payment_instrument_2(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kPix,
           PaymentInstrument::PaymentRail::kUnknown}));

  // Since payment_instrument1 does not have any rails, we expect
  // payment_instrument2 to be greater than payment_instrument1 and thus return
  // -1.
  EXPECT_EQ(-1, payment_instrument_1.Compare(payment_instrument_2));
}

TEST(PaymentInstrumentTest, Compare_IdenticalPaymentInstruments) {
  PaymentInstrument payment_instrument_1(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kPix}));
  PaymentInstrument payment_instrument_2(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kPix}));

  EXPECT_EQ(0, payment_instrument_1.Compare(payment_instrument_2));
}

}  // namespace autofill
