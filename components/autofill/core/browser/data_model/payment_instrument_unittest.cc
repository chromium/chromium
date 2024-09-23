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
           PaymentInstrument::PaymentRail::kUnknown}),
      /*is_fido_enrolled=*/true);

  EXPECT_EQ(100, payment_instrument.instrument_id());
  EXPECT_EQ(u"test_nickname", payment_instrument.nickname());
  EXPECT_EQ(GURL("http://www.example.com"),
            payment_instrument.display_icon_url());
  EXPECT_TRUE(
      payment_instrument.IsSupported(PaymentInstrument::PaymentRail::kPix));
  EXPECT_TRUE(
      payment_instrument.IsSupported(PaymentInstrument::PaymentRail::kUnknown));
  EXPECT_TRUE(payment_instrument.is_fido_enrolled());
}

TEST(PaymentInstrumentTest, IsSupported_ReturnsFalseForUnsupportedPaymentRail) {
  PaymentInstrument payment_instrument(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}));

  EXPECT_FALSE(
      payment_instrument.IsSupported(PaymentInstrument::PaymentRail::kPix));
}

TEST(PaymentInstrumentTest, IsFidoEnrolled_ReturnsFalse) {
  PaymentInstrument payment_instrument(
      100, u"test_nickname", GURL("http://www.example.com"),
      DenseSet<PaymentInstrument::PaymentRail>({}),
      /*is_fido_enrolled=*/false);

  EXPECT_FALSE(payment_instrument.is_fido_enrolled());
}

}  // namespace autofill
