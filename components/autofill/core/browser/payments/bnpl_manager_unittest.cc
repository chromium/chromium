// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class BnplManagerTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    bnpl_manager_ = std::make_unique<BnplManager>(
        autofill_client_->GetPaymentsAutofillClient());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<BnplManager> bnpl_manager_;
};

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to monetary values they represent in micro-units when given empty
// string or zeros.
TEST_F(BnplManagerTest, AmountParser_Zeros) {
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits(""),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$0"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$0.00"), 0ULL);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to monetary values they represent in micro-units when given normal
// format of strings.
TEST_F(BnplManagerTest, AmountParser_NormalCases) {
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 12.34"),
            12'340'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 012.34"),
            12'340'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD 1,234.56"),
            1'234'560'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 1,234.56"),
            1'234'560'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 123.45"),
            123'450'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$0.12"),
            120'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD   0.12"),
            120'000ULL);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to monetary values they represent in micro-units when given input
// string with leading and tailing monetary-representing substrings.
TEST_F(BnplManagerTest, AmountParser_LeadingAndTailingCharacters) {
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$   1,234.56   USD"),
      1'234'560'000ULL);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD $ 1,234.56 USD"),
      1'234'560'000ULL);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("  $ 1,234.56 "),
      1'234'560'000ULL);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD    1234.56    "),
      1'234'560'000ULL);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to std::nullopt when given negative value strings.
TEST_F(BnplManagerTest, AmountParser_NegativeValue) {
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ -1,234.56"),
            std::nullopt);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD -1,234.56"),
      std::nullopt);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD 1,234.56- $"),
      std::nullopt);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to std::nullopt when given incorrect format of strings.
TEST_F(BnplManagerTest, AmountParser_IncorrectFormatOfInputs) {
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ ,123.45"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$1,234.5"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("NaN"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("Inf"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("-Inf"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("1.234E8"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$1.234.56"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 12e2"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 12e2.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 12.23e2"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("E1.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("E1.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("e1.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("-1.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("1.23E"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("1.23e"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("1.23-"),
            std::nullopt);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to std::nullopt when the converted value overflows uint64.
TEST_F(BnplManagerTest, AmountParser_OverflowValue) {
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$19000000000000.00"),
      std::nullopt);
}

}  // namespace autofill::payments
