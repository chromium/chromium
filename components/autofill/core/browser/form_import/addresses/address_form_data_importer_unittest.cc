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
#include "components/autofill/core/browser/autofill_field_test_api.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager_test_api.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
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
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/sync/test/test_sync_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using test::CreateTestFormField;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;

constexpr char kLocale[] = "en_US";

// Define values for various default address profiles.
constexpr char kDefaultFullName[] = "Thomas Neo Anderson";
constexpr char kDefaultFirstName[] = "Thomas";
constexpr char kDefaultLastName[] = "Anderson";
constexpr char kDefaultMail[] = "theone@thematrix.org";
constexpr char kDefaultAddressLine1[] = "21 Laussat St";
constexpr char kDefaultCity[] = "Los Angeles";
constexpr char kDefaultState[] = "California";
constexpr char kDefaultStreetAddress[] = "21 Laussat St\\nApt 123";
constexpr char kDefaultZip[] = "94102";
constexpr char kDefaultCountry[] = "US";
constexpr char kDefaultPhone[] = "+1 650-555-0000";
constexpr char kDefaultPhoneAreaCode[] = "650";
constexpr char kDefaultPhonePrefix[] = "555";
constexpr char kDefaultPhoneSuffix[] = "0000";

constexpr char kSecondPhone[] = "+1 651-666-1111";
constexpr char kSecondPhoneAreaCode[] = "651";
constexpr char kSecondPhonePrefix[] = "666";
constexpr char kSecondPhoneSuffix[] = "1111";

constexpr char kDefaultCreditCardNumber[] = "4111 1111 1111 1111";

constexpr char kDefaultGuid[] = "a21f010a-eac1-41fc-aee9-c06bbedfb292";
constexpr char kSecondGuid[] = "a21f010a-eac1-41fc-aee9-c06bbedfb293";

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

  // Convenience wrapper that calls
  // `FormDataImporter::ExtractFormData()` and subsequently
  // processes the candidates for address profile import. Returns the result of
  // `FormDataImporter::ExtractFormData()`.
  FormDataImporterTestApi::ExtractedFormData
  ExtractFormDataAndProcessAddressCandidates(
      const FormStructure& form,
      bool profile_autofill_enabled,
      bool payment_methods_autofill_enabled) {
    FormDataImporterTestApi::ExtractedFormData extracted_data =
        test_api(form_data_importer())
            .ExtractFormData(form, profile_autofill_enabled,
                             payment_methods_autofill_enabled);
    test_api(form_data_importer().GetAddressFormDataImporter())
        .ProcessExtractedAddressProfiles(
            extracted_data.extracted_address_profiles,
            /*allow_prompt=*/true, ukm_source_id());
    return extracted_data;
  }

  // Convenience wrapper around `ExtractFormDataAndProcessAddressCandidates()`.
  void ExtractFormDataAndProcessAddressCandidates(const FormStructure& form) {
    std::ignore = ExtractFormDataAndProcessAddressCandidates(
        form, /*profile_autofill_enabled=*/true,
        /*payment_methods_autofill_enabled=*/true);
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

  void ImportAddressProfileAndVerifyImportOfNoProfile(
      const FormStructure& form) {
    ExtractAddressProfilesAndVerifyExpectation(form, {});
  }

  TestAddressDataManager& address_data_manager() {
    return autofill_client()
        .GetPersonalDataManager()
        .test_address_data_manager();
  }
  FormDataImporter& form_data_importer() {
    return *autofill_client().GetFormDataImporter();
  }
  TestPaymentsDataManager& payments_data_manager() {
    return autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager();
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

TEST_F(AddressFormDataImporterTest, ImportAddressProfiles) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_F(AddressFormDataImporterTest, ImportSecondAddressProfiles) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSecondProfileFormStructure();
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {ConstructSecondProfile()});
}

TEST_F(AddressFormDataImporterTest, ImportThirdAddressProfiles) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructThirdProfileFormStructure();
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {ConstructThirdProfile()});
}

// Test that with dependent locality parsing enabled, dependent locality fields
// are imported.
TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_DependentLocality) {
  // The Mexican address format contains a dependent locality.
  TypeValuePairs mx_profile =
      GetDefaultProfileTypeValuePairsWithOverriddenCountry("MX");
  mx_profile.emplace_back(ADDRESS_HOME_DEPENDENT_LOCALITY,
                          "Bosques de las Lomas");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(mx_profile);
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructProfileFromTypeValuePairs(mx_profile)});
}

// Test that the storage is prevented if the structured address prompt feature
// is enabled, but address prompts are not allowed.
TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_DontAllowPrompt) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure,
                         /*allow_save_prompts=*/false);
  VerifyExpectationForExtractedAddressProfiles({});
}

TEST_F(AddressFormDataImporterTest, ImportAddressProfileFromUnifiedSection) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();

  // Assign the address field another section than the other fields.
  form_structure->field(4)->set_section(
      Section::FromAutocomplete({.section = "another_section"}));

  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_BadEmail) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();

  // Change the value of the email field.
  ASSERT_THAT(form_structure->field(2)->Type().GetTypes(),
              Contains(EMAIL_ADDRESS));
  form_structure->field(2)->set_value(u"bogus");

  // Verify that there was no import.
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that a 'confirm email' field does not block profile import.
TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_TwoEmails) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {EMAIL_ADDRESS, kDefaultMail},
           // Add two email fields with the same value.
           {EMAIL_ADDRESS, kDefaultMail},
           {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
           {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
           {ADDRESS_HOME_CITY, kDefaultCity},
           {ADDRESS_HOME_STATE, kDefaultState},
           {ADDRESS_HOME_ZIP, kDefaultZip}});

  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests two email fields containing different values blocks profile import.
TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_TwoDifferentEmails) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {EMAIL_ADDRESS, kDefaultMail},
           // Add two email fields with different values.
           {EMAIL_ADDRESS, "another@mail.com"},
           {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
           {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
           {ADDRESS_HOME_CITY, kDefaultCity},
           {ADDRESS_HOME_STATE, kDefaultState},
           {ADDRESS_HOME_ZIP, kDefaultZip}});

  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that multiple phone numbers do not block profile extraction and the
// first one is saved.
TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_MultiplePhoneNumbers) {
  base::test::ScopedFeatureList enable_import_when_multiple_phones_feature;
  enable_import_when_multiple_phones_feature.InitAndEnableFeature(
      features::kAutofillEnableImportWhenMultiplePhoneNumbers);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {EMAIL_ADDRESS, kDefaultMail},
           {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
           // Add a second phone field with a different number.
           {PHONE_HOME_WHOLE_NUMBER, kSecondPhone},
           {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
           {ADDRESS_HOME_CITY, kDefaultCity},
           {ADDRESS_HOME_STATE, kDefaultState},
           {ADDRESS_HOME_ZIP, kDefaultZip}});

  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that multiple phone numbers do not block profile import and the first
// one is saved.
TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_MultiplePhoneNumbersSplitAcrossMultipleFields) {
  base::test::ScopedFeatureList enable_import_when_multiple_phones_feature;
  enable_import_when_multiple_phones_feature.InitAndEnableFeature(
      features::kAutofillEnableImportWhenMultiplePhoneNumbers);

  FormData form_data = ConstructFormDateFromTypeValuePairs(
      {{NAME_FIRST, kDefaultFirstName},
       {NAME_LAST, kDefaultLastName},
       {EMAIL_ADDRESS, kDefaultMail},
       // Add two phone number fields, split across 3 fields each.
       // They are all declared as PHONE_HOME_WHOLE_NUMBER, which only affects
       // the label. Local heuristics will classify them correctly.
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneAreaCode},
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhonePrefix},
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneSuffix},
       {PHONE_HOME_WHOLE_NUMBER, kSecondPhoneAreaCode},
       {PHONE_HOME_WHOLE_NUMBER, kSecondPhonePrefix},
       {PHONE_HOME_WHOLE_NUMBER, kSecondPhoneSuffix},
       {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
       {ADDRESS_HOME_CITY, kDefaultCity},
       {ADDRESS_HOME_STATE, kDefaultState},
       {ADDRESS_HOME_ZIP, kDefaultZip},
       {ADDRESS_HOME_COUNTRY, kDefaultCountry}});

  test_api(form_data).field(3).set_max_length(3);
  test_api(form_data).field(4).set_max_length(3);
  test_api(form_data).field(5).set_max_length(4);
  test_api(form_data).field(6).set_max_length(3);
  test_api(form_data).field(7).set_max_length(3);
  test_api(form_data).field(8).set_max_length(4);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form_data);

  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructProfileFromTypeValuePairs(
                           {{NAME_FIRST, kDefaultFirstName},
                            {NAME_LAST, kDefaultLastName},
                            {EMAIL_ADDRESS, kDefaultMail},
                            {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
                            {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
                            {ADDRESS_HOME_CITY, kDefaultCity},
                            {ADDRESS_HOME_STATE, kDefaultState},
                            {ADDRESS_HOME_ZIP, kDefaultZip},
                            {ADDRESS_HOME_COUNTRY, kDefaultCountry}})});
}

// Tests that not enough filled fields will result in not importing an address.
TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_NotEnoughFilledFields) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {CREDIT_CARD_NUMBER, kDefaultCreditCardNumber}});

  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
  // Also verify that there was no import of a credit card.
  ASSERT_EQ(0U, payments_data_manager().GetCreditCards().size());
}

TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_MinimumAddressUSA) {
  TypeValuePairs type_value_pairs = {
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, "US"},
  };

  AutofillProfile profile =
      ConstructProfileFromTypeValuePairs(type_value_pairs);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {profile});
}

TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_MinimumAddressGB) {
  TypeValuePairs type_value_pairs = {
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, "GB"},
  };

  AutofillProfile profile =
      ConstructProfileFromTypeValuePairs(type_value_pairs);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {profile});
}

TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_MinimumAddressGI) {
  TypeValuePairs type_value_pairs = {
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_COUNTRY, "GI"},
  };

  AutofillProfile profile =
      ConstructProfileFromTypeValuePairs(type_value_pairs);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {profile});
}

TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_PhoneNumberSplitAcrossMultipleFields) {
  FormData form_data = ConstructFormDateFromTypeValuePairs(
      {{NAME_FIRST, kDefaultFirstName},
       {NAME_LAST, kDefaultLastName},
       {EMAIL_ADDRESS, kDefaultMail},
       // Add three phone number fields.
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneAreaCode},
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhonePrefix},
       {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneSuffix},
       {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
       {ADDRESS_HOME_CITY, kDefaultCity},
       {ADDRESS_HOME_STATE, kDefaultState},
       {ADDRESS_HOME_ZIP, kDefaultZip},
       {ADDRESS_HOME_COUNTRY, kDefaultCountry}});

  // Define the length of the phone number fields to allow the parser to
  // identify them as area code, prefix and suffix.
  test_api(form_data).field(3).set_max_length(3);
  test_api(form_data).field(4).set_max_length(3);
  test_api(form_data).field(5).set_max_length(4);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form_data);
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructProfileFromTypeValuePairs(
                           {{NAME_FIRST, kDefaultFirstName},
                            {NAME_LAST, kDefaultLastName},
                            {EMAIL_ADDRESS, kDefaultMail},
                            {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
                            {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
                            {ADDRESS_HOME_CITY, kDefaultCity},
                            {ADDRESS_HOME_STATE, kDefaultState},
                            {ADDRESS_HOME_ZIP, kDefaultZip},
                            {ADDRESS_HOME_COUNTRY, kDefaultCountry}})});
}

// Test that even from unfocusable fields we extract.
TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_UnfocusableFields) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  // Set the Address line field as unfocusable.
  form_structure->field(4)->set_is_focusable(false);
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_MultilineAddress) {
  TypeValuePairs type_value_pairs = {
      {NAME_FULL, kDefaultFullName},
      // This is a multi-line field.
      {ADDRESS_HOME_STREET_ADDRESS, kDefaultStreetAddress},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, "US"},
  };

  AutofillProfile profile =
      ConstructProfileFromTypeValuePairs(type_value_pairs);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {profile});
}

TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_TwoValidProfilesDifferentForms) {
  std::unique_ptr<FormStructure> default_form_structure =
      ConstructDefaultProfileFormStructure();

  AutofillProfile default_profile = ConstructDefaultProfile();
  ExtractAddressProfilesAndVerifyExpectation(*default_form_structure,
                                             {default_profile});

  // Now import a second profile from a different form submission.
  std::unique_ptr<FormStructure> alternative_form_structure =
      ConstructSecondProfileFormStructure();
  AutofillProfile alternative_profile = ConstructSecondProfile();

  // Verify that both profiles have been imported.
  ExtractAddressProfilesAndVerifyExpectation(
      *alternative_form_structure, {alternative_profile, default_profile});
}

TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_TwoValidProfilesSameForm) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructShippingAndBillingFormStructure();
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructDefaultProfile(), ConstructSecondProfile()});
}

TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_OneValidProfileSameForm_PartsHidden) {
  FormData form_data = ConstructDefaultFormData();

  FormData hidden_second_form = form_data;
  for (FormFieldData& field : test_api(hidden_second_form).fields()) {
    // Reset the values and make the field non focusable.
    field.set_value(u"");
    field.set_is_focusable(false);
  }

  // Append the fields of the second form to the first form.
  test_api(form_data).Append(hidden_second_form.fields());

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form_data);
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_MissingInfoInOld) {
  TypeValuePairs initial_type_value_pairs{
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, kDefaultCountry},
      {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
  };
  AutofillProfile initial_profile =
      ConstructProfileFromTypeValuePairs(initial_type_value_pairs);

  std::unique_ptr<FormStructure> initial_form_structure =
      ConstructFormStructureFromTypeValuePairs(initial_type_value_pairs);
  ExtractAddressProfilesAndVerifyExpectation(*initial_form_structure,
                                             {initial_profile});

  // Create a superset that includes a new email address.
  TypeValuePairs superset_type_value_pairs = initial_type_value_pairs;
  superset_type_value_pairs.emplace_back(EMAIL_ADDRESS, kDefaultMail);

  AutofillProfile superset_profile =
      ConstructProfileFromTypeValuePairs(superset_type_value_pairs);

  // Verify that the initial profile and the superset profile are not the
  // same.
  ASSERT_FALSE(initial_profile.Compare(superset_profile) == 0);

  std::unique_ptr<FormStructure> superset_form_structure =
      ConstructFormStructureFromTypeValuePairs(superset_type_value_pairs);
  // Verify that extracting the superset profile will result in an update of
  // the existing profile rather than creating a new one.
  ExtractAddressProfilesAndVerifyExpectation(*superset_form_structure,
                                             {superset_profile});
}

TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_MissingInfoInNew) {
  TypeValuePairs subset_type_value_pairs({
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, kDefaultCountry},
      {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
  });
  // Create a superset that includes a new email address.
  TypeValuePairs superset_type_value_pairs = subset_type_value_pairs;
  superset_type_value_pairs.emplace_back(EMAIL_ADDRESS, kDefaultMail);

  AutofillProfile subset_profile =
      ConstructProfileFromTypeValuePairs(subset_type_value_pairs);
  AutofillProfile superset_profile =
      ConstructProfileFromTypeValuePairs(superset_type_value_pairs);

  // Verify that the subset profile and the superset profile are not the
  // same.
  ASSERT_FALSE(subset_profile.Compare(superset_profile) == 0);

  // First import the superset profile.
  std::unique_ptr<FormStructure> superset_form_structure =
      ConstructFormStructureFromTypeValuePairs(superset_type_value_pairs);
  ExtractAddressProfilesAndVerifyExpectation(*superset_form_structure,
                                             {superset_profile});

  // Than extract the subset profile and verify that the stored profile is still
  // the superset.
  std::unique_ptr<FormStructure> subset_form_structure =
      ConstructFormStructureFromTypeValuePairs(subset_type_value_pairs);
  ExtractAddressProfiles(/*extraction_successful=*/true,
                         *superset_form_structure);
  VerifyExpectationForExtractedAddressProfiles({superset_profile});
}

TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_InsufficientAddress) {
  // This address is missing a state which is required in the US.
  TypeValuePairs type_value_pairs = {
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, "US"},
  };

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  // Verify that no profile is imported.
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that an address can be imported from an Indian address form without
// synthesized field types.
TEST_F(AddressFormDataImporterTest, ImportAddressProfiles_NoSynthesizedTypes) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillUseINAddressModel};
  // The address does not contain synthesized types.
  TypeValuePairs type_value_pairs = {
      {NAME_FULL, "INFirst INSecond"},
      {ADDRESS_HOME_STREET_LOCATION, "12/110, Flat no. 504, Raja Apartments"},
      {ADDRESS_HOME_LANDMARK, "Opp to Ayyappa Swamy temple"},
      {ADDRESS_HOME_DEPENDENT_LOCALITY, "Kondapur"},
      {ADDRESS_HOME_CITY, "Hyderabad"},
      {ADDRESS_HOME_STATE, "Telangana"},
      {ADDRESS_HOME_ZIP, "500084"},
      {ADDRESS_HOME_COUNTRY, "IN"},
  };

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  form_structure->field(1)->SetTypeTo(
      AutofillType(ADDRESS_HOME_STREET_LOCATION),
      AutofillPredictionSource::kHeuristics);
  form_structure->field(2)->SetTypeTo(AutofillType(ADDRESS_HOME_LANDMARK),
                                      AutofillPredictionSource::kHeuristics);
  form_structure->field(3)->SetTypeTo(
      AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY),
      AutofillPredictionSource::kHeuristics);
  // Verify that the profile is imported.
  AutofillProfile in_profile(AddressCountryCode("IN"));
  in_profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"INFirst INSecond",
                                              VerificationStatus::kObserved);
  in_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_LOCATION, u"12/110, Flat no. 504, Raja Apartments",
      VerificationStatus::kObserved);
  in_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_LANDMARK,
                                              u"Opp to Ayyappa Swamy temple",
                                              VerificationStatus::kObserved);
  in_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                              u"Kondapur",
                                              VerificationStatus::kObserved);
  in_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Hyderabad",
                                              VerificationStatus::kObserved);
  in_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"Telangana",
                                              VerificationStatus::kObserved);
  in_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"500084",
                                              VerificationStatus::kObserved);

  in_profile.FinalizeAfterImport();
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {in_profile});
}

// Tests that an address cannot be imported from an Indian address form which
// contains synthesized fields. We don't allow that because the address will
// likely look incomplete when shown to the user.
TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_ContainsSynthesizedTypes) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kAutofillUseINAddressModel}, {});
  // The address contains synthesized types which are not supported during
  // form import.
  ASSERT_TRUE(i18n_model_definition::IsSynthesizedType(
      ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK, AddressCountryCode("IN")));
  TypeValuePairs type_value_pairs = {
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_STREET_LOCATION, "12/110, Flat no. 504, Raja Apartments"},
      // ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK is a synthesized field
      // type.
      {ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK,
       "Kondapur, Opp to Ayyappa Swamy temple"},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, "IN"},
  };

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  form_structure->field(1)->SetTypeTo(
      AutofillType(ADDRESS_HOME_STREET_LOCATION),
      AutofillPredictionSource::kHeuristics);
  form_structure->field(2)->SetTypeTo(
      AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK),
      AutofillPredictionSource::kHeuristics);
  // Verify that no profile is imported.
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that a profile is created for countries with composed names.
TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_CompleteComposedCountryName) {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("First name:", "first_name", "George",
                           FormControlType::kInputText),
       CreateTestFormField("Last name:", "last_name", "Washington",
                           FormControlType::kInputText),
       CreateTestFormField("Email:", "email", "theprez@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Address:", "address1", "No. 43 Bo Aung Gyaw Street",
                           FormControlType::kInputText),
       CreateTestFormField("City:", "city", "Yangon",
                           FormControlType::kInputText),
       CreateTestFormField("Zip:", "zip", "11181", FormControlType::kInputText),
       CreateTestFormField("Country:", "country", "Myanmar [Burma]",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure);

  AutofillProfile expected(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&expected,
                       test::SetProfileInfoOptionsBuilder()
                           .with_first_name("George")
                           .with_last_name("Washington")
                           .with_email("theprez@gmail.com")
                           .with_address1("No. 43 Bo Aung Gyaw Street")
                           .with_city("Yangon")
                           .with_zipcode("11181")
                           .with_country("MM")
                           .Build());
  EXPECT_THAT(address_data_manager().GetProfiles(),
              UnorderedElementsCompareEqual(expected));
}

// TODO(crbug.com/41267680): Create profiles if part of a standalone part of a
// composed country name is present. Currently this is treated as an invalid
// country, which is ignored on import.
TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_IncompleteComposedCountryName) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry(
              "Myanmar"));  // Missing the [Burma] part
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that metrics are correctly recorded when removing setting-inaccessible
// fields.
// Note that this function doesn't test the removal functionality itself. This
// is done in the AutofillProfile unit tests.
TEST_F(AddressFormDataImporterTest, RemoveInaccessibleProfileValuesMetrics) {
  // State is setting-inaccessible in Bermuda. Expect that when importing a
  // Bermudan profile with a state, the state information is removed.
  TypeValuePairs type_value_pairs =
      GetDefaultProfileTypeValuePairsWithOverriddenCountry("BM");
  ASSERT_EQ(type_value_pairs[6].first, ADDRESS_HOME_STATE);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  SetValueForType(type_value_pairs, ADDRESS_HOME_STATE, "");
  base::HistogramTester histogram_tester;
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructProfileFromTypeValuePairs(type_value_pairs)});

  // State was removed. Expect the metrics to behave accordingly.
  const std::string metric =
      "Autofill.ProfileImport.InaccessibleFieldsRemoved.";
  histogram_tester.ExpectUniqueSample(metric + "Total", true, 1);
  histogram_tester.ExpectUniqueSample(
      metric + "ByFieldType",
      autofill_metrics::SettingsVisibleFieldTypeForMetrics::kState, 1);
}

// Tests a 2-page multi-step extraction.
TEST_F(AddressFormDataImporterTest, MultiStepImport) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSplitDefaultProfileFormStructure(/*part=*/1);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  form_structure = ConstructSplitDefaultProfileFormStructure(/*part=*/2);
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that when multi-step complements are enabled, complete profiles those
// import was accepted are added as a multi-step candidate. This enables
// complementing the profile with additional information on further pages.
TEST_F(AddressFormDataImporterTest, MultiStepImport_Complement) {
  // Extract the default profile without an email address.
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, EMAIL_ADDRESS, "");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  // Using `ExtractAddressProfileAndVerifyExtractionOfDefaultProfile()` doesn't
  // suffice, as the multi-step complement candidate is only added in the
  // "ProcessAddressCandidates" step.
  ExtractFormDataAndProcessAddressCandidates(*form_structure);
  VerifyExpectationForExtractedAddressProfiles(
      {ConstructProfileFromTypeValuePairs(type_value_pairs)});

  // Import the email address in a separate form. Without multi-step updates,
  // this information cannot be associated to a profile. The resulting profile
  // is the default one.
  form_structure = ConstructDefaultEmailFormStructure();
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that when an imported profile is modified through external means (e.g.
// via the settings), the multi-step complement candidate is updated accordingly
// and the correct profile update occurs.
TEST_F(AddressFormDataImporterTest, MultiStepImport_Complement_ExternalUpdate) {
  // Extract the default profile without an email address.
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, EMAIL_ADDRESS, "");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractFormDataAndProcessAddressCandidates(*form_structure);
  VerifyExpectationForExtractedAddressProfiles(
      {ConstructProfileFromTypeValuePairs(type_value_pairs)});

  // Update the profile's ZIP through external means.
  AutofillProfile profile = *address_data_manager().GetProfiles()[0];
  profile.SetInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"12345", kLocale,
                                        VerificationStatus::kObserved);
  address_data_manager().UpdateProfile(profile);

  // Expect that the updated profile is complemented with an email address.
  form_structure = ConstructDefaultEmailFormStructure();
  AutofillProfile expected_profile = ConstructDefaultProfile();
  expected_profile.SetInfoWithVerificationStatus(
      ADDRESS_HOME_ZIP, u"12345", kLocale, VerificationStatus::kObserved);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {expected_profile});
}

// Tests that when an imported profile is deleted through external means (e.g.
// via the settings), the multi-step complement candidate is removed and no
// further updates related to it are offered.
TEST_F(AddressFormDataImporterTest, MultiStepImport_Complement_ExternalRemove) {
  // Extract the default profile without an email address.
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, EMAIL_ADDRESS, "");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractFormDataAndProcessAddressCandidates(*form_structure);
  VerifyExpectationForExtractedAddressProfiles(
      {ConstructProfileFromTypeValuePairs(type_value_pairs)});

  // Remove the profile through external means.
  address_data_manager().RemoveProfile(
      address_data_manager().GetProfiles()[0]->guid());

  // Expect that the removed profile cannot be updated with an email address.
  form_structure = ConstructDefaultEmailFormStructure();
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that multi-step candidate profiles from different origins are not
// merged.
TEST_F(AddressFormDataImporterTest, MultiStepImport_DifferentOrigin) {
  FormData form = ConstructSplitDefaultFormData(/*part=*/1);
  form.set_url(GURL("https://www.foo.com"));
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  form = ConstructSplitDefaultFormData(/*part=*/2);
  form.set_url(GURL("https://wwww.bar.com"));
  form_structure = ConstructFormStructureFromFormData(form);
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that multi-step candidates profiles are invalidated after some TTL.
TEST_F(AddressFormDataImporterTest, MultiStepImport_TTL) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSplitDefaultProfileFormStructure(/*part=*/1);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  task_environment().FastForwardBy(kMultiStepImportTTL + base::Minutes(1));

  form_structure = ConstructSplitDefaultProfileFormStructure(/*part=*/2);
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

TEST_F(AddressFormDataImporterTest, ExtractGUIDsOfProfilesWithoutManualEdits) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  int counter = 0;
  for (auto& field : *form_structure) {
    field->set_autofill_source_profile_guid(counter % 2 ? kDefaultGuid
                                                        : kSecondGuid);
    ++counter;
  }
  base::flat_set<std::string> guids =
      test_api(form_data_importer().GetAddressFormDataImporter())
          .ExtractGUIDsOfProfilesWithoutManualEdits(*form_structure);
  EXPECT_THAT(guids, UnorderedElementsAre(kDefaultGuid, kSecondGuid));
}

TEST_F(AddressFormDataImporterTest,
       ExtractGUIDsOfProfilesWithoutManualEdits_FieldWasEdited) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  int counter = 0;
  for (auto& field : *form_structure) {
    field->set_autofill_source_profile_guid(counter % 2 ? kDefaultGuid
                                                        : kSecondGuid);
    ++counter;
  }
  form_structure->field(0)->AddFieldModifier(FieldModifier::kUser);
  base::flat_set<std::string> guids =
      test_api(form_data_importer().GetAddressFormDataImporter())
          .ExtractGUIDsOfProfilesWithoutManualEdits(*form_structure);
  EXPECT_THAT(guids, IsEmpty());
}

TEST_F(AddressFormDataImporterTest,
       ImportAddressProfiles_PrefilledStateAndCountry_Imported) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableImportOfUnchangedValuesForCountryAndState};

  // Create a form with a prefilled state and country.
  FormData form = ConstructFormDateFromTypeValuePairs({
      {NAME_FULL, "Pablo Diego Ruiz y Picasso"},
      {EMAIL_ADDRESS, "theprez@gmail.com"},
      {ADDRESS_HOME_LINE1, "21 Laussat St"},
      {ADDRESS_HOME_CITY, "San Francisco"},
      {ADDRESS_HOME_STATE, "California"},
      {ADDRESS_HOME_ZIP, "94102"},
      {ADDRESS_HOME_COUNTRY, "United States"},
  });

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);

  // ConstructFormStructureFromFormData resets initial_value to an empty string.
  // Set the fields back to simulate a prefilled field.
  test_api(*form_structure->field(4)).set_initial_value(u"California");
  test_api(*form_structure->field(6)).set_initial_value(u"United States");

  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure);

  const std::vector<const AutofillProfile*>& results =
      address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STATE), u"California");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
}

// Tests that duplicate fields with identical field values are valid. They would
// thus not abandon the import of the address.
TEST_F(AddressFormDataImporterTest,
       DuplicateFieldsWithIdenticalValuesAreValid) {
  AutofillField field;
  field.SetTypeTo(AutofillType(NAME_FIRST),
                  AutofillPredictionSource::kHeuristics);
  field.set_value(u"First");
  AutofillField field2;
  field2.SetTypeTo(AutofillType(NAME_FIRST),
                   AutofillPredictionSource::kHeuristics);
  field2.set_value(u"First");
  EXPECT_FALSE(test_api(form_data_importer().GetAddressFormDataImporter())
                   .HasInvalidFieldTypes(
                       std::to_array<const AutofillField*>({&field, &field2})));
}

// Tests that duplicate fields with different field values are invalid. They
// would thus abandon the import of the address.
TEST_F(AddressFormDataImporterTest,
       DuplicateFieldsWithDifferentValuesAreInvalid) {
  AutofillField field;
  field.SetTypeTo(AutofillType(NAME_FIRST),
                  AutofillPredictionSource::kHeuristics);
  field.set_value(u"First");
  AutofillField field2;
  field2.SetTypeTo(AutofillType(NAME_FIRST),
                   AutofillPredictionSource::kHeuristics);
  field2.set_value(u"Other value");
  EXPECT_TRUE(test_api(form_data_importer().GetAddressFormDataImporter())
                  .HasInvalidFieldTypes(
                      std::to_array<const AutofillField*>({&field, &field2})));
}

// Tests that duplicate fields with identical field values are valid for the
// case where a <select> field follows an <input> field and the input field's
// value is the selected option's value. They would thus not abandon the import
// of the address.
TEST_F(AddressFormDataImporterTest,
       InputFollowedBySelectWithIdenticalValuesAreValid) {
  AutofillField field;
  field.SetTypeTo(AutofillType(ADDRESS_HOME_COUNTRY),
                  AutofillPredictionSource::kHeuristics);
  field.set_value(u"US");
  AutofillField field2(
      test::CreateTestSelectField("Country", "country", "US", "country",
                                  {"DE", "US"}, {"Germany", "United States"}));
  field2.SetTypeTo(AutofillType(ADDRESS_HOME_COUNTRY),
                   AutofillPredictionSource::kHeuristics);
  const std::array<const autofill::AutofillField*, 2> section_fields =
      std::to_array<const AutofillField*>({&field, &field2});

  EXPECT_FALSE(test_api(form_data_importer().GetAddressFormDataImporter())
                   .HasInvalidFieldTypes(section_fields));
  EXPECT_THAT(
      test_api(form_data_importer().GetAddressFormDataImporter())
          .GetObservedFieldValues(section_fields),
      ElementsAre(Pair(Eq(ADDRESS_HOME_COUNTRY), Eq(u"United States"))));
}

// Tests that duplicate fields with identical field values are valid for the
// case where a <select> field is followed by an <input> field and the input
// field's value is the selected option's value. They would thus not abandon the
// import of the address.
TEST_F(AddressFormDataImporterTest,
       SelectFollowedByInputWithIdenticalValuesAreValid) {
  AutofillField field(
      test::CreateTestSelectField("Country", "country", "US", "country",
                                  {"DE", "US"}, {"Germany", "United States"}));
  field.SetTypeTo(AutofillType(ADDRESS_HOME_COUNTRY),
                  AutofillPredictionSource::kHeuristics);
  AutofillField field2;
  field2.SetTypeTo(AutofillType(ADDRESS_HOME_COUNTRY),
                   AutofillPredictionSource::kHeuristics);
  field2.set_value(u"US");
  const std::array<const autofill::AutofillField*, 2> section_fields =
      std::to_array<const AutofillField*>({&field, &field2});

  EXPECT_FALSE(test_api(form_data_importer().GetAddressFormDataImporter())
                   .HasInvalidFieldTypes(section_fields));
  EXPECT_THAT(
      test_api(form_data_importer().GetAddressFormDataImporter())
          .GetObservedFieldValues(section_fields),
      ElementsAre(Pair(Eq(ADDRESS_HOME_COUNTRY), Eq(u"United States"))));
}

}  // namespace
}  // namespace autofill
