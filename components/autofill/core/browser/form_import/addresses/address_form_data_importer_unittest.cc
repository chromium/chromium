// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"

#include <ostream>
#include <sstream>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager_test_api.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/test/test_sync_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::Pointee;
using ::testing::Truly;

constexpr char kLocale[] = "en_US";

// Wraps `ConstructDefaultProfile()`, but overrides ADDRESS_HOME_COUNTRY with
// `country`.
AutofillProfile ConstructDefaultProfileWithOverriddenCountry(
    const std::string& country) {
  return ConstructProfileFromTypeValuePairs(
      GetDefaultProfileTypeValuePairsWithOverriddenCountry(country));
}

// Matches an AddressProfile pointer according to Compare(). Takes `expected`
// by value to avoid a dangling reference.
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

class AddressFormDataImporterTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient> {
 public:
  AddressFormDataImporterTest() = default;

  void SetUp() override {
    InitAutofillClient();

    autofill_client().set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());
    test_api(address_data_manager()).set_auto_accept_address_imports(true);
    autofill_client().GetPersonalDataManager().SetSyncServiceForTest(
        &sync_service_);
  }

  void TearDown() override { DestroyAutofillClient(); }

  AddressFormDataImporter& GetAddressFormDataImporter() {
    return autofill_client()
        .GetFormDataImporter()
        ->GetAddressFormDataImporter();
  }

  // Helper methods that simply forward the call to the private member (to avoid
  // having to friend every test that needs to access the private
  // PersonalDataManager::ImportAddressProfile).
  void ExtractAddressProfiles(bool extraction_successful,
                              const FormStructure& form,
                              bool allow_save_prompts = true) {
    std::vector<FormDataImporterTestApi::ExtractedAddressProfile>
        extracted_address_profiles;

    EXPECT_EQ(
        extraction_successful,
        test_api(GetAddressFormDataImporter())
                .ExtractAddressProfiles(form, &extracted_address_profiles) > 0);

    if (!extraction_successful) {
      EXPECT_FALSE(test_api(GetAddressFormDataImporter())
                       .ProcessExtractedAddressProfiles(
                           extracted_address_profiles, allow_save_prompts,
                           ukm_source_id()));
      return;
    }

    EXPECT_EQ(test_api(GetAddressFormDataImporter())
                  .ProcessExtractedAddressProfiles(extracted_address_profiles,
                                                   allow_save_prompts,
                                                   ukm_source_id()),
              allow_save_prompts);
  }

  // Verifies that the stored profiles in the PersonalDataManager equal
  // `expected_profiles` with respect to `AutofillProfile::Compare`.
  // Note, that order is taken into account.
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

  void ExtractAddressProfilesAndVerifyExpectation(
      const FormStructure& form,
      const std::vector<AutofillProfile>& expected_profiles) {
    ExtractAddressProfiles(
        /*extraction_successful=*/!expected_profiles.empty(), form);
    VerifyExpectationForExtractedAddressProfiles(expected_profiles);
  }

  TestAddressDataManager& address_data_manager() {
    return autofill_client()
        .GetPersonalDataManager()
        .test_address_data_manager();
  }
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  ukm::SourceId ukm_source_id() { return 123; }

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillUseINAddressModel};
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  syncer::TestSyncService sync_service_;
};

// Tests that the country is not complemented if a country is part of the form.
TEST_F(AddressFormDataImporterTest, ComplementCountry_PartOfForm) {
  AutofillProfile kDefaultGermanProfile =
      ConstructDefaultProfileWithOverriddenCountry("DE");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry("Germany"));
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {kDefaultGermanProfile});
}

// Tests that the complemented country prefers the variation country code over
// the app locale (US). The form's country field is left empty.
TEST_F(AddressFormDataImporterTest, ComplementCountry_VariationCountryCode) {
  AutofillProfile kDefaultGermanProfile =
      ConstructDefaultProfileWithOverriddenCountry("DE");

  autofill_client().SetVariationConfigCountryCode(GeoIpCountryCode("DE"));

  // Retrieve a default profile with overridden country and overridden phone
  // number to match kDefaultGermanProfile.
  TypeValuePairs form_structure_pairs =
      GetDefaultProfileTypeValuePairsWithOverriddenCountry("DE");
  // Clear the country to verify that it gets complemented from the variation
  // config.
  SetValueForType(form_structure_pairs, ADDRESS_HOME_COUNTRY, "");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(form_structure_pairs);

  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {kDefaultGermanProfile});
}

// Tests that without a variation country code, the country is complemented by
// the app locale. The form's country field is left empty.
TEST_F(AddressFormDataImporterTest,
       ComplementCountry_VariationConfigCountryCode) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry(""));
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructDefaultProfileWithOverriddenCountry("US")});
}

// Tests that the country is complemented before parsing the phone number. This
// is important, since the phone number validation relies on the profile's
// country for nationally formatted numbers.
TEST_F(AddressFormDataImporterTest, ComplementCountry_PhoneNumberParsing) {
  // This is a nationally formatted German phone number, which libphonenumber
  // doesn't parse under the "US" region.
  const char* kNationalNumber = "01578 7912345";
  const char* kHistogramName = "Autofill.ProfileImport.PhoneNumberParsed";

  AutofillProfile expected_profile =
      ConstructDefaultProfileWithOverriddenCountry("DE");

  // Create an address form with `kNationalNumber` and without a country field.
  TypeValuePairs type_value_pairs =
      GetDefaultProfileTypeValuePairsWithOverriddenCountry("");
  SetValueForType(type_value_pairs, PHONE_HOME_WHOLE_NUMBER, kNationalNumber);
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);

  // The complement country feature prefers the variation country code, so the
  // imported country will have country = "DE" assigned.
  autofill_client().SetVariationConfigCountryCode(GeoIpCountryCode("DE"));

  // Country complemention happens before parsing the phone number. Thus, at the
  // time the number is parsed, we correctly apply the German rules.
  base::HistogramTester histogram_tester;
  // The `expected_profile` can successfully parse the number, as the
  // profile's country is "DE".
  EXPECT_TRUE(expected_profile.SetInfo(
      PHONE_HOME_WHOLE_NUMBER, base::UTF8ToUTF16(kNationalNumber), kLocale));
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {expected_profile});
  histogram_tester.ExpectUniqueSample(kHistogramName, true, 1);
}

TEST_F(AddressFormDataImporterTest,
       GetAddressObservedFieldValues_FiltersPlaceholderValues) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillFilterPlaceholderValuesOnImport);

  AutofillField field1;
  field1.set_value(u"Please select a city");
  field1.SetTypeTo(AutofillType(ADDRESS_HOME_CITY),
                   AutofillPredictionSource::kHeuristics);

  AutofillField field2;
  field2.set_value(u"123 Main St");
  field2.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1),
                   AutofillPredictionSource::kHeuristics);

  std::vector<const AutofillField*> section_fields = {&field1, &field2};
  base::HistogramTester histogram_tester;
  base::flat_map<FieldType, std::u16string> observed_values =
      test_api(GetAddressFormDataImporter())
          .GetObservedFieldValues(section_fields);

  EXPECT_FALSE(observed_values.contains(ADDRESS_HOME_CITY));
  EXPECT_TRUE(observed_values.contains(ADDRESS_HOME_LINE1));

  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileImport.PlaceholderValueRemoved.ByFieldType",
      ADDRESS_HOME_CITY, 1);
}

}  // namespace
}  // namespace autofill
