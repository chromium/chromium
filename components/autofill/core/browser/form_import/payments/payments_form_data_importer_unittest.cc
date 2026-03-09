// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_utils.h"
#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/payments/test/mock_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {

using test::CreateTestFormField;
using test::CreateTestIbanFormData;
using ::testing::_;
using ::testing::Return;
using ::testing::Truly;

constexpr char kLocale[] = "en_US";
constexpr char kDefaultCreditCardName[] = "Biggie Smalls";
constexpr char kDefaultCreditCardNumber[] = "4111 1111 1111 1111";
constexpr char kDefaultCreditCardExpMonth[] = "01";
constexpr char kDefaultCreditCardExpYear[] = "2999";

// Matches an CreditCard pointer according to Compare(). Takes `expected` by
// value to avoid a dangling reference.
template <typename T>
auto ComparesEqual(T expected) {
  return Truly([expected = std::move(expected)](const T& actual) {
    return actual.Compare(expected) == 0;
  });
}

template <typename... Matchers>
auto UnorderedElementsCompareEqual(Matchers... matchers) {
  return UnorderedElementsAre(Pointee(ComparesEqual(std::move(matchers)))...);
}

// TODO(crbug.com/40270301): Move MockCreditCardSaveManager to new header and cc
// file.
class MockCreditCardSaveManager : public TestCreditCardSaveManager {
 public:
  explicit MockCreditCardSaveManager(AutofillClient* client)
      : TestCreditCardSaveManager(client) {}
  MOCK_METHOD(bool,
              AttemptToOfferCvcLocalSave,
              (const CreditCard& card),
              (override));
  MOCK_METHOD(void,
              AttemptToOfferCvcUploadSave,
              (const CreditCard& card),
              (override));
  MOCK_METHOD(bool,
              ProceedWithSavingIfApplicable,
              (const FormStructure& submitted_form,
               const CreditCard& card,
               payments::PaymentsFormDataImporter::CreditCardImportType
                   credit_card_import_type,
               bool is_credit_card_upstream_enabled,
               ukm::SourceId ukm_source_id),
              (override));
};

}  // namespace

class PaymentsFormDataImporterTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient> {
 public:
  void SetUp() override {
    InitAutofillClient();
    autofill_client().set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());
    personal_data_manager().SetSyncServiceForTest(&sync_service_);

    payments_client().set_multiple_request_payments_network_interface(
        std::make_unique<payments::MockMultipleRequestPaymentsNetworkInterface>(
            autofill_client().GetURLLoaderFactory(),
            *autofill_client().GetIdentityManager()));
    auto virtual_card_enrollment_manager =
        std::make_unique<MockVirtualCardEnrollmentManager>(
            &payments_data_manager(),
            static_cast<payments::MultipleRequestPaymentsNetworkInterface*>(
                payments_client().GetMultipleRequestPaymentsNetworkInterface()),
            &autofill_client());
    payments_client().set_virtual_card_enrollment_manager(
        std::move(virtual_card_enrollment_manager));
    auto credit_card_save_manager =
        std::make_unique<MockCreditCardSaveManager>(&autofill_client());
    test_api(form_data_importer().GetPaymentsFormDataImporter())
        .set_credit_card_save_manager(std::move(credit_card_save_manager));
  }

  void TearDown() override { DestroyAutofillClient(); }

  // Convenience wrapper that calls
  // `FormDataImporter::ExtractFormData()` and subsequently
  // processes the candidates for IBAN import candidate. Returns the result of
  // `FormDataImporter::ProcessIbanImportCandidate()`.
  bool ExtractFormDataAndProcessIbanCandidates(
      const FormStructure& form,
      bool profile_autofill_enabled,
      bool payment_methods_autofill_enabled) {
    FormDataImporterTestApi::ExtractedFormData extracted_data =
        test_api(form_data_importer())
            .ExtractFormData(form, profile_autofill_enabled,
                             payment_methods_autofill_enabled);
    return extracted_data.extracted_iban &&
           payments_form_data_importer().ProcessIbanImportCandidate(
               extracted_data.extracted_iban.value());
  }

  void SubmitFormAndExpectImportedCardWithData(const FormData& form,
                                               const char* exp_name,
                                               const char* exp_cc_num,
                                               const char* exp_cc_month,
                                               const char* exp_cc_year) {
    std::unique_ptr<FormStructure> form_structure =
        ConstructFormStructureFromFormData(form);
    std::optional<CreditCard> extracted_credit_card =
        ExtractCreditCard(*form_structure);
    ASSERT_TRUE(extracted_credit_card);
    payments_data_manager().OnAcceptedLocalCreditCardSave(
        *extracted_credit_card);

    CreditCard expected = test::CreateCreditCardWithInfo(
        exp_name, exp_cc_num, exp_cc_month, exp_cc_year, "");
    EXPECT_THAT(payments_data_manager().GetCreditCards(),
                UnorderedElementsCompareEqual(expected));
  }

  // Helper method that returns a form with full credit card information.
  FormData CreateFullCreditCardForm(const char* name,
                                    const char* number,
                                    const char* month,
                                    const char* year) {
    FormData form;
    form.set_url(GURL("https://www.foo.com"));
    AddFullCreditCardForm(&form, name, number, month, year);
    return form;
  }

  std::optional<CreditCard> ExtractCreditCard(const FormStructure& form) {
    return test_api(payments_form_data_importer()).ExtractCreditCard(form);
  }

  FormDataImporter& form_data_importer() {
    return *autofill_client().GetFormDataImporter();
  }

  PaymentsFormDataImporter& payments_form_data_importer() {
    return form_data_importer().GetPaymentsFormDataImporter();
  }

  TestPersonalDataManager& personal_data_manager() {
    return autofill_client().GetPersonalDataManager();
  }

  TestPaymentsDataManager& payments_data_manager() {
    return personal_data_manager().test_payments_data_manager();
  }

  payments::TestPaymentsAutofillClient& payments_client() {
    return *autofill_client().GetPaymentsAutofillClient();
  }

  payments::MockMandatoryReauthManager& reauth_manager() {
    return *payments_client().GetOrCreatePaymentsMandatoryReauthManager();
  }

  MockVirtualCardEnrollmentManager& virtual_card_enrollment_manager() {
    return *static_cast<MockVirtualCardEnrollmentManager*>(
        payments_client().GetVirtualCardEnrollmentManager());
  }

  MockCreditCardSaveManager& credit_card_save_manager() {
    return *static_cast<MockCreditCardSaveManager*>(
        form_data_importer()
            .GetPaymentsFormDataImporter()
            .GetCreditCardSaveManager());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  syncer::TestSyncService sync_service_;
};

// ExtractCreditCard tests.

// Tests that a valid credit card is extracted.
TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_Valid) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  base::HistogramTester histogram_tester;
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  payments_data_manager().OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));
}

// Tests that an invalid credit card number is not extracted.
TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_InvalidCardNumber) {
  FormData form = CreateFullCreditCardForm("Jim Johansen", "1000000000000000",
                                           "02", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  base::HistogramTester histogram_tester;
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_FALSE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_EXPIRATION_DATE_ONLY,
                                      1);

  ASSERT_EQ(0U, payments_data_manager().GetCreditCards().size());
}

// Tests that FormFieldData::user_input is preferred over FormFieldData::value
// for credit card numbers.
// Using FormFieldData::user_input enables showing the save-card prompt for
// sites which use JavaScript to set the credit-card <input> to '***'.
TEST_F(PaymentsFormDataImporterTest,
       ExtractCreditCard_PreferUserInputForCreditCardNumber) {
  FormData form = CreateFullCreditCardForm("Jim Johansen", "4111111111111111",
                                           "02", "2999");

  FormFieldData* card_number_field =
      test_api(form).FindFieldByNameForTest(u"card_number");
  ASSERT_TRUE(card_number_field != nullptr);
  card_number_field->set_user_input(u"4444333322221111");

  // FormFieldData::user_input for non-credit card fields should be ignored.
  ASSERT_EQ(nullptr, test_api(form).FindFieldByNameForTest(u"cvc"));
  FormFieldData cvc_field =
      CreateTestFormField("CVC", "cvc", "001", FormControlType::kInputText);
  cvc_field.set_user_input(u"002");
  test_api(form).Append(cvc_field);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);

  payments_data_manager().OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Jim Johansen", "4444333322221111", "02", "2999", "", u"001");
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));
}

// Tests that a credit card with an empty expiration can be extracted due to the
// expiration date fix flow.
TEST_F(PaymentsFormDataImporterTest,
       ExtractCreditCard_InvalidExpiryDate_EditableExpirationExpOn) {
  FormData form =
      CreateFullCreditCardForm("Smalls Biggie", "4111-1111-1111-1111", "", "");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  base::HistogramTester histogram_tester;
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_CARD_NUMBER_ONLY, 1);
}

// Tests that an expired credit card can be extracted due to the expiration date
// fix flow.
TEST_F(PaymentsFormDataImporterTest,
       ExtractCreditCard_ExpiredExpiryDate_EditableExpirationExpOn) {
  FormData form = CreateFullCreditCardForm("Smalls Biggie",
                                           "4111-1111-1111-1111", "01", "2000");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  base::HistogramTester histogram_tester;
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_CARD_NUMBER_ONLY, 1);
}

// Tests that a valid credit card is extracted when the option text for month
// select can't be parsed but its value can.
TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_MonthSelectInvalidText) {
  // Add a single valid credit card form with an invalid option value.
  FormData form = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "Feb (2)", "2999");
  // Add option values and contents to the expiration month field.
  ASSERT_EQ(u"exp_month", form.fields()[2].name());
  test_api(form).field(2).set_options({
      {.value = u"1", .text = u"Jan (1)"},
      {.value = u"2", .text = u"Feb (2)"},
      {.value = u"3", .text = u"Mar (3)"},
  });

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  base::HistogramTester histogram_tester;
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  payments_data_manager().OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  // See that the invalid option text was converted to the right value.
  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "02", "2999",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));
}

TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_TwoValidCards) {
  // Start with a single valid credit card form.
  std::unique_ptr<FormStructure> form_structure1 =
      ConstructDefaultCreditCardFormStructure();
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second different valid credit card.
  FormData form2 =
      CreateFullCreditCardForm("", "5500 0000 0000 0004", "02", "2999");

  std::unique_ptr<FormStructure> form_structure2 =
      ConstructFormStructureFromFormData(form2);

  std::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(*form_structure2);
  EXPECT_TRUE(extracted_credit_card2);
  payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card2);

  CreditCard expected2 = test::CreateCreditCardWithInfo(
      "", "5500000000000004", "02", "2999",
      "");  // Imported cards have no billing info.
  // We ignore the order because multiple profiles or credit cards that
  // are added to the SQLite DB within the same second will be returned in GUID
  // (i.e., random) order.
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected, expected2));
}

// This form has the expiration year as one field with MM/YY.
TEST_F(PaymentsFormDataImporterTest,
       ExtractCreditCard_Month2DigitYearCombination) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "John MMYY",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Date:", "exp_date", "05/45",
                           FormControlType::kInputText, "cc-exp", 5)});

  SubmitFormAndExpectImportedCardWithData(form, "John MMYY", "4111111111111111",
                                          "05", "2045");
}

// This form has the expiration year as one field with MM/YYYY.
TEST_F(PaymentsFormDataImporterTest,
       ExtractCreditCard_Month4DigitYearCombination) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "John MMYYYY",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Date:", "exp_date", "05/2045",
                           FormControlType::kInputText, "cc-exp", 7)});

  SubmitFormAndExpectImportedCardWithData(form, "John MMYYYY",
                                          "4111111111111111", "05", "2045");
}

// This form has the expiration year as one field with M/YYYY.
TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_1DigitMonth4DigitYear) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "John MYYYY",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Date:", "exp_date", "5/2045",
                           FormControlType::kInputText, "cc-exp")});

  SubmitFormAndExpectImportedCardWithData(form, "John MYYYY",
                                          "4111111111111111", "05", "2045");
}

// This form has the expiration year as a 2-digit field.
TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_2DigitYear) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "John Smith",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Month:", "exp_month", "05",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Year:", "exp_year", "45",
                           FormControlType::kInputText)});
  test_api(form).field(-1).set_max_length(2);

  SubmitFormAndExpectImportedCardWithData(form, "John Smith",
                                          "4111111111111111", "05", "2045");
}

// Tests that a credit card is extracted when the card matches a masked server
// card.
TEST_F(PaymentsFormDataImporterTest,
       ExtractCreditCard_DuplicateServerCards_ExtractMaskedCard) {
  // Add a masked server card.
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "John Dillinger", "1111" /* Visa */,
                          "01", "2999", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card);
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // Type the same data as the masked card into a form.
  FormData form = CreateFullCreditCardForm("John Dillinger", "4111111111111111",
                                           "01", "2999");

  // The card should not be offered to be saved locally because the feature flag
  // is disabled.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);
  ASSERT_TRUE(extracted_credit_card.value().record_type() ==
              CreditCard::RecordType::kMaskedServerCard);
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractCreditCard_SameCreditCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2998",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2 =
      CreateFullCreditCardForm("Biggie Smalls", "4111 1111 1111 1111", "01",
                               /* different year */ "2999");

  std::unique_ptr<FormStructure> form_structure2 =
      ConstructFormStructureFromFormData(form2);
  std::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(*form_structure2);
  EXPECT_TRUE(extracted_credit_card2);

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2999".
  CreditCard expected2 = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999",
      "");  // Imported cards have no billing info.
  const std::vector<const CreditCard*>& results2 =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_ShouldReturnLocalCard) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2998",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2 =
      CreateFullCreditCardForm("Biggie Smalls", "4111 1111 1111 1111", "01",
                               /* different year */ "2999");

  std::unique_ptr<FormStructure> form_structure2 =
      ConstructFormStructureFromFormData(form2);
  std::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(*form_structure2);
  // The local card is returned after an update.
  EXPECT_TRUE(extracted_credit_card2);
  // Verify the local card from PDM is equal to `extracted_credit_card2`.
  EXPECT_EQ(extracted_credit_card2.value(),
            *payments_data_manager().GetLocalCreditCards()[0]);

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2999".
  CreditCard expected2 = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999",
      "");  // Imported cards have no billing info.
  const std::vector<const CreditCard*>& results2 =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractCreditCard_ShouldReturnLocalCard_WithExtractedCvc) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Create a form with CVC field present and filled.
  FormData form2 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111 1111 1111 1111", "01", "2998");
  test_api(form2).Append(
      CreateTestFormField("CVC:", "cvc", "123", FormControlType::kInputText));

  std::unique_ptr<FormStructure> form_structure2 =
      ConstructFormStructureFromFormData(form2);
  std::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(*form_structure2);

  // The local card is returned after an update.
  EXPECT_TRUE(extracted_credit_card2);
  // Verify the local card from PDM is equal to the
  // `extracted_credit_card2` for card_number and expiration date but not for
  // the CVC.
  const CreditCard local_saved_credit_card =
      *payments_data_manager().GetLocalCreditCards()[0];
  EXPECT_TRUE(extracted_credit_card2->HasSameNumberAs(local_saved_credit_card));
  EXPECT_TRUE(
      extracted_credit_card2->HasSameExpirationDateAs(local_saved_credit_card));
  EXPECT_NE(extracted_credit_card2->cvc(), local_saved_credit_card.cvc());
  EXPECT_EQ(extracted_credit_card2->cvc(), u"123");
}

TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_EmptyCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);

  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2998",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second credit card with no number.
  FormData form2 =
      CreateFullCreditCardForm("Biggie Smalls",
                               /* no number */ nullptr, "01", "2999");

  std::unique_ptr<FormStructure> form_structure2 =
      ConstructFormStructureFromFormData(form2);
  std::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(*form_structure2);
  EXPECT_FALSE(extracted_credit_card2);

  // No change is expected.
  CreditCard expected2 = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2998", "");
  const std::vector<const CreditCard*>& results2 =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_MissingInfoInNew) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  EXPECT_THAT(payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second different valid credit card where the name is missing but
  // the credit card number matches.
  FormData form2 = CreateFullCreditCardForm(
      /* missing name */ nullptr, "4111-1111-1111-1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure2 =
      ConstructFormStructureFromFormData(form2);
  std::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(*form_structure2);
  EXPECT_TRUE(extracted_credit_card2);

  // No change is expected.
  CreditCard expected2 = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<const CreditCard*>& results2 =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));

  // Add a third credit card where the expiration date is missing.
  FormData form3 =
      CreateFullCreditCardForm("Johnny McEnroe", "5555555555554444",
                               /* no month */ nullptr,
                               /* no year */ nullptr);

  std::unique_ptr<FormStructure> form_structure3 =
      ConstructFormStructureFromFormData(form3);
  std::optional<CreditCard> extracted_credit_card3 =
      ExtractCreditCard(*form_structure3);
  EXPECT_FALSE(extracted_credit_card3);

  // No change is expected.
  CreditCard expected3 = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<const CreditCard*>& results3 =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results3.size());
  EXPECT_THAT(*results3[0], ComparesEqual(expected3));
}

TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_MissingInfoInOld) {
  // Start with a single valid credit card stored via the preferences.
  // Note the empty name.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "", "4111111111111111" /* Visa */,
                          "01", "2998", "1");
  payments_data_manager().AddCreditCard(saved_credit_card);

  const std::vector<const CreditCard*>& results1 =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(saved_credit_card, *results1[0]);

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form =
      CreateFullCreditCardForm("Biggie Smalls", "4111-1111-1111-1111", "01",
                               /* different year */ "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);

  // Expect that the newer information is saved.  In this case the year is
  // added to the existing credit card.
  CreditCard expected2 = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "1");
  const std::vector<const CreditCard*>& results2 =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

// We allow the user to store a credit card number with separators via the UI.
// We should not try to re-aggregate the same card with the separators stripped.
TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_SameCardWithSeparators) {
  // Start with a single valid credit card stored via the preferences.
  // Note the separators in the credit card number.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  payments_data_manager().AddCreditCard(saved_credit_card);

  const std::vector<const CreditCard*>& results1 =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_THAT(*results1[0], ComparesEqual(saved_credit_card));

  // Import the same card info, but with different separators in the number.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111-1111-1111-1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);

  // Expect that no new card is saved.
  const std::vector<const CreditCard*>& results2 =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(saved_credit_card));
}

// Ensure that if a verified credit card already exists, aggregated credit cards
// cannot modify it in any way.
TEST_F(PaymentsFormDataImporterTest,
       ExtractCreditCard_ExistingVerifiedCardWithConflict) {
  // Start with a verified credit card.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2998", "");
  EXPECT_TRUE(credit_card.IsVerified());

  payments_data_manager().AddCreditCard(credit_card);
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // Simulate a form submission with conflicting expiration year.
  FormData form =
      CreateFullCreditCardForm("Biggie Smalls", "4111 1111 1111 1111", "01",
                               /* different year */ "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);

  // Expect that the saved credit card is not modified.
  const std::vector<const CreditCard*>& results =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(credit_card));
}

// Tests that if Save and Fill suggestion was clicked on before the form
// extraction, no payments post-checkout flows are offered. But we should still
// log the "submitted card state" metrics correctly.
TEST_F(PaymentsFormDataImporterTest, ExtractCreditCard_SaveAndFillOccurred) {
  FormData form = CreateFullCreditCardForm("Jim Johansen", "4111111111111111",
                                           "02", "2999");
  payments_form_data_importer()
      .fetched_payments_data_context()
      .card_submitted_through_save_and_fill = true;
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  base::HistogramTester histogram_tester;

  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);

  EXPECT_FALSE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  ASSERT_EQ(0U, payments_data_manager().GetCreditCards().size());
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set and
// reset correctly.
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_SecondImportResetsCreditCardRecordType) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  payments_data_manager().AddCreditCard(saved_credit_card);

  const std::vector<const CreditCard*>& results =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(saved_credit_card));

  // Simulate a form submission with the same card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kLocalCard because
  // upload was offered and the card is a local card already on the device.
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kLocalCard);

  // Second form is filled with a new card so
  // `FormDataImporterTest::credit_card_import_type_` should be
  // reset. Simulate a form submission with a new card.
  FormData form2 = CreateFullCreditCardForm("Biggie Smalls", "4012888888881881",
                                            "01", "2999");

  std::unique_ptr<FormStructure> form_structure2 =
      ConstructFormStructureFromFormData(form2);
  auto extracted_data2 = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure2, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data2.extracted_credit_card);
  // |credit_card_import_type_| should be kNewCard because the
  // imported card is not already on the device.
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kNewCard);

  // Third form is an address form and set `payment_methods_autofill_enabled` to
  // be false so that the ExtractCreditCard won't be called.
  // `FormDataImporterTest::credit_card_import_type_` should
  // still be reset even if ExtractCreditCard is not called. Simulate a form
  // submission with no card.
  FormData form3;
  form3.set_url(GURL("https://wwww.foo.com"));
  form3.set_fields({CreateTestFormField("First name:", "first_name", "George",
                                        FormControlType::kInputText),
                    CreateTestFormField("Last name:", "last_name", "Washington",
                                        FormControlType::kInputText),
                    CreateTestFormField("Email:", "email", "bogus@example.com",
                                        FormControlType::kInputText),
                    CreateTestFormField("Address:", "address1", "21 Laussat St",
                                        FormControlType::kInputText),
                    CreateTestFormField("City:", "city", "San Francisco",
                                        FormControlType::kInputText),
                    CreateTestFormField("State:", "state", "California",
                                        FormControlType::kInputText),
                    CreateTestFormField("Zip:", "zip", "94102",
                                        FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure3 =
      ConstructFormStructureFromFormData(form3);
  auto extracted_data3 = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure3, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/false);
  // |credit_card_import_type_| should be NO_CARD because no
  // valid card was imported from the form.
  EXPECT_NE(0u, extracted_data3.extracted_address_profiles.size());
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NewCard) {
  // Simulate a form submission with a new credit card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNewCard because the
  // imported card is not already on the device.
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kNewCard);
}

// Ensures that `credit_card_import_type_` is set correctly.
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_LocalCard) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  payments_data_manager().AddCreditCard(saved_credit_card);

  const std::vector<const CreditCard*>& results =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(saved_credit_card));

  // Simulate a form submission with the same card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kLocalCard because
  // upload was offered and the card is a local card already on the device.
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kLocalCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_MaskedServerCard) {
  // Add a masked server card.
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "Biggie Smalls", "1111" /* Visa */,
                          "01", "2999", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card);
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // Simulate a form submission with the same masked server card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be SERVER_CARD.
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kServerCard);
}

// Ensures that `FormDataImporterTest::credit_card_import_type_` and
// `record_type` denote that a duplicate card was extracted, and it is a server
// card when the flag is on.
TEST_F(
    PaymentsFormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_DuplicateLocalAndMaskedServerCard) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard local_card = test::GetCreditCard();
  test::SetCreditCardInfo(
      &local_card, kDefaultCreditCardName, kDefaultCreditCardNumber /* Visa */,
      kDefaultCreditCardExpMonth, kDefaultCreditCardExpYear, "");
  payments_data_manager().AddCreditCard(local_card);
  // Add a masked server card.
  CreditCard server_card = test::GetMaskedServerCard();
  test::SetCreditCardInfo(
      &server_card, kDefaultCreditCardName, kDefaultCreditCardNumber /* Visa */,
      kDefaultCreditCardExpMonth, kDefaultCreditCardExpYear, "");
  payments_data_manager().AddServerCreditCard(server_card);

  // Simulate a form submission with the same masked server card.
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  EXPECT_EQ(test_api(payments_form_data_importer()).credit_card_import_type(),
            payments::PaymentsFormDataImporter::CreditCardImportType::
                kDuplicateLocalServerCard);
  EXPECT_EQ(extracted_data.extracted_credit_card->record_type(),
            CreditCard::RecordType::kMaskedServerCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NoCard_InvalidCardNumber) {
  // Simulate a form submission using a credit card with an invalid card number.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1112", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNoCard because no
  // valid card was successfully imported from the form.
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NoCard_VirtualCard) {
  // Simulate a form submission using a credit card that is known as a virtual
  // card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  payments_form_data_importer().CacheFetchedVirtualCard(u"1111");
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  EXPECT_TRUE(extracted_data.extracted_credit_card);
  // `credit_card_import_type_` should be `kVirtualCard` because the
  // card extracted from the form was a virtual card.
  EXPECT_EQ(
      test_api(payments_form_data_importer()).credit_card_import_type(),
      payments::PaymentsFormDataImporter::CreditCardImportType::kVirtualCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(
    PaymentsFormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_NewCard_ExpiredCard_WithExpDateFixFlow) {
  // Simulate a form submission with an expired credit card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "1999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNewCard because card
  // was successfully imported from the form via the expiration date fix flow.
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kNewCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NoCard_NoCardOnForm) {
  // Simulate a form submission with no credit card on form.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields({CreateTestFormField("First name:", "first_name", "George",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last name:", "last_name", "Washington",
                                       FormControlType::kInputText),
                   CreateTestFormField("Email:", "email", "bogus@example.com",
                                       FormControlType::kInputText),
                   CreateTestFormField("Address:", "address1", "21 Laussat St",
                                       FormControlType::kInputText),
                   CreateTestFormField("City:", "city", "San Francisco",
                                       FormControlType::kInputText),
                   CreateTestFormField("State:", "state", "California",
                                       FormControlType::kInputText),
                   CreateTestFormField("Zip:", "zip", "94102",
                                       FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // Mandatory re-auth opt-in should not be attempted if no card was extracted
  // from the form.
  EXPECT_CALL(reauth_manager(), ShouldOfferOptin).Times(0);

  ASSERT_FALSE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNoCard because the
  // form doesn't have credit card section.
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that `cvc` is set when a server card is found.
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_ServerCardWithCvc) {
  // Add a valid server card.
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  test::SetCreditCardInfo(&server_card, "John Dillinger",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  payments_data_manager().AddServerCreditCard(server_card);
  ASSERT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // Simulate a form submission with the same card number but different
  // expiration date.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "02", "2999");
  test_api(form).Append(
      CreateTestFormField("CVC:", "cvc", "123", FormControlType::kInputText));
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  EXPECT_EQ(extracted_data.extracted_credit_card->cvc(), u"123");
}

// Ensures that `credit_card_import_type_` is set as kNewCard when there is a
// masked server card with the same last four but different expiration date.
TEST_F(
    PaymentsFormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_MaskedServerCardWithSameLastFour) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "John Dillinger", "1111" /* Visa */,
                          "01", "2999", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card);
  ASSERT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // Simulate a form submission with the card with same last four but different
  // expiration date.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "02", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  ASSERT_EQ(extracted_data.extracted_credit_card->expiration_month(), 2);
  // `credit_card_import_type_` should be kNewCard because a server card with
  // the same card number was found, but they have different expiration date.
  ASSERT_TRUE(
      test_api(payments_form_data_importer()).credit_card_import_type() ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kNewCard);
}

// Ensures that `credit_card_import_type_` is set correctly when there are two
// masked server card with the same last four and the extracted credit card has
// same last four with both of them.
// Also, verify that `SubmittedServerCardExpirationStatus` will be logged only
// once.
// This test includes two cases:
// 1. The extracted credit card has the same expiration with the second masked
//    server card.
// 2. The extracted credit card's expiration date is not the same as any of the
//    the masked server cards.
TEST_F(
    PaymentsFormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_TwoMaskedServerCardWithSameLastFour) {
  CreditCard server_card1(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card1, "John Dillinger", "1111" /* Visa */,
                          "01", "2111", "");
  server_card1.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card1);
  CreditCard server_card2(CreditCard::RecordType::kMaskedServerCard, "a124");
  test::SetCreditCardInfo(&server_card2, "John Dillinger", "1111" /* Visa */,
                          "02", "2112", "");
  server_card2.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card2);
  EXPECT_EQ(2U, payments_data_manager().GetCreditCards().size());

  {
    // A user fills/enters the card's information on a checkout form but changes
    // the expiration date of the card. Ensure that an expiration date mismatch
    // is recorded.
    FormData form = CreateFullCreditCardForm(
        "Clyde Barrow", "4444 3333 2222 1111", "04", "2345");

    base::HistogramTester histogram_tester;
    std::unique_ptr<FormStructure> form_structure =
        ConstructFormStructureFromFormData(form);
    auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
        form_data_importer(), *form_structure,
        /*profile_autofill_enabled=*/true,
        /*payment_methods_autofill_enabled=*/true);
    ASSERT_TRUE(extracted_data.extracted_credit_card);
    // `credit_card_import_type_` should be kNewCard because a masked server
    // card with the same card number was found, but they have different
    // expiration date.
    ASSERT_TRUE(
        test_api(payments_form_data_importer()).credit_card_import_type() ==
        payments::PaymentsFormDataImporter::CreditCardImportType::kNewCard);
    histogram_tester.ExpectUniqueSample(
        "Autofill.SubmittedServerCardExpirationStatus",
        AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH, 1);
  }
  {
    // Simulate a form submission with the card with same last four but same
    // expiration date as the first masked server card.
    FormData form = CreateFullCreditCardForm(
        "Biggie Smalls", "4111 1111 1111 1111", "02", "2112");

    base::HistogramTester histogram_tester;
    std::unique_ptr<FormStructure> form_structure =
        ConstructFormStructureFromFormData(form);
    auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
        form_data_importer(), *form_structure,
        /*profile_autofill_enabled=*/true,
        /*payment_methods_autofill_enabled=*/true);
    ASSERT_TRUE(extracted_data.extracted_credit_card);
    ASSERT_TRUE(extracted_data.extracted_credit_card->Compare(server_card2) ==
                0);
    // `credit_card_import_type_` should be kServerCard because a masked server
    // card with the same card number and expiration date was found.
    ASSERT_TRUE(
        test_api(payments_form_data_importer()).credit_card_import_type() ==
        payments::PaymentsFormDataImporter::CreditCardImportType::kServerCard);
    histogram_tester.ExpectUniqueSample(
        "Autofill.SubmittedServerCardExpirationStatus",
        AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
  }
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ImportIbanRecordType_NoIban) {
  // Simulate a form submission with no IBAN.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_iban);
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_SubmittingIbanFormUpdatesPref) {
  // The pref should always start disabled.
  ASSERT_FALSE(payments_data_manager().IsAutofillHasSeenIbanPrefEnabled());

  // Simulate a form submission with a new IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());
  ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // Submitting the IBAN form permanently enables the pref.
  EXPECT_TRUE(payments_data_manager().IsAutofillHasSeenIbanPrefEnabled());
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_SubmittingCreditCardFormDoesNotUpdateIbanPref) {
  ASSERT_FALSE(payments_data_manager().IsAutofillHasSeenIbanPrefEnabled());
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // Submitting the credit card form won't enable the pref, even if the flag is
  // on.
  EXPECT_FALSE(payments_data_manager().IsAutofillHasSeenIbanPrefEnabled());
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ImportIbanRecordType_IbanAutofill_NewInvalidIban) {
  // Simulate a form submission with a new IBAN.
  // Invalid Kuwait IBAN with incorrect IBAN length.
  // KW16 will be converted into 203216, and the remainder on 97 is 1.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(
          CreateTestIbanFormData("KW1600000000000000000"));
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // IBAN candidate is empty as the value is invalid.
  ASSERT_FALSE(extracted_data.extracted_iban);
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ImportIbanRecordType_IbanAutofill_NewIban) {
  // Simulate a form submission with a new IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  EXPECT_TRUE(extracted_data.extracted_iban);
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ImportIbanRecordType_LocalIban) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  const std::string guid = payments_data_manager().AddAsLocalIban(iban);
  // Should set identifier and record_type manually here as `iban` has been
  // passed by value above in `AddAsLocalIban`, and `AddAsLocalIban` method sets
  // identifier and record_type to the given `iban`.
  iban.set_identifier(Iban::Guid(guid));
  iban.set_record_type(Iban::kLocalIban);

  const std::vector<const Iban*>& results =
      payments_data_manager().GetLocalIbans();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(iban));

  // Simulate a form submission with the same IBAN. The IBAN can be extracted
  // from the form.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(
          CreateTestIbanFormData(/*value=*/test::kIbanValue));
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  EXPECT_TRUE(extracted_data.extracted_iban);
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(PaymentsFormDataImporterTest, DuplicateMaskedServerCard) {
  CreditCard server_card1(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card1, "John Dillinger", "1881" /* Visa */,
                          "01", "2999", "");
  server_card1.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card1);
  CreditCard server_card2(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_card2, "Clyde Barrow",
                          "0005" /* American Express */, "04", "2999", "");
  server_card2.SetNetworkForMaskedCard(kAmericanExpressCard);
  payments_data_manager().AddServerCreditCard(server_card2);
  EXPECT_EQ(2U, payments_data_manager().GetCreditCards().size());

  // A valid credit card form. A user re-enters one of their masked cards.
  // We should not offer to save locally.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "John Dillinger",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4012888888881881",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Month:", "exp_month", "01",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Year:", "exp_year", "2999",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
}

// Tests that a credit card form that is hidden after receiving input still
// imports the card.
TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_HiddenCreditCardFormAfterEntered) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "Biggie Smalls",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Month:", "exp_month", "01",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Year:", "exp_year", "2999",
                           FormControlType::kInputText)});
  for (FormFieldData& field : test_api(form).fields()) {
    field.set_is_focusable(false);
  }

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  // Test that the credit card has been saved.
  CreditCard expected_card = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<const CreditCard*>& results =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(expected_card));
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date month.
TEST_F(PaymentsFormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationMonth) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_card, "Clyde Barrow", "1111" /* Visa */, "04",
                          "2111", "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card);
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Month:", "exp_month", "",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Year:", "exp_year", "2111",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date year.
TEST_F(PaymentsFormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationYear) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_card, "Clyde Barrow", "1111" /* Visa */, "04",
                          "2111", "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card);
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Month:", "exp_month", "08",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Year:", "exp_year", "",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
}

// Ensure that we still offer to save if we have different cards stored as a
// server card and user submitted an invalid expiration date year.
TEST_F(
    PaymentsFormDataImporterTest,
    Metrics_SubmittedDifferentServerCardExpirationStatus_EmptyExpirationYear) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_card, "Clyde Barrow", "1881" /* Visa */, "04",
                          "2111", "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card);
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Month:", "exp_month", "08",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Year:", "exp_year", "",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
}

TEST_F(PaymentsFormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMatch) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "John Dillinger", "1111" /* Visa */,
                          "01", "2111", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card);
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form.  Ensure that
  // an expiration date match is recorded.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Month:", "exp_month", "01",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Year:", "exp_year", "2111",
                           FormControlType::kInputText)});

  base::HistogramTester histogram_tester;
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
}

TEST_F(PaymentsFormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMismatch) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "John Dillinger", "1111" /* Visa */,
                          "01", "2111", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  payments_data_manager().AddServerCreditCard(server_card);
  EXPECT_EQ(1U, payments_data_manager().GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form but changes
  // the expiration date of the card.  Ensure that an expiration date mismatch
  // is recorded.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                           FormControlType::kInputText),
       CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Month:", "exp_month", "04",
                           FormControlType::kInputText),
       CreateTestFormField("Exp Year:", "exp_year", "2345",
                           FormControlType::kInputText)});

  base::HistogramTester histogram_tester;
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH, 1);
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PaymentsFormDataImporterTest,
       ProcessIbanImportCandidate_ShouldOfferLocalSave_NewIban) {
  Iban extracted_iban = test::GetLocalIban();

  EXPECT_TRUE(
      payments_form_data_importer().ProcessIbanImportCandidate(extracted_iban));
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ProcessIbanImportCandidate_NoIban) {
  // Simulate a form submission with an empty Iban.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData(/*value=*/""));

  ASSERT_FALSE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}

TEST_F(
    PaymentsFormDataImporterTest,
    ExtractFormData_ProcessIbanImportCandidate_PaymentMethodsSettingDisabled) {
  // Simulate a form submission with a new IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());

  ASSERT_FALSE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/false));
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ProcessIbanImportCandidate_NewIban) {
  // Simulate a form submission with a new IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());

  EXPECT_TRUE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ProcessIbanImportCandidate_LocalIban) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  payments_data_manager().AddAsLocalIban(iban);

  // Simulate a form submission with the same IBAN. The IBAN should not be
  // offered to be saved, because it already exists as a local IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(
          CreateTestIbanFormData(/*value=*/test::kIbanValue));

  EXPECT_FALSE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}

TEST_F(PaymentsFormDataImporterTest,
       ExtractFormData_ProcessIbanImportCandidate_MaxStrikes) {
  IbanSaveStrikeDatabase iban_save_strike_database =
      IbanSaveStrikeDatabase(autofill_client().GetStrikeDatabase());

  iban_save_strike_database.AddStrikes(
      iban_save_strike_database.GetMaxStrikesLimit(),
      IbanSaveManager::GetPartialIbanHashString(
          test::GetStrippedValue(test::kIbanValue)));

  // Simulate a form submission with a new IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());

  ASSERT_FALSE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(PaymentsFormDataImporterTest,
       ProcessExtractedCreditCard_EmptyCreditCard) {
  std::optional<CreditCard> extracted_credit_card;
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();

  // `PaymentsFormDataImporter`'s `credit_card_import_type_` is set
  // to kLocalCard because we need to make sure we do not return early in the
  // kNewCard case, and kLocalCard with upstream enabled but empty
  // `extracted_credit_card` is the most likely scenario for a crash.
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(
          payments::PaymentsFormDataImporter::CreditCardImportType::kLocalCard);

  EXPECT_FALSE(
      test_api(payments_form_data_importer())
          .ProcessExtractedCreditCard(*form_structure, extracted_credit_card,
                                      /*is_credit_card_upstream_enabled=*/true,
                                      ukm_source_id()));
  personal_data_manager().SetSyncServiceForTest(nullptr);
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PaymentsFormDataImporterTest,
       ProcessExtractedCreditCard_VirtualCardEligible) {
  CreditCard extracted_credit_card = test::GetMaskedServerCard();
  extracted_credit_card.SetNetworkForMaskedCard(kAmericanExpressCard);
  extracted_credit_card.set_instrument_id(1111);
  extracted_credit_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible);
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();

  test_api(payments_form_data_importer())
      .set_credit_card_import_type(payments::PaymentsFormDataImporter::
                                       CreditCardImportType::kServerCard);
  payments_form_data_importer()
      .fetched_payments_data_context()
      .fetched_card_instrument_id = 2222;

  EXPECT_CALL(virtual_card_enrollment_manager(),
              InitVirtualCardEnroll(_, VirtualCardEnrollmentSource::kDownstream,
                                    _, _, _, _))
      .Times(0);

  EXPECT_FALSE(
      test_api(payments_form_data_importer())
          .ProcessExtractedCreditCard(*form_structure, extracted_credit_card,
                                      /*is_credit_card_upstream_enabled=*/true,
                                      ukm_source_id()));

  payments_form_data_importer()
      .fetched_payments_data_context()
      .fetched_card_instrument_id = 1111;
  EXPECT_CALL(virtual_card_enrollment_manager(),
              InitVirtualCardEnroll(_, VirtualCardEnrollmentSource::kDownstream,
                                    _, _, _, _));

  EXPECT_TRUE(
      test_api(payments_form_data_importer())
          .ProcessExtractedCreditCard(*form_structure, extracted_credit_card,
                                      /*is_credit_card_upstream_enabled=*/true,
                                      ukm_source_id()));

  personal_data_manager().SetSyncServiceForTest(nullptr);
}
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// Test that in the case where the MandatoryReauthManager denotes we should not
// offer re-auth opt-in, we do not start the opt-in flow.
TEST_F(PaymentsFormDataImporterTest,
       ProcessExtractedCreditCard_MandatoryReauthNotOffered) {
  CreditCard extracted_credit_card = test::GetVirtualCard();
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  payments_form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kVirtualCard);
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(payments::PaymentsFormDataImporter::
                                       CreditCardImportType::kVirtualCard);

  EXPECT_CALL(reauth_manager(), ShouldOfferOptin).WillOnce(Return(false));
  EXPECT_CALL(reauth_manager(), StartOptInFlow).Times(0);

  test_api(payments_form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, extracted_credit_card,
                                  /*is_credit_card_upstream_enabled=*/true,
                                  ukm_source_id());
}

// Test that in the case where the MandatoryReauthManager denotes re-auth opt-in
// should be offered, but the card is a new card (presumed updated by the user
// after filling), re-auth opt-in is not offered.
TEST_F(PaymentsFormDataImporterTest,
       ProcessExtractedCreditCard_MandatoryReauthNotOffered_NewCard) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  payments_form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalCard);
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(
          payments::PaymentsFormDataImporter::CreditCardImportType::kNewCard);

  EXPECT_CALL(reauth_manager(), ShouldOfferOptin).Times(0);
  EXPECT_CALL(reauth_manager(), StartOptInFlow).Times(0);

  test_api(payments_form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, test::GetCreditCard2(),
                                  /*is_credit_card_upstream_enabled=*/true,
                                  ukm_source_id());
}

// Verifies the legacy behavior when
// `kAutofillPrioritizeSaveCardOverMandatoryReauth` is disabled. Verifies that
// when the conditions for offering mandatory re-auth are met, the re-auth
// bubble is offered immediately and the save card flow is not attempted.
TEST_F(PaymentsFormDataImporterTest,
       ProcessExtractedCreditCard_PrioritizeSaveCard_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillPrioritizeSaveCardOverMandatoryReauth);
  CreditCard extracted_credit_card = test::GetCreditCard2();
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  payments_form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalCard);
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(
          payments::PaymentsFormDataImporter::CreditCardImportType::kLocalCard);

  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable)
      .Times(0);
  EXPECT_CALL(reauth_manager(), ShouldOfferOptin).WillOnce(Return(true));
  EXPECT_CALL(reauth_manager(), StartOptInFlow);

  EXPECT_TRUE(
      test_api(payments_form_data_importer())
          .ProcessExtractedCreditCard(*form_structure, extracted_credit_card,
                                      /*is_credit_card_upstream_enabled=*/true,
                                      ukm_source_id()));

  // Ensure that we reset the record type at the end of the flow.
  EXPECT_FALSE(
      test_api(payments_form_data_importer())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

// Test that when `kAutofillPrioritizeSaveCardOverMandatoryReauth` is enabled,
// the save card bubble is prioritized. If that bubble is shown, the mandatory
// re-auth bubble is not offered.
TEST_F(
    PaymentsFormDataImporterTest,
    ProcessExtractedCreditCard_PrioritizeSaveCard_SaveSucceedsMandatoryReauthNotOffered) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillPrioritizeSaveCardOverMandatoryReauth);

  CreditCard card = test::GetCreditCard();
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(
          payments::PaymentsFormDataImporter::CreditCardImportType::kLocalCard);
  payments_form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalCard);

  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable)
      .WillOnce(Return(true));
  // Verify that the mandatory re-auth flow is never started.
  EXPECT_CALL(reauth_manager(), ShouldOfferOptin).Times(0);
  EXPECT_CALL(reauth_manager(), StartOptInFlow).Times(0);

  test_api(payments_form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, card,
                                  /*is_credit_card_upstream_enabled=*/false,
                                  ukm_source_id());
}

// Test that when `kAutofillPrioritizeSaveCardOverMandatoryReauth` is enabled,
// offering the save card bubble is prioritized. If it fails, we offer the
// mandatory re-auth bubble as a fallback.
TEST_F(
    PaymentsFormDataImporterTest,
    ProcessExtractedCreditCard_PrioritizeSaveCard_SaveCardFailsMandatoryReauthOffered) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillPrioritizeSaveCardOverMandatoryReauth);

  CreditCard card = test::GetCreditCard();
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(
          payments::PaymentsFormDataImporter::CreditCardImportType::kLocalCard);
  payments_form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalCard);

  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable)
      .WillOnce(Return(false));
  // As a fallback, the mandatory re-auth flow should be offered.
  EXPECT_CALL(reauth_manager(), ShouldOfferOptin).WillOnce(Return(true));
  EXPECT_CALL(reauth_manager(), StartOptInFlow).Times(1);

  test_api(payments_form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, card,
                                  /*is_credit_card_upstream_enabled=*/false,
                                  ukm_source_id());
}

// Test that in the case where the MandatoryReauthManager denotes we should
// offer re-auth opt-in, we start the opt-in in IBAN processing flow.
TEST_F(PaymentsFormDataImporterTest,
       ProcessExtractedIban_MandatoryReauthOffered) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());
  payments_form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalIban);

  EXPECT_CALL(reauth_manager(), ShouldOfferOptin).WillOnce(Return(true));
  EXPECT_CALL(reauth_manager(), StartOptInFlow);

  EXPECT_TRUE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));

  // Ensure that we reset the record type at the end of the flow.
  EXPECT_FALSE(
      test_api(payments_form_data_importer())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

// Test that in the case where the MandatoryReauthManager denotes we should not
// offer re-auth opt-in, we do not start the opt-in in IBAN processing flow.
TEST_F(PaymentsFormDataImporterTest,
       ProcessExtractedIban_MandatoryReauthNotOffered) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());

  EXPECT_CALL(reauth_manager(), ShouldOfferOptin).WillOnce(Return(false));
  EXPECT_CALL(reauth_manager(), StartOptInFlow).Times(0);

  EXPECT_TRUE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));

  // Ensure that we reset the record type at the end of the flow.
  EXPECT_FALSE(
      test_api(payments_form_data_importer())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

// Test that ProceedWithSavingIfApplicable gets called for server cards with the
// correct pre-requisites set.
TEST_F(PaymentsFormDataImporterTest,
       ProcessExtractedCreditCard_ProceedWithSavingIfApplicable_Server) {
  CreditCard card = test::WithCvc(test::GetCreditCard(), u"123");
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(payments::PaymentsFormDataImporter::
                                       CreditCardImportType::kServerCard);

  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable);
  test_api(payments_form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, card,
                                  /*is_credit_card_upstream_enabled=*/false,
                                  ukm_source_id());
}

// Test that ProceedWithSavingIfApplicable gets called for local cards with the
// correct pre-requisites set.
TEST_F(PaymentsFormDataImporterTest,
       ProcessExtractedCreditCard_ProceedWithSavingIfApplicable_Local) {
  CreditCard card = test::WithCvc(test::GetCreditCard(), u"123");
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(
          payments::PaymentsFormDataImporter::CreditCardImportType::kLocalCard);

  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable);
  test_api(payments_form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, card,
                                  /*is_credit_card_upstream_enabled=*/false,
                                  ukm_source_id());
}

// Tests that the Autofill.PromptStatus metric is correctly recorded when only
// the credit card prompt can be shown.
TEST_F(PaymentsFormDataImporterTest, AutofillPromptStatusMetric_CreditCard) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable)
      .WillOnce(Return(true));
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  test_api(form_data_importer())
      .ImportAndProcessFormData(
          *form_structure, /*profile_autofill_enabled=*/true,
          /*payment_methods_autofill_enabled=*/true, ukm_source_id());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PromptStatus",
      AutofillMetrics::AutofillPromptStatus::kCreditCardShown, 1);
}

class SkipSaveCardInFormDataImporterTest
    : public PaymentsFormDataImporterTest,
      public testing::WithParamInterface<bool> {
 public:
  SkipSaveCardInFormDataImporterTest() {
    feature_list_.InitWithFeatureState(
        features::kAutofillSkipSaveCardForTabModalPopup,
        IsSkipSaveCardEnabled());
  }
  bool IsSkipSaveCardEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SkipSaveCardInFormDataImporterTest,
                         testing::Bool());

// Test that save card functionality is skipped for tab modal popup only when
// kAutofillSkipSaveCardForTabModalPopup is enabled; otherwise, the card saving
// functionality is started.
TEST_P(SkipSaveCardInFormDataImporterTest,
       ImportAndProcessFormData_TabModalPopup) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(payments::PaymentsFormDataImporter::
                                       CreditCardImportType::kServerCard);
  payments_client().set_is_tab_model_popup(true);

  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable)
      .Times(IsSkipSaveCardEnabled() ? 0 : 1);

  test_api(form_data_importer())
      .ImportAndProcessFormData(
          *form_structure, /*profile_autofill_enabled=*/true,
          /*payment_methods_autofill_enabled=*/true, ukm_source_id());
}

// Test that save card functionality is initiated for non tab modal popups.
TEST_P(SkipSaveCardInFormDataImporterTest,
       ImportAndProcessFormData_StartSaveCardFlow) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  test_api(payments_form_data_importer())
      .set_credit_card_import_type(payments::PaymentsFormDataImporter::
                                       CreditCardImportType::kServerCard);

  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable)
      .Times(1);

  test_api(form_data_importer())
      .ImportAndProcessFormData(
          *form_structure, /*profile_autofill_enabled=*/true,
          /*payment_methods_autofill_enabled=*/true, ukm_source_id());
}

// Test case for credit card extraction.
class FormDataImporterTest_ExtractCreditCardFromForm
    : public PaymentsFormDataImporterTest {
 public:
  enum class Mode { kDefaultValue, kAutofilled, kUserEdited };

  void PushField(FieldType field_type,
                 std::u16string value,
                 Mode mode = Mode::kDefaultValue,
                 size_t offset = 0) {
    AutofillField& f = test_api(form_).PushField();
    f.set_server_predictions({test::CreateFieldPrediction(field_type)});
    f.set_value(std::move(value));
    if (mode == Mode::kAutofilled) {
      f.AddFieldModifier(FieldModifier::kAutofill);
    }
    if (mode == Mode::kUserEdited) {
      f.AddFieldModifier(FieldModifier::kUser);
    }
    f.set_credit_card_number_offset(offset);
  }

  FormStructure form_{/*form=*/{}};
};

// Tests that inconsistent values from different priority classes do not prevent
// import.
// For example, the user-edited "Donald Trump" has higher priority than the
// autofilled "Joe Biden", which has still higher priority than default-value
// "Joe Average".
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm,
       IgnoreInconsistentValuesFromDifferentPriorityClasses) {
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Joe Average",
            Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Joe Biden", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Donald Trump",
            Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444444444444444",
            Mode::kDefaultValue);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444333322221111",
            Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, u"01/2020",
            Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, u"01/2021",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NAME_FULL, kLocale),
            u"Donald Trump");
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NUMBER, kLocale),
            u"4444333322221111");
  EXPECT_EQ(
      r.card.GetInfo(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, kLocale),
      u"01/2021");
  EXPECT_FALSE(r.has_duplicate_credit_card_field_type);
}

// Tests that equivalent values of different types do not prevent import:
// - first name + last names = full name;
// - month + year = expiration date.
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm, MergeDerivedValues) {
  PushField(FieldType::CREDIT_CARD_NAME_FIRST, u"Donald", Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_NAME_LAST, u"Trump", Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Joe Biden", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444333322221111",
            Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_EXP_MONTH, u"12", Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020",
            Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, u"12/20",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NAME_FULL, kLocale),
            u"Donald Trump");
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NUMBER, kLocale),
            u"4444333322221111");
  EXPECT_EQ(
      r.card.GetInfo(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, kLocale),
      u"12/2020");
  EXPECT_FALSE(r.has_duplicate_credit_card_field_type);
}

// Tests detection of inconsistent values (first names "Audrey" and "Katherine")
// in the same priority class (user-edited fields).
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm,
       BlockImportForInconsistentValues) {
  PushField(FieldType::CREDIT_CARD_NAME_FIRST, u"Katherine", Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_NAME_FIRST, u"Audrey", Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_NAME_LAST, u"Hepburn", Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444333322221111",
            Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, u"12/2020",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  ASSERT_TRUE(r.has_duplicate_credit_card_field_type);
}

// Tests that even editing only a first name (without editing the last name) is
// is reflected in the import candidate.
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm, PartialFirstLastNames) {
  PushField(FieldType::CREDIT_CARD_NAME_FIRST, u"Katherine", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NAME_FIRST, u"Audrey", Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_NAME_LAST, u"Hepburn", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444333322221111",
            Mode::kUserEdited);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, u"12/2020",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NAME_FULL, kLocale),
            u"Audrey Hepburn");
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NUMBER, kLocale),
            u"4444333322221111");
  EXPECT_EQ(
      r.card.GetInfo(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, kLocale),
      u"12/2020");
  EXPECT_FALSE(r.has_duplicate_credit_card_field_type);
}

// Tests that split credit card number extraction works in the same priority
// class (user-edited fields).
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm,
       ExtractSplitCreditCardNumber) {
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Joe Biden", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444", Mode::kUserEdited, 0);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"3333", Mode::kUserEdited, 4);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"2222", Mode::kUserEdited, 8);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"1111", Mode::kUserEdited, 12);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, u"01/2021",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NAME_FULL, kLocale),
            u"Joe Biden");
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NUMBER, kLocale),
            u"4444333322221111");
  EXPECT_EQ(
      r.card.GetInfo(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, kLocale),
      u"01/2021");
  EXPECT_FALSE(r.has_duplicate_credit_card_field_type);
}

// Tests that card extraction works when there are both split credit card
// fields and full credit card fields and the card numbers match.
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm,
       SplitCardAndFullCardFieldsMatch) {
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Joe Biden", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444", Mode::kUserEdited, 0);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"3333", Mode::kUserEdited, 4);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"2222", Mode::kUserEdited, 8);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"1111", Mode::kUserEdited, 12);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444333322221111",
            Mode::kUserEdited, 0);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, u"01/2021",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NUMBER, kLocale),
            u"4444333322221111");
  EXPECT_FALSE(r.has_duplicate_credit_card_field_type);
}

// Tests that split credit card number extraction is blocked when there are both
// split credit card fields and full credit card fields and the numbers do not
// match.
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm,
       SplitCardAndFullCardFieldsDoNotMatch) {
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Joe Biden", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444", Mode::kUserEdited, 0);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"3333", Mode::kUserEdited, 4);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"2222", Mode::kUserEdited, 8);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"1111", Mode::kUserEdited, 12);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444333322220000",
            Mode::kUserEdited, 0);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, u"01/2021",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_TRUE(r.has_duplicate_credit_card_field_type);
}

// Tests that split credit card number extraction extracts the last value if any
// of the field is invalid or missing.
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm,
       ExtractsLastFieldIfHasInvalidOrMIssingFields) {
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Joe Biden", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"", Mode::kUserEdited, 0);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"3333", Mode::kUserEdited, 4);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"1", Mode::kUserEdited, 12);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, u"01/2021",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NUMBER, kLocale), u"1");
  EXPECT_FALSE(r.has_duplicate_credit_card_field_type);
}

// Tests that user edited fields take priority and autofilled fields are ignored
// when in conflict.
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm,
       IgnoreDuplicatedFieldsFromDifferentPriorityClasses) {
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Joe Biden", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444", Mode::kUserEdited, 0);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"3333", Mode::kUserEdited, 4);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"1234", Mode::kAutofilled, 4);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"2222", Mode::kUserEdited, 8);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"1111", Mode::kUserEdited, 12);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, u"01/2021",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NUMBER, kLocale),
            u"4444333322221111");
  EXPECT_FALSE(r.has_duplicate_credit_card_field_type);
}

// Tests split credit card number fields in a lower priority class are ignored.
TEST_F(FormDataImporterTest_ExtractCreditCardFromForm,
       IgnoreFieldsFromLowerPriorityClass) {
  PushField(FieldType::CREDIT_CARD_NAME_FULL, u"Joe Biden", Mode::kAutofilled);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"4444", Mode::kUserEdited, 0);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"3333", Mode::kUserEdited, 4);
  PushField(FieldType::CREDIT_CARD_NUMBER, u"2222", Mode::kAutofilled, 8);
  PushField(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, u"01/2021",
            Mode::kUserEdited);
  auto r = payments_form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NAME_FULL, kLocale),
            u"Joe Biden");
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NUMBER, kLocale),
            u"44443333");
  EXPECT_EQ(
      r.card.GetInfo(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, kLocale),
      u"01/2021");
  EXPECT_FALSE(r.has_duplicate_credit_card_field_type);
}

}  // namespace autofill::payments
