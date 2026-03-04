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
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/plus_addresses/mock_autofill_plus_address_delegate.h"
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

using test::CreateTestFormField;
using ::testing::NiceMock;
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

  void ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(
      const FormStructure& form) {
    ExtractAddressProfilesAndVerifyExpectation(form,
                                               {ConstructDefaultProfile()});
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

// This test verifies that a phone number is stored correctly in the following
// situation: A form contains a telephone number field that is classified as
// a PHONE_HOME_CITY_AND_NUMBER field (either due to heuristics or due to
// crowdsourcing). If a user enters an international phone number (e.g. +374 10
// 123456), this must be parsed as such, not as a local number in the assumed
// country. Otherwise, the stored value is incorrect. Before a fix, the
// number quoted above would be stored as "(010) 123456" for a DE address
// profile and not stored at all for a US address profile.
TEST_F(AddressFormDataImporterTest, ParseI18nPhoneNumberInCityAndNumberField) {
  // This is an Armenian phone number
  const char* kInternationalNumber = "+374 10 123456";

  AutofillProfile expected_profile = ConstructDefaultProfile();
  // Despite the US default profile, we expect the international number.
  ASSERT_TRUE(expected_profile.SetInfo(PHONE_HOME_WHOLE_NUMBER,
                                       base::UTF8ToUTF16(kInternationalNumber),
                                       kLocale));

  // Create an address form with `kInternationalNumber`.
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, PHONE_HOME_WHOLE_NUMBER,
                  kInternationalNumber);
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);

  // Replace PHONE_HOME_WHOLE_NUMBER by PHONE_HOME_CITY_AND_NUMBER in field
  // classifications.
  std::vector<FieldType> types;
  for (const auto& field : form_structure->fields()) {
    if (field->heuristic_type() == PHONE_HOME_WHOLE_NUMBER) {
      types.push_back(PHONE_HOME_CITY_AND_NUMBER);
    } else {
      types.push_back(field->heuristic_type());
    }
  }
  test_api(*form_structure.get()).SetFieldTypes(types, types);

  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {expected_profile});
  ASSERT_EQ(address_data_manager().GetProfiles().size(), 1u);
  EXPECT_EQ(base::UTF8ToUTF16(kInternationalNumber),
            address_data_manager().GetProfiles()[0]->GetRawInfo(
                PHONE_HOME_WHOLE_NUMBER));
}

// Tests that invalid countries in submitted forms are ignored, and that the
// complement country logic overwrites it. In this case, expect the country to
// default to the locale's country "US".
TEST_F(AddressFormDataImporterTest, InvalidCountry) {
  // Due to the extra 'A', the country of this `form_structure` is invalid.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry("USAA"));
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that invalid phone numbers are removed and importing continues.
TEST_F(AddressFormDataImporterTest, InvalidPhoneNumber) {
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, PHONE_HOME_WHOLE_NUMBER, "invalid");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);

  auto profile_without_number = ConstructDefaultProfile();
  profile_without_number.ClearFields({PHONE_HOME_WHOLE_NUMBER});
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {profile_without_number});
}

// Tests that active plus addresses are not part of the values captured during
// form submissions.
TEST_F(AddressFormDataImporterTest, ActivePlusAddressesExcluded) {
  const std::string kDummyPlusAddress = "plus+plus@plus.plus";

  // Save `kDummyPlusAddress` into the `plus_address_service`, and configure the
  // `autofill_client()` to use it.
  auto plus_address_delegate =
      std::make_unique<NiceMock<MockAutofillPlusAddressDelegate>>();
  ON_CALL(*plus_address_delegate, IsPlusAddress)
      .WillByDefault([&kDummyPlusAddress](const std::string& address) {
        return address == kDummyPlusAddress;
      });
  autofill_client().set_plus_address_delegate(std::move(plus_address_delegate));

  // Next, make a form with the `kDummyPlusAddress` filled in, which should be
  // excluded from imports.
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, EMAIL_ADDRESS, kDummyPlusAddress);
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);

  // Create a default profile, but remove the email address, since extraction
  // should skip the known plus address.
  AutofillProfile expected_profile = ConstructDefaultProfile();
  expected_profile.ClearFields({EMAIL_ADDRESS});

  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {expected_profile});
}

// Tests that strings matching the plus address format are not part of the
// values captured during form submissions.
TEST_F(AddressFormDataImporterTest, MatchedPlusAddressesExcluded) {
  const std::string kMatchedPlusAddress = "plus+plus@grelay.com";

  // Save `kDummyPlusAddress` into the `plus_address_service`, and configure the
  // `autofill_client()` to use it.
  auto plus_address_delegate =
      std::make_unique<NiceMock<MockAutofillPlusAddressDelegate>>();
  ON_CALL(*plus_address_delegate, IsPlusAddress)
      .WillByDefault([](const std::string& address) {
        return address.ends_with("@grelay.com");
      });
  autofill_client().set_plus_address_delegate(std::move(plus_address_delegate));

  // Next, make a form with the `kDummyPlusAddress` filled in, which should be
  // excluded from imports.
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, EMAIL_ADDRESS, kMatchedPlusAddress);
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);

  // Create a default profile, but remove the email address, since extraction
  // should skip the known plus address.
  AutofillProfile expected_profile = ConstructDefaultProfile();
  expected_profile.ClearFields({EMAIL_ADDRESS});

  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {expected_profile});
}

// ImportAddressProfiles tests.
TEST_F(AddressFormDataImporterTest, ImportStructuredNameProfile) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));

  form.set_fields(
      {CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Address:", "address1", "21 Laussat St",
                           FormControlType::kInputText),
       CreateTestFormField("City:", "city", "San Francisco",
                           FormControlType::kInputText),
       CreateTestFormField("State:", "state", "California",
                           FormControlType::kInputText),
       CreateTestFormField("Country:", "country", "ES",
                           FormControlType::kInputText),
       CreateTestFormField("Zip:", "zip", "94102",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure);

  const std::vector<const AutofillProfile*>& results =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"21");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Laussat St");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"21 Laussat St");

  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kParsed);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kParsed);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            VerificationStatus::kObserved);
}

TEST_F(AddressFormDataImporterTest,
       ImportStructuredAddressProfile_StreetNameAndHouseNumber) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Street name:", "street_name", "Laussat St",
                           FormControlType::kInputText),
       CreateTestFormField("House number:", "house_number", "21",
                           FormControlType::kInputText),
       CreateTestFormField("City:", "city", "San Francisco",
                           FormControlType::kInputText),
       CreateTestFormField("State:", "state", "California",
                           FormControlType::kInputText),
       CreateTestFormField("Country:", "country", "ES",
                           FormControlType::kInputText),
       CreateTestFormField("Zip:", "zip", "94102",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure);

  const std::vector<const AutofillProfile*>& results =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"21");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Laussat St");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"Laussat St 21");

  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            VerificationStatus::kFormatted);
}

TEST_F(
    AddressFormDataImporterTest,
    ImportStructuredAddressProfile_StreetNameAndHouseNumberAndApartmentNumber) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Street name:", "street_name", "Laussat St",
                           FormControlType::kInputText),
       CreateTestFormField("House number:", "house_number", "21",
                           FormControlType::kInputText),
       CreateTestFormField("Apartment", "apartment", "101",
                           FormControlType::kInputText),
       CreateTestFormField("City:", "city", "Oaxaca de Juárez",
                           FormControlType::kInputText),
       CreateTestFormField("State:", "state", "Oaxaca",
                           FormControlType::kInputText),
       CreateTestFormField("Country:", "country", "MX",
                           FormControlType::kInputText),
       CreateTestFormField("Zip:", "zip", "94102",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form, GeoIpCountryCode("MX"));
  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure);

  const std::vector<const AutofillProfile*>& results =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"21");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Laussat St");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"Laussat St 21, 101");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_APT_NUM), u"101");
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            VerificationStatus::kFormatted);
}

TEST_F(AddressFormDataImporterTest,
       ImportStructuredAddressProfile_GermanStreetNameAndHouseNumber) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Street name:", "street_name", "Hermann Strasse",
                           FormControlType::kInputText),
       CreateTestFormField("House number:", "house_number", "23",
                           FormControlType::kInputText),
       CreateTestFormField("City:", "city", "Munich",
                           FormControlType::kInputText),
       CreateTestFormField("Country:", "country", "Germany",
                           FormControlType::kInputText),
       CreateTestFormField("Zip:", "zip", "80992",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure);

  const std::vector<const AutofillProfile*>& results =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"23");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME),
            u"Hermann Strasse");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"Hermann Strasse 23");

  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            VerificationStatus::kFormatted);
}

TEST_F(AddressFormDataImporterTest,
       ImportStructuredAddressProfile_I18nAddressFormMX) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Nombre y apellidos:", "name", "Pablo Ruiz",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Ciudad:", "ciudad", "Guadalajara",
                           FormControlType::kInputText),
       CreateTestFormField("Country:", "country", "Mexico",
                           FormControlType::kInputText),
       CreateTestFormField("Calle y número", "street",
                           "Avenida Álvaro Obregón 1234",
                           FormControlType::kInputText),
       CreateTestFormField("Referencia y entre calles:", "between_streets",
                           "Entre Calles Tonalá y Monterrey",
                           FormControlType::kInputText),
       CreateTestFormField("Zip:", "zip", "80992",
                           FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form, GeoIpCountryCode("MX"));
  AutofillProfile mx_profile(AddressCountryCode("MX"));
  mx_profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"Pablo Ruiz",
                                              VerificationStatus::kObserved);
  mx_profile.SetRawInfoWithVerificationStatus(
      EMAIL_ADDRESS, u"theprez@gmail.com", VerificationStatus::kObserved);
  mx_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Guadalajara",
                                              VerificationStatus::kObserved);
  mx_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                              u"Avenida Álvaro Obregón 1234",
                                              VerificationStatus::kObserved);
  mx_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
      u"Entre Calles Tonalá y Monterrey", VerificationStatus::kObserved);
  mx_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"80992",
                                              VerificationStatus::kObserved);

  mx_profile.FinalizeAfterImport();
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {mx_profile});
}

TEST_F(AddressFormDataImporterTest,
       ImportStructuredAddressProfile_I18nAddressFormBR) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("Nome:", "nome", "Pablo Ruiz",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Cidade:", "City", "Brasília",
                           FormControlType::kInputText),
       CreateTestFormField("Provincia:", "provincia", "Goiás",
                           FormControlType::kInputText),
       CreateTestFormField("Country:", "country", "Brasil",
                           FormControlType::kInputText),
       CreateTestFormField("Logradouro", "logradouro", "Avenida Mem de Sá",
                           FormControlType::kInputText),
       CreateTestFormField("Número da residência", "house_number", "1234",
                           FormControlType::kInputText),
       CreateTestFormField("Complemento", "complemento", "Andar 1, apto 12",
                           FormControlType::kInputText),
       CreateTestFormField("Referencia", "referencia", "Referencia example",
                           FormControlType::kInputText),
       CreateTestFormField("Zip:", "zip", "71725",
                           FormControlType::kInputText)});

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form, GeoIpCountryCode("BR"));

  AutofillProfile br_profile(AddressCountryCode("BR"));
  br_profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"Pablo Ruiz",
                                              VerificationStatus::kObserved);
  br_profile.SetRawInfoWithVerificationStatus(
      EMAIL_ADDRESS, u"theprez@gmail.com", VerificationStatus::kObserved);
  br_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Brasília",
                                              VerificationStatus::kObserved);
  br_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"Goiás",
                                              VerificationStatus::kObserved);
  br_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_NAME,
                                              u"Avenida Mem de Sá",
                                              VerificationStatus::kObserved);
  br_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"1234", VerificationStatus::kObserved);
  br_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_OVERFLOW,
                                              u"Andar 1, apto 12",
                                              VerificationStatus::kObserved);
  br_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_LANDMARK,
                                              u"Referencia example",
                                              VerificationStatus::kObserved);
  br_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"71725",
                                              VerificationStatus::kObserved);
  br_profile.FinalizeAfterImport();
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {br_profile});
}

// ImportAddressProfiles tests.
TEST_F(AddressFormDataImporterTest, ImportStructuredNameAddressProfile) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields({

      CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                          FormControlType::kInputText),
      CreateTestFormField("Email:", "email", "theprez@gmail.com",
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
  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure);

  const std::vector<const AutofillProfile*>& results =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Pablo Diego Ruiz y Picasso");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Pablo Diego");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Ruiz y Picasso");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_FIRST), u"Ruiz");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_CONJUNCTION), u"y");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_SECOND), u"Picasso");
}

}  // namespace
}  // namespace autofill
