// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/form_data_importer.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_field_test_api.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager_test_api.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_utils.h"
#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/plus_addresses/mock_autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/test/mock_multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/payments/test/mock_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"

namespace autofill {
namespace {

using ::testing::Return;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;

// Constructs a FormStructure with one address section and one payment section.
std::unique_ptr<FormStructure> ConstructAddressAndCreditCardForm() {
  TypeValuePairs a = GetDefaultProfileTypeValuePairs();
  TypeValuePairs b = GetDefaultCreditCardTypeValuePairs();
  a.reserve(a.size() + b.size());
  std::ranges::move(b, std::back_inserter(a));
  return ConstructFormStructureFromTypeValuePairs(a);
}

// Constructs a |FormData| instance with two address sections by concatenating
// the default profile and second profile.
FormData ConstructDefaultFormDataWithTwoAddresses() {
  TypeValuePairs a = GetDefaultProfileTypeValuePairs();
  TypeValuePairs b = GetSecondProfileTypeValuePairs();
  a.reserve(a.size() + b.size());
  std::ranges::move(b, std::back_inserter(a));
  return ConstructFormDateFromTypeValuePairs(a);
}

// Matches an AddressProfile or CreditCard pointer according to Compare().
// Takes `expected` by value to avoid a dangling reference.
template <typename T>
auto ComparesEqual(T expected) {
  return Truly([expected = std::move(expected)](const T& actual) {
    return actual.Compare(expected) == 0;
  });
}

// The below matchers follow ::testing::UnorderedElementsAre[Array] except that
// they accept AutofillProfile or CreditCard *pointers* and compare their
// pointees using ComparesEqual().

template <typename T>
auto UnorderedElementsCompareEqualArray(const std::vector<T>& expected_values) {
  std::vector<testing::Matcher<const T*>> matchers;
  for (const T& expected : expected_values) {
    matchers.push_back(Pointee(ComparesEqual(expected)));
  }
  return UnorderedElementsAreArray(matchers);
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

class FormDataImporterTest : public testing::Test {
 public:
  FormDataImporterTest() {
    // Advance the clock to year 20XX.
    task_environment().FastForwardBy(base::Days(365) * 31);
  }

  void SetUp() override {
    client().set_test_strike_database(std::make_unique<TestStrikeDatabase>());
    test_api(address_data_manager()).set_auto_accept_address_imports(true);
    personal_data_manager().SetSyncServiceForTest(&sync_service_);

    payments_client().set_multiple_request_payments_network_interface(
        std::make_unique<payments::MockMultipleRequestPaymentsNetworkInterface>(
            client().GetURLLoaderFactory(), *client().GetIdentityManager()));
    auto virtual_card_enrollment_manager =
        std::make_unique<MockVirtualCardEnrollmentManager>(
            &payments_data_manager(),
            static_cast<payments::MultipleRequestPaymentsNetworkInterface*>(
                payments_client().GetMultipleRequestPaymentsNetworkInterface()),
            &client());
    payments_client().set_virtual_card_enrollment_manager(
        std::move(virtual_card_enrollment_manager));
    auto credit_card_save_manager =
        std::make_unique<MockCreditCardSaveManager>(&client());
    test_api(form_data_importer().GetPaymentsFormDataImporter())
        .set_credit_card_save_manager(std::move(credit_card_save_manager));
  }

  // Helper methods that simply forward the call to the private member (to avoid
  // having to friend every test that needs to access the private
  // PersonalDataManager::ImportAddressProfile or ExtractCreditCard).
  // TODO(crbug.com/481379161): This code is currently partially-duplicated in
  //     AddressFDITest and is only used by to-be-migrated address tests. Once
  //     all address tests have migrated over, delete this function.
  void ExtractAddressProfiles(bool extraction_successful,
                              const FormStructure& form,
                              bool allow_save_prompts = true) {
    std::vector<FormDataImporterTestApi::ExtractedAddressProfile>
        extracted_address_profiles;

    EXPECT_EQ(
        extraction_successful,
        test_api(form_data_importer().GetAddressFormDataImporter())
                .ExtractAddressProfiles(form, &extracted_address_profiles) > 0);

    if (!extraction_successful) {
      EXPECT_FALSE(test_api(form_data_importer().GetAddressFormDataImporter())
                       .ProcessExtractedAddressProfiles(
                           extracted_address_profiles, allow_save_prompts,
                           ukm_source_id()));
      return;
    }

    EXPECT_EQ(test_api(form_data_importer().GetAddressFormDataImporter())
                  .ProcessExtractedAddressProfiles(extracted_address_profiles,
                                                   allow_save_prompts,
                                                   ukm_source_id()),
              allow_save_prompts);
  }

  // Verifies that the stored profiles in the PersonalDataManager equal
  // `expected_profiles` with respect to `AutofillProfile::Compare`.
  // Note, that order is taken into account.
  // TODO(crbug.com/481379161): This code is currently partially-duplicated in
  //     AddressFDITest and is only used by to-be-migrated address tests. Once
  //     all address tests have migrated over, delete this function.
  void VerifyExpectationForExtractedAddressProfiles(
      const std::vector<AutofillProfile>& expected_profiles) {
    auto print_profiles = [&] {
      std::ostringstream output;
      output << "Expected:" << std::endl;
      for (const AutofillProfile& p : expected_profiles) {
        output << p << std::endl;
      }
      output << "Observed:" << std::endl;
      for (const AutofillProfile* p : address_data_manager().GetProfiles()) {
        output << *p << std::endl;
      }
      return output.str();
    };
    EXPECT_THAT(address_data_manager().GetProfiles(),
                UnorderedElementsCompareEqualArray(expected_profiles))
        << print_profiles();
  }

  // TODO(crbug.com/481379161): This code is currently partially-duplicated in
  //     AddressFDITest and is only used by to-be-migrated address tests (with
  //     one BrowsingHistory exception). Once all address tests have migrated
  //     over, inline or delete this function.
  void ExtractAddressProfilesAndVerifyExpectation(
      const FormStructure& form,
      const std::vector<AutofillProfile>& expected_profiles) {
    ExtractAddressProfiles(
        /*extraction_successful=*/!expected_profiles.empty(), form);
    VerifyExpectationForExtractedAddressProfiles(expected_profiles);
  }

  // TODO(crbug.com/481379161): This code is currently partially-duplicated in
  //     AddressFDITest and is only used by to-be-migrated address tests. Once
  //     all address tests have migrated over, delete this function.
  void ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(
      const FormStructure& form) {
    ExtractAddressProfilesAndVerifyExpectation(form,
                                               {ConstructDefaultProfile()});
  }

  // TODO(crbug.com/481379161): This code is currently partially-duplicated in
  //     AddressFDITest and is only used by to-be-migrated address tests (with
  //     one BrowsingHistory exception). Once all address tests have migrated
  //     over, inline or delete this function.
  void ImportAddressProfileAndVerifyImportOfNoProfile(
      const FormStructure& form) {
    ExtractAddressProfilesAndVerifyExpectation(form, {});
  }

  TestAddressDataManager& address_data_manager() {
    return personal_data_manager().test_address_data_manager();
  }
  TestAutofillClient& client() { return autofill_client_; }
  payments::TestPaymentsAutofillClient& payments_client() {
    return *client().GetPaymentsAutofillClient();
  }
  MockCreditCardSaveManager& credit_card_save_manager() {
    return *static_cast<MockCreditCardSaveManager*>(
        form_data_importer()
            .GetPaymentsFormDataImporter()
            .GetCreditCardSaveManager());
  }
  FormDataImporter& form_data_importer() {
    return *client().GetFormDataImporter();
  }
  TestPaymentsDataManager& payments_data_manager() {
    return personal_data_manager().test_payments_data_manager();
  }
  TestPersonalDataManager& personal_data_manager() {
    return client().GetPersonalDataManager();
  }
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillUseINAddressModel};
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<PrefService> prefs_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
};

// ExtractFormData tests (both addresses and credit cards).

// Test that a form with both address and credit card sections imports the
// address and the credit card.
TEST_F(FormDataImporterTest, ExtractFormData_OneAddressOneCreditCard) {
  FormData form = ConstructDefaultFormData();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  // Test that the address has been saved.
  AutofillProfile expected_address = ConstructDefaultProfile();
  const std::vector<const AutofillProfile*>& results_addr =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_THAT(*results_addr[0], ComparesEqual(expected_address));

  // Test that the credit card has also been saved.
  CreditCard expected_card = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<const CreditCard*>& results_cards =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results_cards.size());
  EXPECT_THAT(*results_cards[0], ComparesEqual(expected_card));
}

// Test that a form with two address sections and a credit card section does not
// import the address but does import the credit card.
TEST_F(FormDataImporterTest, ExtractFormData_TwoAddressesOneCreditCard) {
  FormData form = ConstructDefaultFormDataWithTwoAddresses();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  // Still returns true because the credit card import was successful.
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  ASSERT_TRUE(extracted_data.extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  // Test that both addresses have been saved.
  EXPECT_EQ(2U, address_data_manager().GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<const CreditCard*>& results =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(expected_card));
}

// Test that a form with both address and credit card sections imports only the
// the credit card if addresses are disabled.
TEST_F(FormDataImporterTest, ExtractFormData_AddressesDisabledOneCreditCard) {
  FormData form = ConstructDefaultFormData();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/false,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  // Test that addresses were not saved.
  EXPECT_EQ(0U, address_data_manager().GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<const CreditCard*>& results =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(expected_card));
}

// Test that a form with both address and credit card sections imports only the
// the address if credit cards are disabled.
TEST_F(FormDataImporterTest, ExtractFormData_OneAddressCreditCardDisabled) {
  FormData form = ConstructDefaultFormData();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/false);
  ASSERT_FALSE(extracted_data.extracted_credit_card);

  // Test that the address has been saved.
  AutofillProfile expected_address = ConstructDefaultProfile();
  const std::vector<const AutofillProfile*>& results_addr =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_THAT(*results_addr[0], ComparesEqual(expected_address));

  // Test that the credit card was not saved.
  const std::vector<const CreditCard*>& results_cards =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(0U, results_cards.size());
}

// Test that a form with both address and credit card sections imports nothing
// if both addressed and credit cards are disabled.
TEST_F(FormDataImporterTest, ExtractFormData_AddressCreditCardDisabled) {
  FormData form = ConstructDefaultFormData();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/false,
      /*payment_methods_autofill_enabled=*/false);
  ASSERT_FALSE(extracted_data.extracted_credit_card);

  // Test that addresses were not saved.
  EXPECT_EQ(0U, address_data_manager().GetProfiles().size());

  // Test that the credit card was not saved.
  const std::vector<const CreditCard*>& results_cards =
      payments_data_manager().GetCreditCards();
  ASSERT_EQ(0U, results_cards.size());
}

// Tests that multi-step candidates profiles are cleared if the browsing history
// is deleted.
TEST_F(FormDataImporterTest, MultiStepImport_DeleteOnBrowsingHistoryCleared) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSplitDefaultProfileFormStructure(/*part=*/1);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  form_data_importer().OnHistoryDeletions(
      /*history_service=*/nullptr,
      history::DeletionInfo::ForUrls(
          {history::URLRow(form_structure->source_url())},
          /*favicon_urls=*/{}));

  form_structure = ConstructSplitDefaultProfileFormStructure(/*part=*/2);
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that the FormAssociator is correctly integrated in FormDataImporter and
// that multiple address form in the same form are associated with each other.
// The functionality itself is tested in form_data_importer_utils_unittest.cc.
TEST_F(FormDataImporterTest, FormAssociator) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructShippingAndBillingFormStructure();
  FormSignature form_signature = form_structure->form_signature();
  // Don't use `ExtractAddressProfileAndVerifyExtractionOfDefaultProfile()`, as
  // this function assumes we know it's an address form already. Form
  // associations are tracked in `ExtractFormData()` instead.
  ExtractFormDataAndProcessAddressCandidates(
      form_data_importer(), *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  std::optional<FormStructure::FormAssociations> associations =
      form_data_importer().GetFormAssociations(form_signature);
  // Expect the same form signature for the two most recent address form, as
  // `form_structure` consists of two sections.
  EXPECT_TRUE(associations);
  EXPECT_EQ(associations->last_address_form_submitted, form_signature);
  EXPECT_EQ(associations->second_last_address_form_submitted, form_signature);
  EXPECT_FALSE(associations->last_credit_card_form_submitted);
}

// Test that Autofill will not try to import from a field that was filled with
// fallback.
TEST_F(FormDataImporterTest,
       GetObservedFieldValues_SkipFieldsFilledWithFallback) {
  AutofillField field;
  field.SetTypeTo(AutofillType(NAME_FIRST),
                  AutofillPredictionSource::kHeuristics);
  field.set_value(u"First");

  base::flat_map<FieldType, std::u16string> observed_field_types =
      test_api(form_data_importer().GetAddressFormDataImporter())
          .GetObservedFieldValues(
              std::to_array<const AutofillField*>({&field}));
  EXPECT_EQ(observed_field_types.size(), 1u);

  // Set the autofilled type of the field as something different from its
  // classified type, representing that the field was filled using this type as
  // fallback.
  field.set_autofilled_type(NAME_FULL);
  observed_field_types =
      test_api(form_data_importer().GetAddressFormDataImporter())
          .GetObservedFieldValues(
              std::to_array<const AutofillField*>({&field}));
  EXPECT_TRUE(observed_field_types.empty());
}

// Test the behavior of Autofill importing from fields with
// autocomplete=unrecognized.
TEST_F(FormDataImporterTest,
       GetObservedFieldValues_ImportFromAutocompleteUnrecognized) {
  AutofillField field;
  field.SetHtmlType(HtmlFieldType::kUnrecognized, HtmlFieldMode::kNone);
  field.SetTypeTo(AutofillType(NAME_FIRST),
                  AutofillPredictionSource::kHeuristics);
  field.set_value(u"First");
  base::flat_map<FieldType, std::u16string> observed_field_types =
      test_api(form_data_importer().GetAddressFormDataImporter())
          .GetObservedFieldValues(
              std::to_array<const AutofillField*>({&field}));
  EXPECT_EQ(observed_field_types.size(), 1u);
}

// Tests that the Autofill.PromptStatus metric is correctly recorded when only
// the address prompt can be shown.
TEST_F(FormDataImporterTest, AutofillPromptStatusMetric_Address) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  test_api(form_data_importer())
      .ImportAndProcessFormData(
          *form_structure, /*profile_autofill_enabled=*/true,
          /*payment_methods_autofill_enabled=*/true, ukm_source_id());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PromptStatus",
      AutofillMetrics::AutofillPromptStatus::kAddressShown, 1);
}

// Tests that the Autofill.PromptStatus metric is correctly recorded when both
// the address and the credit card prompts can be shown.
TEST_F(FormDataImporterTest, AutofillPromptStatusMetric_AddressAndCreditCard) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable)
      .WillOnce(Return(true));
  std::unique_ptr<FormStructure> form_structure =
      ConstructAddressAndCreditCardForm();
  test_api(form_data_importer())
      .ImportAndProcessFormData(
          *form_structure, /*profile_autofill_enabled=*/true,
          /*payment_methods_autofill_enabled=*/true, ukm_source_id());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PromptStatus",
      AutofillMetrics::AutofillPromptStatus::kAddressAndCreditCardShown, 1);
}

}  // namespace
}  // namespace autofill
