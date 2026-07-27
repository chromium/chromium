// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_util.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace payments {

class PaymentsUtilTest : public testing::Test {
 public:
  PaymentsUtilTest() = default;

  PaymentsUtilTest(const PaymentsUtilTest&) = delete;
  PaymentsUtilTest& operator=(const PaymentsUtilTest&) = delete;

  ~PaymentsUtilTest() override = default;

 protected:
  TestPaymentsDataManager payments_data_manager_;
};

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Normal) {
  payments_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  EXPECT_EQ(123456, GetBillingCustomerId(payments_data_manager_));
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Garbage) {
  payments_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"garbage"));

  EXPECT_EQ(0, GetBillingCustomerId(payments_data_manager_));
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_NoData) {
  // Explictly do not set PaymentsCustomerData. Nothing crashes and the returned
  // customer ID is 0.
  EXPECT_EQ(0, GetBillingCustomerId(payments_data_manager_));
}

TEST_F(PaymentsUtilTest, HasGooglePaymentsAccount_Normal) {
  payments_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  EXPECT_TRUE(HasGooglePaymentsAccount(payments_data_manager_));
}

TEST_F(PaymentsUtilTest, HasGooglePaymentsAccount_NoData) {
  // Explicitly do not set Prefs data. Nothing crashes and returns false.
  EXPECT_FALSE(HasGooglePaymentsAccount(payments_data_manager_));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_EmptyBin) {
  // Create empty supported card bin ranges.
  std::vector<std::pair<int, int>> supported_card_bin_ranges;
  std::u16string card_number = u"4111111111111111";
  // Card number is not supported since the supported bin range is empty.
  EXPECT_FALSE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_SameStartAndEnd) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(411111, 411111)};
  std::u16string card_number = u"4111111111111111";
  // Card number is supported since it is within the range of the same start and
  // end.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_InsideRange) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(411110, 411112)};
  std::u16string card_number = u"4111111111111111";
  // Card number is supported since it is inside the range.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_StartBoundary) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(411111, 422222)};
  std::u16string card_number = u"4111111111111111";
  // Card number is supported since it is at the start boundary.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_EndBoundary) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(410000, 411111)};
  std::u16string card_number = u"4111111111111111";
  // Card number is supported since it is at the end boundary.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_OutOfRange) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(2111, 2111), std::make_pair(412, 413),
      std::make_pair(300, 305)};
  std::u16string card_number = u"4111111111111111";
  // Card number is not supported since it is out of any range.
  EXPECT_FALSE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardNumberSupported_SeparatorStripped) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(4111, 4111)};
  std::u16string card_number = u"4111-1111-1111-1111";
  // The separators are correctly stripped and the card number is supported.
  EXPECT_TRUE(
      IsCreditCardNumberSupported(card_number, supported_card_bin_ranges));
}

namespace {

using ::testing::Pointee;
using ::testing::Property;
using ::testing::VariantWith;

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  explicit MockBrowserAutofillManager(TestAutofillDriver* driver)
      : TestBrowserAutofillManager(driver) {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(void,
              FillOrPreviewForm,
              (mojom::ActionPersistence action_persistence,
               const FormGlobalId& form_id,
               const FieldGlobalId& trigger_field_id,
               const FillingPayload& filling_payload,
               AutofillTriggerSource trigger_source,
               const base::flat_set<FieldGlobalId>& blocked_fields),
              (override));
};

}  // namespace

class PaymentsUtilFillOrPreviewCardTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 TestAutofillDriver,
                                                 MockBrowserAutofillManager> {
 public:
  void SetUp() override {
    InitAutofillClient();
    autofill_client().GetPersonalDataManager().set_payments_data_manager(
        std::make_unique<TestPaymentsDataManager>());
    autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager()
        .SetPrefService(autofill_client().GetPrefs());
    card_ = test::GetMaskedServerCard();
    autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager()
        .AddCreditCard(card_);
    CreateAutofillDriver();
  }

  void TearDown() override { DestroyAutofillClient(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_environment_;
  CreditCard card_;
};

// Tests that filling a regular credit card suggestion calls `FillOrPreviewForm`
// with `kFill` action persistence and the corresponding credit card.
TEST_F(PaymentsUtilFillOrPreviewCardTest, NormalCreditCardFill) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewForm(mojom::ActionPersistence::kFill, form_id, field_id,
                        VariantWith<const CreditCard*>(Pointee(card_)),
                        AutofillTriggerSource::kPopup,
                        base::flat_set<FieldGlobalId>()));

  FillOrPreviewCard(mojom::ActionPersistence::kFill,
                    SuggestionType::kCreditCardEntry,
                    Suggestion::Guid(card_.guid()), autofill_manager(), form_id,
                    field_id, AutofillTriggerSource::kPopup);
}

// Tests that previewing a regular credit card suggestion calls
// `FillOrPreviewForm` with `kPreview` action persistence and the credit card.
TEST_F(PaymentsUtilFillOrPreviewCardTest, NormalCreditCardPreview) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewForm(mojom::ActionPersistence::kPreview, form_id, field_id,
                        VariantWith<const CreditCard*>(Pointee(card_)),
                        AutofillTriggerSource::kPopup,
                        base::flat_set<FieldGlobalId>()));

  FillOrPreviewCard(mojom::ActionPersistence::kPreview,
                    SuggestionType::kCreditCardEntry,
                    Suggestion::Guid(card_.guid()), autofill_manager(), form_id,
                    field_id, AutofillTriggerSource::kPopup);
}

// Tests that filling a virtual credit card suggestion calls
// `FillOrPreviewForm` with a virtual card variant of the credit card.
TEST_F(PaymentsUtilFillOrPreviewCardTest, VirtualCreditCardFill) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewForm(
          mojom::ActionPersistence::kFill, form_id, field_id,
          VariantWith<const CreditCard*>(Pointee(Property(
              &CreditCard::record_type, CreditCard::RecordType::kVirtualCard))),
          AutofillTriggerSource::kPopup, base::flat_set<FieldGlobalId>()));

  FillOrPreviewCard(mojom::ActionPersistence::kFill,
                    SuggestionType::kVirtualCreditCardEntry,
                    Suggestion::Guid(card_.guid()), autofill_manager(), form_id,
                    field_id, AutofillTriggerSource::kPopup);
}

// Tests that previewing a virtual credit card suggestion calls
// `FillOrPreviewForm` with the underlying real credit card.
TEST_F(PaymentsUtilFillOrPreviewCardTest, VirtualCreditCardPreview) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  // Previewing a virtual card suggestion should preview the original card, not
  // a virtual card.
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewForm(mojom::ActionPersistence::kPreview, form_id, field_id,
                        VariantWith<const CreditCard*>(Pointee(card_)),
                        AutofillTriggerSource::kPopup,
                        base::flat_set<FieldGlobalId>()));

  FillOrPreviewCard(mojom::ActionPersistence::kPreview,
                    SuggestionType::kVirtualCreditCardEntry,
                    Suggestion::Guid(card_.guid()), autofill_manager(), form_id,
                    field_id, AutofillTriggerSource::kPopup);
}

}  // namespace payments
}  // namespace autofill
