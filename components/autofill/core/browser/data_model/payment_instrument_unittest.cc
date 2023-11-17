// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payment_instrument.h"

#include "components/autofill/core/browser/data_model/bank_account.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
class TestPaymentInstrument : public PaymentInstrument {
 public:
  explicit TestPaymentInstrument(int64_t instrument_id)
      : PaymentInstrument(instrument_id,
                          u"test_nickname",
                          GURL("http://www.example.com")) {}
  ~TestPaymentInstrument() override = default;
  // PaymentInstrument
  InstrumentType GetInstrumentType() const override {
    return PaymentInstrument::InstrumentType::kBankAccount;
  }

  bool AddToDatabase(AutofillTable* database) const override { return false; }
  bool UpdateInDatabase(AutofillTable* database) const override {
    return false;
  }
  bool DeleteFromDatabase(AutofillTable* database) const override {
    return false;
  }
};

TEST(PaymentInstrumentTest, VerifyFieldValues) {
  TestPaymentInstrument payment_instrument(100);
  payment_instrument.AddPaymentRail(PaymentInstrument::PaymentRail::kPix);
  payment_instrument.AddPaymentRail(PaymentInstrument::PaymentRail::kUnknown);

  EXPECT_EQ(100, payment_instrument.instrument_id());
  EXPECT_EQ(u"test_nickname", payment_instrument.nickname());
  EXPECT_EQ(GURL("http://www.example.com"),
            payment_instrument.display_icon_url());
}

TEST(PaymentInstrumentTest, AddPaymentRailMultipleTimes_OnlyAddedOnce) {
  TestPaymentInstrument payment_instrument(100);
  payment_instrument.AddPaymentRail(PaymentInstrument::PaymentRail::kPix);
  payment_instrument.AddPaymentRail(PaymentInstrument::PaymentRail::kPix);

  EXPECT_EQ(1u, payment_instrument.supported_rails().size());
}

TEST(PaymentInstrumentTest, IsSupported_ReturnsTrueForSupportedPaymentRail) {
  TestPaymentInstrument payment_instrument(100);
  payment_instrument.AddPaymentRail(PaymentInstrument::PaymentRail::kPix);
  payment_instrument.AddPaymentRail(PaymentInstrument::PaymentRail::kUnknown);

  EXPECT_TRUE(
      payment_instrument.IsSupported(PaymentInstrument::PaymentRail::kPix));
  EXPECT_TRUE(
      payment_instrument.IsSupported(PaymentInstrument::PaymentRail::kUnknown));
}

TEST(PaymentInstrumentTest, IsSupported_ReturnsFalseForUnsupportedPaymentRail) {
  TestPaymentInstrument payment_instrument(100);

  EXPECT_FALSE(
      payment_instrument.IsSupported(PaymentInstrument::PaymentRail::kPix));
}

}  // namespace autofill
