// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_util.h"

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace payments {

class PaymentsUtilTest : public testing::Test {
 public:
  PaymentsUtilTest() {}
  ~PaymentsUtilTest() override {}

 protected:
  TestPersonalDataManager personal_data_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentsUtilTest);
};

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Normal) {
  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  EXPECT_EQ(123456, GetBillingCustomerId(&personal_data_manager_));
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Garbage) {
  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"garbage"));

  EXPECT_EQ(0, GetBillingCustomerId(&personal_data_manager_));
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_NoData) {
  // Explictly do not set PaymentsCustomerData. Nothing crashes and the returned
  // customer ID is 0.
  EXPECT_EQ(0, GetBillingCustomerId(&personal_data_manager_));
}

TEST_F(PaymentsUtilTest, HasGooglePaymentsAccount_Normal) {
  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  EXPECT_TRUE(HasGooglePaymentsAccount(&personal_data_manager_));
}

TEST_F(PaymentsUtilTest, HasGooglePaymentsAccount_NoData) {
  // Explicitly do not set Prefs data. Nothing crashes and returns false.
  EXPECT_FALSE(HasGooglePaymentsAccount(&personal_data_manager_));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_EmptyBin) {
  // Create empty supported card bin ranges.
  std::vector<std::pair<int, int>> supported_card_bin_ranges;
  base::string16 card_number = base::ASCIIToUTF16("4111111111111111");
  // Card number is not supported since the supported bin range is empty.
  EXPECT_FALSE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_SameStartAndEnd) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(411111, 411111)};
  base::string16 card_number = base::ASCIIToUTF16("4111111111111111");
  // Card number is supported since it is within the range of the same start and
  // end.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_InsideRange) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(411110, 411112)};
  base::string16 card_number = base::ASCIIToUTF16("4111111111111111");
  // Card number is supported since it is inside the range.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_StartBoundary) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(411111, 422222)};
  base::string16 card_number = base::ASCIIToUTF16("4111111111111111");
  // Card number is supported since it is at the start boundary.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_EndBoundary) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(410000, 411111)};
  base::string16 card_number = base::ASCIIToUTF16("4111111111111111");
  // Card number is supported since it is at the end boundary.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_OutOfRange) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(2111, 2111), std::make_pair(412, 413),
      std::make_pair(300, 305)};
  base::string16 card_number = base::ASCIIToUTF16("4111111111111111");
  // Card number is not supported since it is out of any range.
  EXPECT_FALSE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_SeparatorStripped) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(4111, 4111)};
  base::string16 card_number = base::ASCIIToUTF16("4111-1111-1111-1111");
  // The separators are correctly stripped and the card number is supported.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

}  // namespace payments
}  // namespace autofill
