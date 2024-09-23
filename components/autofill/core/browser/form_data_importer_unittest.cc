// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_data_importer.h"

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
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/address_data_manager_test_api.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/mock_autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/test_autofill_client.h"
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

using base::UTF8ToUTF16;
using test::CreateTestFormField;
using test::CreateTestIbanFormData;
using ::testing::_;
using ::testing::NiceMock;

constexpr char kLocale[] = "en_US";

// Define values for the default address profile.
constexpr char kDefaultFullName[] = "Thomas Neo Anderson";
constexpr char kDefaultFirstName[] = "Thomas";
constexpr char kDefaultLastName[] = "Anderson";
constexpr char kDefaultMail[] = "theone@thematrix.org";
constexpr char kDefaultAddressLine1[] = "21 Laussat St";
constexpr char kDefaultStreetAddress[] = "21 Laussat St\\nApt 123";
constexpr char kDefaultZip[] = "94102";
constexpr char kDefaultCity[] = "Los Angeles";
constexpr char kDefaultState[] = "California";
constexpr char kDefaultCountry[] = "US";
// Unlike phone numbers from other countries, US phone numbers are stored
// without a leading "+". Formatting a US or CA phone number drops the leading
// "+". As these tests check equality, we drop the "+" in the input as it would
// be gone in the output.
constexpr char kDefaultPhone[] = "1 650-555-0000";
constexpr char kDefaultPhoneDomesticFormatting[] = "(650) 555-0000";
constexpr char kDefaultPhoneAreaCode[] = "650";
constexpr char kDefaultPhonePrefix[] = "555";
constexpr char kDefaultPhoneSuffix[] = "0000";

// Define values for a second address profile.
constexpr char kSecondFirstName[] = "Bruce";
constexpr char kSecondLastName[] = "Wayne";
constexpr char kSecondMail[] = "wayne@bruce.org";
constexpr char kSecondAddressLine1[] = "23 Main St";
constexpr char kSecondZip[] = "94106";
constexpr char kSecondCity[] = "Los Angeles";
constexpr char kSecondState[] = "California";
constexpr char kSecondPhone[] = "1 651-666-1111";
constexpr char kSecondPhoneAreaCode[] = "651";
constexpr char kSecondPhonePrefix[] = "666";
constexpr char kSecondPhoneSuffix[] = "1111";

// Define values for a third address profile.
constexpr char kThirdFirstName[] = "Homer";
constexpr char kThirdLastName[] = "Simpson";
constexpr char kThirdMail[] = "donut@whatever.net";
constexpr char kThirdAddressLine1[] = "742 Evergreen Terrace";
constexpr char kThirdZip[] = "65619";
constexpr char kThirdCity[] = "Springfield";
constexpr char kThirdState[] = "Oregon";
constexpr char kThirdPhone[] = "1 850-777-2222";

constexpr char kDefaultCreditCardName[] = "Biggie Smalls";
constexpr char kDefaultCreditCardNumber[] = "4111 1111 1111 1111";
constexpr char kDefaultCreditCardExpMonth[] = "01";
constexpr char kDefaultCreditCardExpYear[] = "2999";

constexpr char kDefaultPhoneGermany[] = "+49 89 123456";
constexpr char kDefaultPhoneMexico[] = "+52 55 1234 5678";
constexpr char kDefaultPhoneArmenia[] = "+374 10 123456";

// For a given FieldType |type| returns a pair of field name and label
// that should be parsed into this type by our field type parsers.
std::pair<std::string, std::string> GetLabelAndNameForType(FieldType type) {
  static const std::map<FieldType, std::pair<std::string, std::string>>
      name_type_map = {
          {NAME_FULL, {"Full Name:", "full_name"}},
          {NAME_FIRST, {"First Name:", "first_name"}},
          {NAME_MIDDLE, {"Middle Name", "middle_name"}},
          {NAME_LAST, {"Last Name:", "last_name"}},
          {EMAIL_ADDRESS, {"Email:", "email"}},
          {ADDRESS_HOME_LINE1, {"Address:", "address1"}},
          {ADDRESS_HOME_STREET_ADDRESS, {"Address:", "address"}},
          {ADDRESS_HOME_CITY, {"City:", "city"}},
          {ADDRESS_HOME_ZIP, {"Zip:", "zip"}},
          {ADDRESS_HOME_STATE, {"State:", "state"}},
          {ADDRESS_HOME_DEPENDENT_LOCALITY, {"Neighborhood:", "neighborhood"}},
          {ADDRESS_HOME_COUNTRY, {"Country:", "country"}},
          {PHONE_HOME_WHOLE_NUMBER, {"Phone:", "phone"}},
          {CREDIT_CARD_NAME_FULL, {"Name on card:", "name_on_card"}},
          {CREDIT_CARD_NUMBER, {"Credit Card Number:", "card_number"}},
          {CREDIT_CARD_EXP_MONTH, {"Exp Month:", "exp_month"}},
          {CREDIT_CARD_EXP_4_DIGIT_YEAR, {"Exp Year:", "exp_year"}},
      };
  auto it = name_type_map.find(type);
  if (it == name_type_map.end()) {
    NOTIMPLEMENTED() << " field name and label is missing for "
                     << FieldTypeToStringView(type);
    return {std::string(), std::string()};
  }
  return it->second;
}

using TypeValuePairs = std::vector<std::pair<FieldType, std::string>>;

// Constructs a FormData instance for |url| from a vector of type value pairs
// that defines a sequence of fields and the filled values.
// The field names and labels for the different types are relieved from
// |GetLabelAndNameForType(type)|
FormData ConstructFormDateFromTypeValuePairs(
    TypeValuePairs type_value_pairs,
    std::string url = "https://www.foo.com") {
  FormData form;
  form.set_url(GURL(url));

  for (const auto& [type, value] : type_value_pairs) {
    const auto& [name, label] = GetLabelAndNameForType(type);
    test_api(form).Append(CreateTestFormField(
        name, label, value,
        type == ADDRESS_HOME_STREET_ADDRESS ? FormControlType::kTextArea
                                            : FormControlType::kInputText));
  }

  return form;
}

// Fakes that a `form` has been seen (without its field value) and parsed and
// then values have been entered. Returns the resulting FormStructure.
std::unique_ptr<FormStructure> ConstructFormStructureFromFormData(
    const FormData& form,
    GeoIpCountryCode geo_country = GeoIpCountryCode("")) {
  auto cached_form_structure =
      std::make_unique<FormStructure>(test::WithoutValues(form));
  cached_form_structure->DetermineHeuristicTypes(geo_country, nullptr, nullptr);

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->RetrieveFromCache(
      *cached_form_structure,
      FormStructure::RetrieveFromCacheReason::kFormImport);
  return form_structure;
}

// Constructs a FormStructure instance with fields and inserted values given by
// a vector of type and value pairs.
std::unique_ptr<FormStructure> ConstructFormStructureFromTypeValuePairs(
    TypeValuePairs type_value_pairs,
    std::string url = "https://www.foo.com") {
  FormData form = ConstructFormDateFromTypeValuePairs(type_value_pairs, url);
  return ConstructFormStructureFromFormData(form);
}

// Construct and finalizes an AutofillProfile based on a vector of type and
// value pairs. The values are set as |VerificationStatus::kObserved| and the
// profile is finalizes in the end.
AutofillProfile ConstructProfileFromTypeValuePairs(
    TypeValuePairs type_value_pairs) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  for (const auto& [type, value] : type_value_pairs) {
    profile.SetRawInfoWithVerificationStatus(type, base::UTF8ToUTF16(value),
                                             VerificationStatus::kObserved);
  }
  if (!profile.FinalizeAfterImport())
    NOTREACHED_IN_MIGRATION();
  return profile;
}

// Returns a vector of FieldType and value pairs used to construct the
// default AutofillProfile, or a FormStructure or FormData instance that carries
// that corresponding information.
TypeValuePairs GetDefaultProfileTypeValuePairs() {
  return {
      {NAME_FIRST, kDefaultFirstName},
      {NAME_LAST, kDefaultLastName},
      {EMAIL_ADDRESS, kDefaultMail},
      {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, kDefaultCountry},
  };
}

// Sets the value of `type` in `pairs` to `value`. If the `value` is empty, the
// `type` is removed entirely.
void SetValueForType(TypeValuePairs& pairs,
                     FieldType type,
                     const std::string& value) {
  auto it = base::ranges::find(pairs, type,
                               [](const auto& pair) { return pair.first; });
  CHECK(it != pairs.end(), base::NotFatalUntil::M130);
  if (value.empty())
    pairs.erase(it);
  else
    it->second = value;
}

// Wraps `GetDefaultProfileTypeValuePairs()` but replaces `kDefaultCountry` with
// `country`. If `country` is empty, ADDRESS_HOME_COUNTRY is removed entirely.
TypeValuePairs GetDefaultProfileTypeValuePairsWithOverriddenCountry(
    const std::string& country) {
  auto pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(pairs, ADDRESS_HOME_COUNTRY, country);
  if (country == "DE" || country == "Germany") {
    SetValueForType(pairs, PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneGermany);
  } else if (country == "MX" || country == "Mexico") {
    SetValueForType(pairs, PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneMexico);
  } else if (country == "AM" || country == "Armenien") {
    SetValueForType(pairs, PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneArmenia);
  }
  return pairs;
}

// Same as |GetDefaultProfileTypeValuePairs()|, but split into two parts to test
// multi-step imports. No part by itself satisfies the import requirements.
// |part| specifies the requested half and can be either 1 or 2.
TypeValuePairs GetSplitDefaultProfileTypeValuePairs(int part) {
  DCHECK(part == 1 || part == 2);
  if (part == 1) {
    return {
        {NAME_FIRST, kDefaultFirstName},
        {NAME_LAST, kDefaultLastName},
        {EMAIL_ADDRESS, kDefaultMail},
        {ADDRESS_HOME_CITY, kDefaultCity},
        {ADDRESS_HOME_STATE, kDefaultState},
        {ADDRESS_HOME_COUNTRY, kDefaultCountry},
    };
  } else {
    return {
        {PHONE_HOME_WHOLE_NUMBER, kDefaultPhone},
        {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
        {ADDRESS_HOME_ZIP, kDefaultZip},
    };
  }
}

// Same as |GetDefaultProfileTypeValuePairs()| but with the second profile
// information.
TypeValuePairs GetSecondProfileTypeValuePairs() {
  return {
      {NAME_FIRST, kSecondFirstName},
      {NAME_LAST, kSecondLastName},
      {EMAIL_ADDRESS, kSecondMail},
      {PHONE_HOME_WHOLE_NUMBER, kSecondPhone},
      {ADDRESS_HOME_LINE1, kSecondAddressLine1},
      {ADDRESS_HOME_CITY, kSecondCity},
      {ADDRESS_HOME_STATE, kSecondState},
      {ADDRESS_HOME_ZIP, kSecondZip},
      {ADDRESS_HOME_COUNTRY, kDefaultCountry},
  };
}

// Same as |GetDefaultProfileTypeValuePairs()| but with the third profile
// information.
TypeValuePairs GetThirdProfileTypeValuePairs() {
  return {
      {NAME_FIRST, kThirdFirstName},
      {NAME_LAST, kThirdLastName},
      {EMAIL_ADDRESS, kThirdMail},
      {PHONE_HOME_WHOLE_NUMBER, kThirdPhone},
      {ADDRESS_HOME_LINE1, kThirdAddressLine1},
      {ADDRESS_HOME_CITY, kThirdCity},
      {ADDRESS_HOME_STATE, kThirdState},
      {ADDRESS_HOME_ZIP, kThirdZip},
      {ADDRESS_HOME_COUNTRY, kDefaultCountry},
  };
}

// Same as `GetDefaultProfileTypeValuePairs()`, but for credit cards.
TypeValuePairs GetDefaultCreditCardTypeValuePairs() {
  return {
      {CREDIT_CARD_NAME_FULL, kDefaultCreditCardName},
      {CREDIT_CARD_NUMBER, kDefaultCreditCardNumber},
      {CREDIT_CARD_EXP_MONTH, kDefaultCreditCardExpMonth},
      {CREDIT_CARD_EXP_4_DIGIT_YEAR, kDefaultCreditCardExpYear},
  };
}

// Returns the default AutofillProfile used in this test file.
AutofillProfile ConstructDefaultProfile() {
  return ConstructProfileFromTypeValuePairs(GetDefaultProfileTypeValuePairs());
}

// Wraps `ConstructDefaultProfile()`, but overrides ADDRESS_HOME_COUNTRY with
// `country`.
AutofillProfile ConstructDefaultProfileWithOverriddenCountry(
    const std::string& country) {
  return ConstructProfileFromTypeValuePairs(
      GetDefaultProfileTypeValuePairsWithOverriddenCountry(country));
}

// Returns the second AutofillProfile used in this test file.
AutofillProfile ConstructSecondProfile() {
  return ConstructProfileFromTypeValuePairs(GetSecondProfileTypeValuePairs());
}

// Returns the third AutofillProfile used in this test file.
AutofillProfile ConstructThirdProfile() {
  return ConstructProfileFromTypeValuePairs(GetThirdProfileTypeValuePairs());
}

// Returns a form with the default profile. The AutofillProfile that is imported
// from this form should be similar to the profile create by calling
// `ConstructDefaultProfile()`.
std::unique_ptr<FormStructure> ConstructDefaultProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetDefaultProfileTypeValuePairs());
}

// Constructs a form structure containing only an email field, set to
// `kDefaultMail`. This is useful for testing multi-step complements.
std::unique_ptr<FormStructure> ConstructDefaultEmailFormStructure() {
  // The autocomplete attribute is set manually, because for small forms (number
  // of fields < kMinRequiredFieldsForHeuristics), no heuristics are used.
  FormData form =
      ConstructFormDateFromTypeValuePairs({{EMAIL_ADDRESS, kDefaultMail}});
  const char* autocomplete = "email";
  test_api(form).field(0).set_autocomplete_attribute(autocomplete);
  test_api(form).field(0).set_parsed_autocomplete(
      ParseAutocompleteAttribute(autocomplete));
  return ConstructFormStructureFromFormData(form);
}

// Same as `ConstructDefaultFormStructure()` but split into two parts to test
// multi-step imports (see `GetSplitDefaultProfileTypeValuePairs()`).
std::unique_ptr<FormStructure> ConstructSplitDefaultProfileFormStructure(
    int part) {
  return ConstructFormStructureFromTypeValuePairs(
      GetSplitDefaultProfileTypeValuePairs(part));
}

// Same as `ConstructDefaultFormStructure()` but for the second profile.
std::unique_ptr<FormStructure> ConstructSecondProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetSecondProfileTypeValuePairs());
}

// Same as `ConstructDefaultFormStructure()` but for the third profile.
std::unique_ptr<FormStructure> ConstructThirdProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetThirdProfileTypeValuePairs());
}

// Constructs a FormStructure with two address sections by concatenating
// the default profile and second profile form structures.
std::unique_ptr<FormStructure> ConstructShippingAndBillingFormStructure() {
  TypeValuePairs a = GetDefaultProfileTypeValuePairs();
  TypeValuePairs b = GetSecondProfileTypeValuePairs();
  a.reserve(a.size() + b.size());
  base::ranges::move(b, std::back_inserter(a));
  return ConstructFormStructureFromTypeValuePairs(a);
}

// Same as `ConstructDefaultFormStructure()` but for credit cards.
std::unique_ptr<FormStructure> ConstructDefaultCreditCardFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetDefaultCreditCardTypeValuePairs());
}

// Constructs a |FormData| instance that carries the information of the default
// profile.
FormData ConstructDefaultFormData() {
  return ConstructFormDateFromTypeValuePairs(GetDefaultProfileTypeValuePairs());
}

// Constructs a |FormData| instance with two address sections by concatenating
// the default profile and second profile.
FormData ConstructDefaultFormDataWithTwoAddresses() {
  TypeValuePairs a = GetDefaultProfileTypeValuePairs();
  TypeValuePairs b = GetSecondProfileTypeValuePairs();
  a.reserve(a.size() + b.size());
  base::ranges::move(b, std::back_inserter(a));
  return ConstructFormDateFromTypeValuePairs(a);
}

// Same as |ConstructDefaultFormData()| but split into two parts to test multi-
// step imports (see |GetSplitDefaultProfileTypeValuePairs()|).
FormData ConstructSplitDefaultFormData(int part) {
  return ConstructFormDateFromTypeValuePairs(
      GetSplitDefaultProfileTypeValuePairs(part));
}

// Matches an AddressProfile or CreditCard pointer according to Compare().
// Takes `expected` by value to avoid a dangling reference.
template <typename T>
auto ComparesEqual(T expected) {
  return ::testing::Truly([expected = std::move(expected)](const T& actual) {
    return actual.Compare(expected) == 0;
  });
}

// The below matchers follow ::testing::UnorderedElementsAre[Array] except that
// they accept AutofillProfile or CreditCard *pointers* and compare their
// pointees using ComparesEqual().

template <typename T>
auto UnorderedElementsCompareEqualArray(const std::vector<T>& expected_values) {
  std::vector<::testing::Matcher<const T*>> matchers;
  for (const T& expected : expected_values)
    matchers.push_back(::testing::Pointee(ComparesEqual(expected)));
  return ::testing::UnorderedElementsAreArray(matchers);
}

template <typename... Matchers>
auto UnorderedElementsCompareEqual(Matchers... matchers) {
  return ::testing::UnorderedElementsAre(
      ::testing::Pointee(ComparesEqual(std::move(matchers)))...);
}

class MockVirtualCardEnrollmentManager
    : public TestVirtualCardEnrollmentManager {
 public:
  MockVirtualCardEnrollmentManager(
      TestPersonalDataManager* personal_data_manager,
      payments::TestPaymentsNetworkInterface* payments_network_interface,
      TestAutofillClient* autofill_client)
      : TestVirtualCardEnrollmentManager(personal_data_manager,
                                         payments_network_interface,
                                         autofill_client) {}
  MOCK_METHOD(
      void,
      InitVirtualCardEnroll,
      (const CreditCard& credit_card,
       VirtualCardEnrollmentSource virtual_card_enrollment_source,
       std::optional<payments::PaymentsNetworkInterface::
                         GetDetailsForEnrollmentResponseDetails>
           get_details_for_enrollment_response_details,
       PrefService* user_prefs,
       VirtualCardEnrollmentManager::RiskAssessmentFunction
           risk_assessment_function,
       VirtualCardEnrollmentManager::VirtualCardEnrollmentFieldsLoadedCallback
           virtual_card_enrollment_fields_loaded_callback),
      (override));
};

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
               FormDataImporter::CreditCardImportType credit_card_import_type,
               bool is_credit_card_upstream_enabled),
              (override));
};

class FormDataImporterTest : public testing::Test {
 public:
  FormDataImporterTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAutofillUseAUAddressModel,
         features::kAutofillUseCAAddressModel,
         features::kAutofillUseDEAddressModel,
         features::kAutofillUseFRAddressModel,
         features::kAutofillUseINAddressModel,
         features::kAutofillUseITAddressModel},
        {});

    // Advance the clock to year 20XX.
    task_environment_.FastForwardBy(base::Days(365) * 31);
  }

  void SetUp() override {
    prefs_ = test::PrefServiceForTesting();

    autofill_client_ = std::make_unique<TestAutofillClient>();
    autofill_client_->set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());

    personal_data_manager_ = autofill_client_->GetPersonalDataManager();
    test_api(personal_data_manager_->address_data_manager())
        .set_auto_accept_address_imports(true);
    personal_data_manager_->SetPrefService(prefs_.get());
    personal_data_manager_->SetSyncServiceForTest(&sync_service_);

    auto virtual_card_enrollment_manager =
        std::make_unique<MockVirtualCardEnrollmentManager>(
            nullptr, nullptr, autofill_client_.get());
    autofill_client_->GetPaymentsAutofillClient()
        ->set_virtual_card_enrollment_manager(
            std::move(virtual_card_enrollment_manager));
    auto credit_card_save_manager =
        std::make_unique<MockCreditCardSaveManager>(autofill_client_.get());
    test_api(form_data_importer())
        .set_credit_card_save_manager(std::move(credit_card_save_manager));
  }

  FormDataImporter& form_data_importer() {
    return *autofill_client_->GetFormDataImporter();
  }

  // Helper method that will add credit card fields in `form`, according to the
  // specified values. If a value is nullptr, the corresponding field won't get
  // added (empty string will add a field with an empty string as the value).
  void AddFullCreditCardForm(FormData* form,
                             const char* name,
                             const char* number,
                             const char* month,
                             const char* year) {
    std::vector<FormFieldData>& fields = test_api(*form).fields();
    if (name) {
      fields.push_back(CreateTestFormField("Name on card:", "name_on_card",
                                           name, FormControlType::kInputText));
    }
    if (number) {
      fields.push_back(CreateTestFormField(
          "Card Number:", "card_number", number, FormControlType::kInputText));
    }
    if (month) {
      fields.push_back(CreateTestFormField("Exp Month:", "exp_month", month,
                                           FormControlType::kInputText));
    }
    if (year) {
      fields.push_back(CreateTestFormField("Exp Year:", "exp_year", year,
                                           FormControlType::kInputText));
    }
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

  // Helper methods that simply forward the call to the private member (to avoid
  // having to friend every test that needs to access the private
  // PersonalDataManager::ImportAddressProfile or ExtractCreditCard).
  void ExtractAddressProfiles(bool extraction_successful,
                              const FormStructure& form,
                              bool allow_save_prompts = true) {
    std::vector<FormDataImporterTestApi::AddressProfileImportCandidate>
        address_profile_import_candidates;

    EXPECT_EQ(extraction_successful,
              test_api(form_data_importer())
                      .ExtractAddressProfiles(
                          form, &address_profile_import_candidates) > 0);

    if (!extraction_successful) {
      EXPECT_FALSE(
          test_api(form_data_importer())
              .ProcessAddressProfileImportCandidates(
                  address_profile_import_candidates, allow_save_prompts));
      return;
    }

    EXPECT_EQ(test_api(form_data_importer())
                  .ProcessAddressProfileImportCandidates(
                      address_profile_import_candidates, allow_save_prompts),
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
      for (const AutofillProfile* p :
           personal_data_manager_->address_data_manager().GetProfiles()) {
        output << *p << std::endl;
      }
      return output.str();
    };
    EXPECT_THAT(personal_data_manager_->address_data_manager().GetProfiles(),
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
    test_api(form_data_importer())
        .ProcessAddressProfileImportCandidates(
            extracted_data.address_profile_import_candidates);
    return extracted_data;
  }

  // Convenience wrapper around `ExtractFormDataAndProcessAddressCandidates()`.
  void ExtractFormDataAndProcessAddressCandidates(const FormStructure& form) {
    std::ignore = ExtractFormDataAndProcessAddressCandidates(
        form, /*profile_autofill_enabled=*/true,
        /*payment_methods_autofill_enabled=*/true);
  }

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
           form_data_importer().ProcessIbanImportCandidate(
               extracted_data.extracted_iban.value());
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

  std::optional<CreditCard> ExtractCreditCard(const FormStructure& form) {
    return test_api(form_data_importer()).ExtractCreditCard(form);
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
    personal_data_manager_->payments_data_manager()
        .OnAcceptedLocalCreditCardSave(*extracted_credit_card);

    CreditCard expected = test::CreateCreditCardWithInfo(
        exp_name, exp_cc_num, exp_cc_month, exp_cc_year, "");
    EXPECT_THAT(
        personal_data_manager_->payments_data_manager().GetCreditCards(),
        UnorderedElementsCompareEqual(expected));
  }

  MockVirtualCardEnrollmentManager& virtual_card_enrollment_manager() {
    return *static_cast<MockVirtualCardEnrollmentManager*>(
        autofill_client_->GetPaymentsAutofillClient()
            ->GetVirtualCardEnrollmentManager());
  }

  MockCreditCardSaveManager& credit_card_save_manager() {
    return *static_cast<MockCreditCardSaveManager*>(
        form_data_importer().GetCreditCardSaveManager());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<PrefService> prefs_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  // Owned by `autofill_client_`.
  raw_ptr<TestPersonalDataManager> personal_data_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the country is not complemented if a country is part of the form.
TEST_F(FormDataImporterTest, ComplementCountry_PartOfForm) {
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
TEST_F(FormDataImporterTest, ComplementCountry_VariationCountryCode) {
  AutofillProfile kDefaultGermanProfile =
      ConstructDefaultProfileWithOverriddenCountry("DE");

  autofill_client_->SetVariationConfigCountryCode(GeoIpCountryCode("DE"));

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
TEST_F(FormDataImporterTest, ComplementCountry_VariationConfigCountryCode) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry(""));
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructDefaultProfileWithOverriddenCountry("US")});
}

// Tests that the country is complemented before parsing the phone number. This
// is important, since the phone number validation relies on the profile's
// country for nationally formatted numbers.
TEST_F(FormDataImporterTest, ComplementCountry_PhoneNumberParsing) {
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
  autofill_client_->SetVariationConfigCountryCode(GeoIpCountryCode("DE"));

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

// This test verifies that a phone number is stored correctly in the following
// situation: A form contains a telephone number field that is classified as
// a PHONE_HOME_CITY_AND_NUMBER field (either due to heuristics or due to
// crowdsourcing). If a user enters an international phone number (e.g. +374 10
// 123456), this must be parsed as such, not as a local number in the assumed
// country. Otherwise, the stored value is incorrect. Before a fix, the
// number quoted above would be stored as "(010) 123456" for a DE address
// profile and not stored at all for a US address profile.
TEST_F(FormDataImporterTest, ParseI18nPhoneNumberInCityAndNumberField) {
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
  ASSERT_EQ(personal_data_manager_->address_data_manager().GetProfiles().size(),
            1u);
  EXPECT_EQ(base::UTF8ToUTF16(kInternationalNumber),
            personal_data_manager_->address_data_manager()
                .GetProfiles()[0]
                ->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
}

// Tests that invalid countries in submitted forms are ignored, and that the
// complement country logic overwrites it. In this case, expect the country to
// default to the locale's country "US".
TEST_F(FormDataImporterTest, InvalidCountry) {
  // Due to the extra 'A', the country of this `form_structure` is invalid.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry("USAA"));
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that invalid phone numbers are removed and importing continues.
TEST_F(FormDataImporterTest, InvalidPhoneNumber) {
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, PHONE_HOME_WHOLE_NUMBER, "invalid");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);

  auto profile_without_number = ConstructDefaultProfile();
  profile_without_number.ClearFields({PHONE_HOME_WHOLE_NUMBER});
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {profile_without_number});
}

TEST_F(FormDataImporterTest, PlusAddressesExcluded) {
  const std::string kDummyPlusAddress = "plus+plus@plus.plus";

  // Save `kDummyPlusAddress` into the `plus_address_service`, and configure the
  // `autofill_client_` to use it.
  auto plus_address_delegate =
      std::make_unique<NiceMock<MockAutofillPlusAddressDelegate>>();
  ON_CALL(*plus_address_delegate, IsPlusAddress)
      .WillByDefault([&kDummyPlusAddress](const std::string& address) {
        return address == kDummyPlusAddress;
      });
  autofill_client_->set_plus_address_delegate(std::move(plus_address_delegate));

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

// ImportAddressProfiles tests.
TEST_F(FormDataImporterTest, ImportStructuredNameProfile) {
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
      personal_data_manager_->address_data_manager().GetProfiles();
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

TEST_F(FormDataImporterTest,
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
      personal_data_manager_->address_data_manager().GetProfiles();
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
    FormDataImporterTest,
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
      personal_data_manager_->address_data_manager().GetProfiles();
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

TEST_F(FormDataImporterTest,
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
      personal_data_manager_->address_data_manager().GetProfiles();
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

TEST_F(FormDataImporterTest, ImportStructuredAddressProfile_I18nAddressFormMX) {
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

TEST_F(FormDataImporterTest, ImportStructuredAddressProfile_I18nAddressFormBR) {
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
TEST_F(FormDataImporterTest, ImportStructuredNameAddressProfile) {
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
      personal_data_manager_->address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Pablo Diego Ruiz y Picasso");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Pablo Diego");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Ruiz y Picasso");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_FIRST), u"Ruiz");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_CONJUNCTION), u"y");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_SECOND), u"Picasso");
}

TEST_F(FormDataImporterTest, ImportAddressProfiles) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_F(FormDataImporterTest, ImportSecondAddressProfiles) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSecondProfileFormStructure();
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {ConstructSecondProfile()});
}

TEST_F(FormDataImporterTest, ImportThirdAddressProfiles) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructThirdProfileFormStructure();
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {ConstructThirdProfile()});
}

// Test that with dependent locality parsing enabled, dependent locality fields
// are imported.
TEST_F(FormDataImporterTest, ImportAddressProfiles_DependentLocality) {
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
TEST_F(FormDataImporterTest, ImportAddressProfiles_DontAllowPrompt) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure,
                         /*allow_save_prompts=*/false);
  VerifyExpectationForExtractedAddressProfiles({});
}

TEST_F(FormDataImporterTest, ImportAddressProfileFromUnifiedSection) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();

  // Assign the address field another section than the other fields.
  form_structure->field(4)->set_section(
      Section::FromAutocomplete({.section = "another_section"}));

  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_BadEmail) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();

  // Change the value of the email field.
  ASSERT_EQ(form_structure->field(2)->Type().GetStorableType(), EMAIL_ADDRESS);
  form_structure->field(2)->set_value(u"bogus");

  // Verify that there was no import.
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that a 'confirm email' field does not block profile import.
TEST_F(FormDataImporterTest, ImportAddressProfiles_TwoEmails) {
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
TEST_F(FormDataImporterTest, ImportAddressProfiles_TwoDifferentEmails) {
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
TEST_F(FormDataImporterTest, ImportAddressProfiles_MultiplePhoneNumbers) {
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
TEST_F(FormDataImporterTest,
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
      *form_structure,
      {ConstructProfileFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {EMAIL_ADDRESS, kDefaultMail},
           // Note that this formatting is without a country code.
           {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneDomesticFormatting},
           {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
           {ADDRESS_HOME_CITY, kDefaultCity},
           {ADDRESS_HOME_STATE, kDefaultState},
           {ADDRESS_HOME_ZIP, kDefaultZip},
           {ADDRESS_HOME_COUNTRY, kDefaultCountry}})});
}

// Tests that not enough filled fields will result in not importing an address.
TEST_F(FormDataImporterTest, ImportAddressProfiles_NotEnoughFilledFields) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {CREDIT_CARD_NUMBER, kDefaultCreditCardNumber}});

  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
  // Also verify that there was no import of a credit card.
  ASSERT_EQ(
      0U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_MinimumAddressUSA) {
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

TEST_F(FormDataImporterTest, ImportAddressProfiles_MinimumAddressGB) {
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

TEST_F(FormDataImporterTest, ImportAddressProfiles_MinimumAddressGI) {
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

TEST_F(FormDataImporterTest,
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
      *form_structure,
      {ConstructProfileFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {EMAIL_ADDRESS, kDefaultMail},
           {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneDomesticFormatting},
           {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
           {ADDRESS_HOME_CITY, kDefaultCity},
           {ADDRESS_HOME_STATE, kDefaultState},
           {ADDRESS_HOME_ZIP, kDefaultZip},
           {ADDRESS_HOME_COUNTRY, kDefaultCountry}})});
}

// Test that even from unfocusable fields we extract.
TEST_F(FormDataImporterTest, ImportAddressProfiles_UnfocusableFields) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  // Set the Address line field as unfocusable.
  form_structure->field(4)->set_is_focusable(false);
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_MultilineAddress) {
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

TEST_F(FormDataImporterTest,
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

TEST_F(FormDataImporterTest, ImportAddressProfiles_TwoValidProfilesSameForm) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructShippingAndBillingFormStructure();
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructDefaultProfile(), ConstructSecondProfile()});
}

TEST_F(FormDataImporterTest,
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

TEST_F(FormDataImporterTest, ImportAddressProfiles_MissingInfoInOld) {
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

TEST_F(FormDataImporterTest, ImportAddressProfiles_MissingInfoInNew) {
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

TEST_F(FormDataImporterTest, ImportAddressProfiles_InsufficientAddress) {
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
TEST_F(FormDataImporterTest, ImportAddressProfiles_NoSynthesizedTypes) {
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
      AutofillType(ADDRESS_HOME_STREET_LOCATION));
  form_structure->field(2)->SetTypeTo(AutofillType(ADDRESS_HOME_LANDMARK));
  form_structure->field(3)->SetTypeTo(
      AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY));
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
TEST_F(FormDataImporterTest, ImportAddressProfiles_ContainsSynthesizedTypes) {
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
      AutofillType(ADDRESS_HOME_STREET_LOCATION));
  form_structure->field(2)->SetTypeTo(
      AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK));
  // Verify that no profile is imported.
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that a profile is created for countries with composed names.
TEST_F(FormDataImporterTest,
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
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr,
                       "No. 43 Bo Aung Gyaw Street", nullptr, "Yangon", "",
                       "11181", "MM", nullptr);
  EXPECT_THAT(personal_data_manager_->address_data_manager().GetProfiles(),
              UnorderedElementsCompareEqual(expected));
}

// TODO(crbug.com/41267680): Create profiles if part of a standalone part of a
// composed country name is present. Currently this is treated as an invalid
// country, which is ignored on import.
TEST_F(FormDataImporterTest,
       ImportAddressProfiles_IncompleteComposedCountryName) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry(
              "Myanmar"));  // Missing the [Burma] part
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// ExtractCreditCard tests.

// Tests that a valid credit card is extracted.
TEST_F(FormDataImporterTest, ExtractCreditCard_Valid) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  base::HistogramTester histogram_tester;
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));
}

// Tests that an invalid credit card number is not extracted.
TEST_F(FormDataImporterTest, ExtractCreditCard_InvalidCardNumber) {
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

  ASSERT_EQ(
      0U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());
}

// Tests that FormFieldData::user_input is preferred over FormFieldData::value
// for credit card numbers.
// Using FormFieldData::user_input enables showing the save-card prompt for
// sites which use JavaScript to set the credit-card <input> to '***'.
TEST_F(FormDataImporterTest,
       ExtractCreditCard_PreferUserInputForCreditCardNumber) {
  FormData form = CreateFullCreditCardForm("Jim Johansen", "4111111111111111",
                                           "02", "2999");

  FormFieldData* card_number_field =
      form.FindFieldByNameForTest(u"card_number");
  ASSERT_TRUE(card_number_field != nullptr);
  card_number_field->set_user_input(u"4444333322221111");

  // FormFieldData::user_input for non-credit card fields should be ignored.
  ASSERT_EQ(nullptr, form.FindFieldByNameForTest(u"cvc"));
  FormFieldData cvc_field =
      CreateTestFormField("CVC", "cvc", "001", FormControlType::kInputText);
  cvc_field.set_user_input(u"002");
  test_api(form).Append(cvc_field);

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);

  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Jim Johansen", "4444333322221111", "02", "2999", "", u"001");
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));
}

// Tests that a credit card with an empty expiration can be extracted due to the
// expiration date fix flow.
TEST_F(FormDataImporterTest,
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
TEST_F(FormDataImporterTest,
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
TEST_F(FormDataImporterTest, ExtractCreditCard_MonthSelectInvalidText) {
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
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card);

  // See that the invalid option text was converted to the right value.
  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "02", "2999",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));
}

TEST_F(FormDataImporterTest, ExtractCreditCard_TwoValidCards) {
  // Start with a single valid credit card form.
  std::unique_ptr<FormStructure> form_structure1 =
      ConstructDefaultCreditCardFormStructure();
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second different valid credit card.
  FormData form2 =
      CreateFullCreditCardForm("", "5500 0000 0000 0004", "02", "2999");

  std::unique_ptr<FormStructure> form_structure2 =
      ConstructFormStructureFromFormData(form2);

  std::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(*form_structure2);
  EXPECT_TRUE(extracted_credit_card2);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card2);

  CreditCard expected2 = test::CreateCreditCardWithInfo(
      "", "5500000000000004", "02", "2999",
      "");  // Imported cards have no billing info.
  // We ignore the order because multiple profiles or credit cards that
  // are added to the SQLite DB within the same second will be returned in GUID
  // (i.e., random) order.
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
              UnorderedElementsCompareEqual(expected, expected2));
}

// This form has the expiration year as one field with MM/YY.
TEST_F(FormDataImporterTest, ExtractCreditCard_Month2DigitYearCombination) {
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
TEST_F(FormDataImporterTest, ExtractCreditCard_Month4DigitYearCombination) {
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
TEST_F(FormDataImporterTest, ExtractCreditCard_1DigitMonth4DigitYear) {
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
TEST_F(FormDataImporterTest, ExtractCreditCard_2DigitYear) {
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
TEST_F(FormDataImporterTest,
       ExtractCreditCard_DuplicateServerCards_ExtractMaskedCard) {
  // Add a masked server card.
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "John Dillinger", "1111" /* Visa */,
                          "01", "2999", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);
  EXPECT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

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

TEST_F(FormDataImporterTest, ExtractCreditCard_SameCreditCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2998",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
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
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

TEST_F(FormDataImporterTest, ExtractCreditCard_ShouldReturnLocalCard) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2998",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
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
            *personal_data_manager_->payments_data_manager()
                 .GetLocalCreditCards()[0]);

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2999".
  CreditCard expected2 = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999",
      "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

TEST_F(FormDataImporterTest,
       ExtractCreditCard_ShouldReturnLocalCard_WithExtractedCvc) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card);

  CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
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
      *personal_data_manager_->payments_data_manager().GetLocalCreditCards()[0];
  EXPECT_TRUE(extracted_credit_card2->HasSameNumberAs(local_saved_credit_card));
  EXPECT_TRUE(
      extracted_credit_card2->HasSameExpirationDateAs(local_saved_credit_card));
  EXPECT_NE(extracted_credit_card2->cvc(), local_saved_credit_card.cvc());
  EXPECT_EQ(extracted_credit_card2->cvc(), u"123");
}

TEST_F(FormDataImporterTest, ExtractCreditCard_EmptyCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);

  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2998",
      "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
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
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

TEST_F(FormDataImporterTest, ExtractCreditCard_MissingInfoInNew) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure1 =
      ConstructFormStructureFromFormData(form1);
  std::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_credit_card);

  CreditCard expected = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  EXPECT_THAT(personal_data_manager_->payments_data_manager().GetCreditCards(),
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
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->payments_data_manager().GetCreditCards();
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
  const std::vector<CreditCard*>& results3 =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results3.size());
  EXPECT_THAT(*results3[0], ComparesEqual(expected3));
}

TEST_F(FormDataImporterTest, ExtractCreditCard_MissingInfoInOld) {
  // Start with a single valid credit card stored via the preferences.
  // Note the empty name.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "", "4111111111111111" /* Visa */,
                          "01", "2998", "1");
  personal_data_manager_->payments_data_manager().AddCreditCard(
      saved_credit_card);

  const std::vector<CreditCard*>& results1 =
      personal_data_manager_->payments_data_manager().GetCreditCards();
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
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

// We allow the user to store a credit card number with separators via the UI.
// We should not try to re-aggregate the same card with the separators stripped.
TEST_F(FormDataImporterTest, ExtractCreditCard_SameCardWithSeparators) {
  // Start with a single valid credit card stored via the preferences.
  // Note the separators in the credit card number.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->payments_data_manager().AddCreditCard(
      saved_credit_card);

  const std::vector<CreditCard*>& results1 =
      personal_data_manager_->payments_data_manager().GetCreditCards();
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
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(saved_credit_card));
}

// Ensure that if a verified credit card already exists, aggregated credit cards
// cannot modify it in any way.
TEST_F(FormDataImporterTest,
       ExtractCreditCard_ExistingVerifiedCardWithConflict) {
  // Start with a verified credit card.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2998", "");
  EXPECT_TRUE(credit_card.IsVerified());

  personal_data_manager_->payments_data_manager().AddCreditCard(credit_card);
  EXPECT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

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
  const std::vector<CreditCard*>& results =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(credit_card));
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set and
// reset correctly.
TEST_F(FormDataImporterTest,
       ExtractFormData_SecondImportResetsCreditCardRecordType) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->payments_data_manager().AddCreditCard(
      saved_credit_card);

  const std::vector<CreditCard*>& results =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(saved_credit_card));

  // Simulate a form submission with the same card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kLocalCard because
  // upload was offered and the card is a local card already on the device.
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kLocalCard);

  // Second form is filled with a new card so
  // `FormDataImporterTest::credit_card_import_type_` should be
  // reset. Simulate a form submission with a new card.
  FormData form2 = CreateFullCreditCardForm("Biggie Smalls", "4012888888881881",
                                            "01", "2999");

  std::unique_ptr<FormStructure> form_structure2 =
      ConstructFormStructureFromFormData(form2);
  auto extracted_data2 = ExtractFormDataAndProcessAddressCandidates(
      *form_structure2, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data2.extracted_credit_card);
  // |credit_card_import_type_| should be kNewCard because the
  // imported card is not already on the device.
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kNewCard);

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
      *form_structure3, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/false);
  // |credit_card_import_type_| should be NO_CARD because no
  // valid card was imported from the form.
  EXPECT_NE(0u, extracted_data3.address_profile_import_candidates.size());
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NewCard) {
  // Simulate a form submission with a new credit card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNewCard because the
  // imported card is not already on the device.
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kNewCard);
}

// Ensures that `credit_card_import_type_` is set correctly.
TEST_F(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_LocalCard) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->payments_data_manager().AddCreditCard(
      saved_credit_card);

  const std::vector<CreditCard*>& results =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(saved_credit_card));

  // Simulate a form submission with the same card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kLocalCard because
  // upload was offered and the card is a local card already on the device.
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kLocalCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_MaskedServerCard) {
  // Add a masked server card.
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "Biggie Smalls", "1111" /* Visa */,
                          "01", "2999", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);
  EXPECT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

  // Simulate a form submission with the same masked server card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be SERVER_CARD.
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kServerCard);
}

// Ensures that `FormDataImporterTest::credit_card_import_type_` and
// `record_type` denote that a duplicate card was extracted, and it is a server
// card when the flag is on.
TEST_F(
    FormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_DuplicateLocalAndMaskedServerCard) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard local_card = test::GetCreditCard();
  test::SetCreditCardInfo(
      &local_card, kDefaultCreditCardName, kDefaultCreditCardNumber /* Visa */,
      kDefaultCreditCardExpMonth, kDefaultCreditCardExpYear, "");
  personal_data_manager_->payments_data_manager().AddCreditCard(local_card);
  // Add a masked server card.
  CreditCard server_card = test::GetMaskedServerCard();
  test::SetCreditCardInfo(
      &server_card, kDefaultCreditCardName, kDefaultCreditCardNumber /* Visa */,
      kDefaultCreditCardExpMonth, kDefaultCreditCardExpYear, "");
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);

  // Simulate a form submission with the same masked server card.
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  EXPECT_EQ(test_api(form_data_importer()).credit_card_import_type(),
            FormDataImporter::CreditCardImportType::kDuplicateLocalServerCard);
  EXPECT_EQ(extracted_data.extracted_credit_card->record_type(),
            CreditCard::RecordType::kMaskedServerCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NoCard_InvalidCardNumber) {
  // Simulate a form submission using a credit card with an invalid card number.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1112", "01", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNoCard because no
  // valid card was successfully imported from the form.
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NoCard_VirtualCard) {
  // Simulate a form submission using a credit card that is known as a virtual
  // card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  form_data_importer().CacheFetchedVirtualCard(u"1111");
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  EXPECT_TRUE(extracted_data.extracted_credit_card);
  // `credit_card_import_type_` should be `kVirtualCard` because the
  // card extracted from the form was a virtual card.
  EXPECT_EQ(test_api(form_data_importer()).credit_card_import_type(),
            FormDataImporter::CreditCardImportType::kVirtualCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(
    FormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_NewCard_ExpiredCard_WithExpDateFixFlow) {
  // Simulate a form submission with an expired credit card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "1999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNewCard because card
  // was successfully imported from the form via the expiration date fix flow.
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kNewCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_F(FormDataImporterTest,
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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // Mandatory re-auth opt-in should not be attempted if no card was extracted
  // from the form.
  EXPECT_CALL(
      *static_cast<::testing::NiceMock<payments::MockMandatoryReauthManager>*>(
          autofill_client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager()),
      ShouldOfferOptin)
      .Times(0);

  ASSERT_FALSE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNoCard because the
  // form doesn't have credit card section.
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that `cvc` is set when a server card is found.
TEST_F(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_ServerCardWithCvc) {
  // Add a valid server card.
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  test::SetCreditCardInfo(&server_card, "John Dillinger",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);
  ASSERT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

  // Simulate a form submission with the same card number but different
  // expiration date.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "02", "2999");
  test_api(form).Append(
      CreateTestFormField("CVC:", "cvc", "123", FormControlType::kInputText));
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  EXPECT_EQ(extracted_data.extracted_credit_card->cvc(), u"123");
}

// Ensures that `credit_card_import_type_` is set as kNewCard when there is a
// masked server card with the same last four but different expiration date.
TEST_F(
    FormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_MaskedServerCardWithSameLastFour) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "John Dillinger", "1111" /* Visa */,
                          "01", "2999", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);
  ASSERT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

  // Simulate a form submission with the card with same last four but different
  // expiration date.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "02", "2999");

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  ASSERT_EQ(extracted_data.extracted_credit_card->expiration_month(), 2);
  // `credit_card_import_type_` should be kNewCard because a server card with
  // the same card number was found, but they have different expiration date.
  ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
              FormDataImporter::CreditCardImportType::kNewCard);
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
    FormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_TwoMaskedServerCardWithSameLastFour) {
  CreditCard server_card1(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card1, "John Dillinger", "1111" /* Visa */,
                          "01", "2111", "");
  server_card1.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card1);
  CreditCard server_card2(CreditCard::RecordType::kMaskedServerCard, "a124");
  test::SetCreditCardInfo(&server_card2, "John Dillinger", "1111" /* Visa */,
                          "02", "2112", "");
  server_card2.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card2);
  EXPECT_EQ(
      2U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

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
        *form_structure, /*profile_autofill_enabled=*/true,
        /*payment_methods_autofill_enabled=*/true);
    ASSERT_TRUE(extracted_data.extracted_credit_card);
    // `credit_card_import_type_` should be kNewCard because a masked server
    // card with the same card number was found, but they have different
    // expiration date.
    ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
                FormDataImporter::CreditCardImportType::kNewCard);
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
        *form_structure, /*profile_autofill_enabled=*/true,
        /*payment_methods_autofill_enabled=*/true);
    ASSERT_TRUE(extracted_data.extracted_credit_card);
    ASSERT_TRUE(extracted_data.extracted_credit_card->Compare(server_card2) ==
                0);
    // `credit_card_import_type_` should be kServerCard because a masked server
    // card with the same card number and expiration date was found.
    ASSERT_TRUE(test_api(form_data_importer()).credit_card_import_type() ==
                FormDataImporter::CreditCardImportType::kServerCard);
    histogram_tester.ExpectUniqueSample(
        "Autofill.SubmittedServerCardExpirationStatus",
        AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
  }
}

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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  // Test that the address has been saved.
  AutofillProfile expected_address = ConstructDefaultProfile();
  const std::vector<const AutofillProfile*>& results_addr =
      personal_data_manager_->address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_THAT(*results_addr[0], ComparesEqual(expected_address));

  // Test that the credit card has also been saved.
  CreditCard expected_card = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->payments_data_manager().GetCreditCards();
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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  ASSERT_TRUE(extracted_data.extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  // Test that both addresses have been saved.
  EXPECT_EQ(
      2U, personal_data_manager_->address_data_manager().GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(expected_card));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(FormDataImporterTest, ExtractFormData_ImportIbanRecordType_NoIban) {
  // Simulate a form submission with no IBAN.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_iban);
}

TEST_F(FormDataImporterTest, ExtractFormData_SubmittingIbanFormUpdatesPref) {
  // The pref should always start disabled.
  ASSERT_FALSE(personal_data_manager_->payments_data_manager()
                   .IsAutofillHasSeenIbanPrefEnabled());

  // Simulate a form submission with a new IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());
  ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // Submitting the IBAN form permanently enables the pref.
  EXPECT_TRUE(personal_data_manager_->payments_data_manager()
                  .IsAutofillHasSeenIbanPrefEnabled());
}

TEST_F(FormDataImporterTest,
       ExtractFormData_SubmittingCreditCardFormDoesNotUpdateIbanPref) {
  ASSERT_FALSE(personal_data_manager_->payments_data_manager()
                   .IsAutofillHasSeenIbanPrefEnabled());
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // Submitting the credit card form won't enable the pref, even if the flag is
  // on.
  EXPECT_FALSE(personal_data_manager_->payments_data_manager()
                   .IsAutofillHasSeenIbanPrefEnabled());
}

TEST_F(FormDataImporterTest,
       ExtractFormData_ImportIbanRecordType_IbanAutofill_NewInvalidIban) {
  // Simulate a form submission with a new IBAN.
  // Invalid Kuwait IBAN with incorrect IBAN length.
  // KW16 will be converted into 203216, and the remainder on 97 is 1.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(
          CreateTestIbanFormData("KW1600000000000000000"));
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // IBAN candidate is empty as the value is invalid.
  ASSERT_FALSE(extracted_data.extracted_iban);
}

TEST_F(FormDataImporterTest,
       ExtractFormData_ImportIbanRecordType_IbanAutofill_NewIban) {
  // Simulate a form submission with a new IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  EXPECT_TRUE(extracted_data.extracted_iban);
}

TEST_F(FormDataImporterTest, ExtractFormData_ImportIbanRecordType_LocalIban) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  const std::string guid =
      personal_data_manager_->payments_data_manager().AddAsLocalIban(iban);
  // Should set identifier and record_type manually here as `iban` has been
  // passed by value above in `AddAsLocalIban`, and `AddAsLocalIban` method sets
  // identifier and record_type to the given `iban`.
  iban.set_identifier(Iban::Guid(guid));
  iban.set_record_type(Iban::kLocalIban);

  const std::vector<const Iban*>& results =
      personal_data_manager_->payments_data_manager().GetLocalIbans();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(iban));

  // Simulate a form submission with the same IBAN. The IBAN can be extracted
  // from the form.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(
          CreateTestIbanFormData(/*value=*/test::kIbanValue));
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  EXPECT_TRUE(extracted_data.extracted_iban);
}

#endif  // !BUILDFLAG(IS_IOS)

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
      *form_structure, /*profile_autofill_enabled=*/false,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  // Test that addresses were not saved.
  EXPECT_EQ(
      0U, personal_data_manager_->address_data_manager().GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->payments_data_manager().GetCreditCards();
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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/false);
  ASSERT_FALSE(extracted_data.extracted_credit_card);

  // Test that the address has been saved.
  AutofillProfile expected_address = ConstructDefaultProfile();
  const std::vector<const AutofillProfile*>& results_addr =
      personal_data_manager_->address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_THAT(*results_addr[0], ComparesEqual(expected_address));

  // Test that the credit card was not saved.
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->payments_data_manager().GetCreditCards();
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
      *form_structure, /*profile_autofill_enabled=*/false,
      /*payment_methods_autofill_enabled=*/false);
  ASSERT_FALSE(extracted_data.extracted_credit_card);

  // Test that addresses were not saved.
  EXPECT_EQ(
      0U, personal_data_manager_->address_data_manager().GetProfiles().size());

  // Test that the credit card was not saved.
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(0U, results_cards.size());
}

TEST_F(FormDataImporterTest, DuplicateMaskedServerCard) {
  CreditCard server_card1(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card1, "John Dillinger", "1881" /* Visa */,
                          "01", "2999", "");
  server_card1.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card1);
  CreditCard server_card2(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_card2, "Clyde Barrow",
                          "0005" /* American Express */, "04", "2999", "");
  server_card2.SetNetworkForMaskedCard(kAmericanExpressCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card2);
  EXPECT_EQ(
      2U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
}

// Tests that a credit card form that is hidden after receiving input still
// imports the card.
TEST_F(FormDataImporterTest, ExtractFormData_HiddenCreditCardFormAfterEntered) {
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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  personal_data_manager_->payments_data_manager().OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  // Test that the credit card has been saved.
  CreditCard expected_card = test::CreateCreditCardWithInfo(
      "Biggie Smalls", "4111111111111111", "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(expected_card));
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date month.
TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationMonth) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_card, "Clyde Barrow", "1111" /* Visa */, "04",
                          "2111", "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);
  EXPECT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date year.
TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationYear) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_card, "Clyde Barrow", "1111" /* Visa */, "04",
                          "2111", "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);
  EXPECT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
}

// Ensure that we still offer to save if we have different cards stored as a
// server card and user submitted an invalid expiration date year.
TEST_F(
    FormDataImporterTest,
    Metrics_SubmittedDifferentServerCardExpirationStatus_EmptyExpirationYear) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "c789");
  test::SetCreditCardInfo(&server_card, "Clyde Barrow", "1881" /* Visa */, "04",
                          "2111", "1");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);
  EXPECT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
}

TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMatch) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "John Dillinger", "1111" /* Visa */,
                          "01", "2111", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);
  EXPECT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
}

TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMismatch) {
  CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_card, "John Dillinger", "1111" /* Visa */,
                          "01", "2111", "");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      server_card);
  EXPECT_EQ(
      1U,
      personal_data_manager_->payments_data_manager().GetCreditCards().size());

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
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH, 1);
}

TEST_F(FormDataImporterTest, SilentlyUpdateExistingProfileByIncompleteProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillSilentProfileUpdateForInsufficientImport);

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  // Set the verification status for the first and middle name to parsed.
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Marion",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Mitchell",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Morrison",
                                           VerificationStatus::kParsed);

  personal_data_manager_->address_data_manager().AddProfile(profile);

  // Simulate a form submission with conflicting info.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("First name:", "first_name", "Marion",
                           FormControlType::kInputText),
       CreateTestFormField("Middle name:", "middle_name", "",
                           FormControlType::kInputText),
       CreateTestFormField("Last name:", "last_name", "Mitchell Morrison",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  ExtractAddressProfiles(/*extraction_successful=*/false, *form_structure);

  // Expect that no new profile is saved.
  const std::vector<const AutofillProfile*>& results =
      personal_data_manager_->address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_NE(0, profile.Compare(*results[0]));
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Marion Mitchell Morrison");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Marion");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Mitchell Morrison");
}

TEST_F(
    FormDataImporterTest,
    SilentlyUpdateExistingProfileByIncompleteProfile_DespiteDisallowedPrompts) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillSilentProfileUpdateForInsufficientImport);

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  // Set the verification status for the first and middle name to parsed.
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Marion",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Mitchell",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Morrison",
                                           VerificationStatus::kParsed);

  personal_data_manager_->address_data_manager().AddProfile(profile);

  // Simulate a form submission with conflicting info.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("First name:", "first_name", "Marion",
                           FormControlType::kInputText),
       CreateTestFormField("Middle name:", "middle_name", "",
                           FormControlType::kInputText),
       CreateTestFormField("Last name:", "last_name", "Mitchell Morrison",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  ExtractAddressProfiles(/*extraction_successful=*/false, *form_structure,
                         /*allow_save_prompts=*/false);

  // Expect that no new profile is saved and the existing profile is updated.
  const std::vector<const AutofillProfile*>& results =
      personal_data_manager_->address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_NE(0, profile.Compare(*results[0]));
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Marion Mitchell Morrison");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Marion");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Mitchell Morrison");
}

TEST_F(FormDataImporterTest, UnusableIncompleteProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillSilentProfileUpdateForInsufficientImport);

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  // Set the verification status for the first and middle name to parsed.
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Marion",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Mitchell",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Morrison",
                                           VerificationStatus::kParsed);

  personal_data_manager_->address_data_manager().AddProfile(profile);

  // Simulate a form submission with conflicting info.
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(
      {CreateTestFormField("First name:", "first_name", "Marion",
                           FormControlType::kInputText),
       CreateTestFormField("Middle name:", "middle_name", "",
                           FormControlType::kInputText),
       CreateTestFormField("Last name:", "last_name", "Mitch Morrison",
                           FormControlType::kInputText)});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  ExtractAddressProfiles(/*extraction_successful=*/false, *form_structure);

  // Expect that no new profile is saved.
  const std::vector<const AutofillProfile*>& results =
      personal_data_manager_->address_data_manager().GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(profile));
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Marion Mitchell Morrison");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Marion");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"Mitchell");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Morrison");
}

// Tests that metrics are correctly recorded when removing setting-inaccessible
// fields.
// Note that this function doesn't test the removal functionality itself. This
// is done in the AutofillProfile unit tests.
TEST_F(FormDataImporterTest, RemoveInaccessibleProfileValuesMetrics) {
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
TEST_F(FormDataImporterTest, MultiStepImport) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSplitDefaultProfileFormStructure(/*part=*/1);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  form_structure = ConstructSplitDefaultProfileFormStructure(/*part=*/2);
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that when multi-step complements are enabled, complete profiles those
// import was accepted are added as a multi-step candidate. This enables
// complementing the profile with additional information on further pages.
TEST_F(FormDataImporterTest, MultiStepImport_Complement) {
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
TEST_F(FormDataImporterTest, MultiStepImport_Complement_ExternalUpdate) {
  // Extract the default profile without an email address.
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, EMAIL_ADDRESS, "");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractFormDataAndProcessAddressCandidates(*form_structure);
  VerifyExpectationForExtractedAddressProfiles(
      {ConstructProfileFromTypeValuePairs(type_value_pairs)});

  // Update the profile's ZIP through external means.
  AutofillProfile profile =
      *personal_data_manager_->address_data_manager().GetProfiles()[0];
  profile.SetInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"12345", kLocale,
                                        VerificationStatus::kObserved);
  personal_data_manager_->address_data_manager().UpdateProfile(profile);

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
TEST_F(FormDataImporterTest, MultiStepImport_Complement_ExternalRemove) {
  // Extract the default profile without an email address.
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, EMAIL_ADDRESS, "");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractFormDataAndProcessAddressCandidates(*form_structure);
  VerifyExpectationForExtractedAddressProfiles(
      {ConstructProfileFromTypeValuePairs(type_value_pairs)});

  // Remove the profile through external means.
  personal_data_manager_->RemoveByGUID(
      personal_data_manager_->address_data_manager().GetProfiles()[0]->guid());

  // Expect that the removed profile cannot be updated with an email address.
  form_structure = ConstructDefaultEmailFormStructure();
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that multi-step candidate profiles from different origins are not
// merged.
TEST_F(FormDataImporterTest, MultiStepImport_DifferentOrigin) {
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
TEST_F(FormDataImporterTest, MultiStepImport_TTL) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSplitDefaultProfileFormStructure(/*part=*/1);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  task_environment_.FastForwardBy(kMultiStepImportTTL + base::Minutes(1));

  form_structure = ConstructSplitDefaultProfileFormStructure(/*part=*/2);
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
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
  ExtractFormDataAndProcessAddressCandidates(*form_structure);

  std::optional<FormStructure::FormAssociations> associations =
      form_data_importer().GetFormAssociations(form_signature);
  // Expect the same form signature for the two most recent address form, as
  // `form_structure` consists of two sections.
  EXPECT_TRUE(associations);
  EXPECT_EQ(associations->last_address_form_submitted, form_signature);
  EXPECT_EQ(associations->second_last_address_form_submitted, form_signature);
  EXPECT_FALSE(associations->last_credit_card_form_submitted);
}

// Tests that ac=unrecognized fields have a prediction, but are not imported.
TEST_F(FormDataImporterTest, SkipAutocompleteUnrecognizedFields) {
  // Create a `form_structure` where the email field has ac=unrecognized.
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  AutofillField* email_field = form_structure->field(2);
  ASSERT_EQ(email_field->Type().GetStorableType(), EMAIL_ADDRESS);
  email_field->SetHtmlType(HtmlFieldType::kUnrecognized, HtmlFieldMode::kNone);

  // Expect that ac=unrecognized doesn't change the prediction.
  EXPECT_EQ(email_field->Type().GetStorableType(), EMAIL_ADDRESS);

  // Expect that the email address is not imported.
  AutofillProfile expected_profile = ConstructDefaultProfile();
  expected_profile.ClearFields({EMAIL_ADDRESS});
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {expected_profile});
}

#if !BUILDFLAG(IS_IOS)
TEST_F(FormDataImporterTest,
       ProcessIbanImportCandidate_ShouldOfferLocalSave_NewIban) {
  Iban extracted_iban = test::GetLocalIban();

  EXPECT_TRUE(
      form_data_importer().ProcessIbanImportCandidate(extracted_iban));
}

TEST_F(FormDataImporterTest,
       ExtractFormData_ProcessIbanImportCandidate_NoIban) {
  // Simulate a form submission with an empty Iban.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData(/*value=*/""));

  ASSERT_FALSE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}

TEST_F(
    FormDataImporterTest,
    ExtractFormData_ProcessIbanImportCandidate_PaymentMethodsSettingDisabled) {
  // Simulate a form submission with a new IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());

  ASSERT_FALSE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/false));
}

TEST_F(FormDataImporterTest,
       ExtractFormData_ProcessIbanImportCandidate_NewIban) {
  // Simulate a form submission with a new IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());

  EXPECT_TRUE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}

TEST_F(FormDataImporterTest,
       ExtractFormData_ProcessIbanImportCandidate_LocalIban) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  personal_data_manager_->payments_data_manager().AddAsLocalIban(iban);

  // Simulate a form submission with the same IBAN. The IBAN should not be
  // offered to be saved, because it already exists as a local IBAN.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(
          CreateTestIbanFormData(/*value=*/test::kIbanValue));

  EXPECT_FALSE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}

TEST_F(FormDataImporterTest,
       ExtractFormData_ProcessIbanImportCandidate_MaxStrikes) {
  IbanSaveStrikeDatabase iban_save_strike_database =
      IbanSaveStrikeDatabase(autofill_client_->GetStrikeDatabase());

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

TEST_F(FormDataImporterTest, ProcessExtractedCreditCard_EmptyCreditCard) {
  std::optional<CreditCard> extracted_credit_card;
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();

  // `form_data_importer()`'s `credit_card_import_type_` is set
  // to kLocalCard because we need to make sure we do not return early in the
  // kNewCard case, and kLocalCard with upstream enabled but empty
  // `extracted_credit_card` is the most likely scenario for a crash.
  test_api(form_data_importer())
      .set_credit_card_import_type(
          FormDataImporter::CreditCardImportType::kLocalCard);

  // We need a sync service so that
  // LocalCardMigrationManager::ShouldOfferLocalCardMigration() does not crash.
  syncer::TestSyncService sync_service;
  personal_data_manager_->SetSyncServiceForTest(&sync_service);

  EXPECT_FALSE(test_api(form_data_importer())
                   .ProcessExtractedCreditCard(
                       *form_structure, extracted_credit_card,
                       /*is_credit_card_upstream_enabled=*/true));
  personal_data_manager_->SetSyncServiceForTest(nullptr);
}

#if !BUILDFLAG(IS_IOS)
TEST_F(FormDataImporterTest, ProcessExtractedCreditCard_VirtualCardEligible) {
  CreditCard extracted_credit_card = test::GetMaskedServerCard();
  extracted_credit_card.SetNetworkForMaskedCard(kAmericanExpressCard);
  extracted_credit_card.set_instrument_id(1111);
  extracted_credit_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible);
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();

  test_api(form_data_importer())
      .set_credit_card_import_type(
          FormDataImporter::CreditCardImportType::kServerCard);
  form_data_importer().SetFetchedCardInstrumentId(2222);

  // We need a sync service so that
  // LocalCardMigrationManager::ShouldOfferLocalCardMigration() does not
  // crash.
  syncer::TestSyncService sync_service;
  personal_data_manager_->SetSyncServiceForTest(&sync_service);

  EXPECT_CALL(virtual_card_enrollment_manager(),
              InitVirtualCardEnroll(_, VirtualCardEnrollmentSource::kDownstream,
                                    _, _, _, _))
      .Times(0);

  EXPECT_FALSE(test_api(form_data_importer())
                   .ProcessExtractedCreditCard(
                       *form_structure, extracted_credit_card,
                       /*is_credit_card_upstream_enabled=*/true));

  form_data_importer().SetFetchedCardInstrumentId(1111);
  EXPECT_CALL(virtual_card_enrollment_manager(),
              InitVirtualCardEnroll(_, VirtualCardEnrollmentSource::kDownstream,
                                    _, _, _, _))
      .Times(1);

  EXPECT_TRUE(test_api(form_data_importer())
                  .ProcessExtractedCreditCard(
                      *form_structure, extracted_credit_card,
                      /*is_credit_card_upstream_enabled=*/true));

  personal_data_manager_->SetSyncServiceForTest(nullptr);
}
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// Test that in the case where the MandatoryReauthManager denotes we should not
// offer re-auth opt-in, we do not start the opt-in flow.
TEST_F(FormDataImporterTest,
       ProcessExtractedCreditCard_MandatoryReauthNotOffered) {
  CreditCard extracted_credit_card = test::GetVirtualCard();
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kVirtualCard);
  test_api(form_data_importer())
      .set_credit_card_import_type(
          FormDataImporter::CreditCardImportType::kVirtualCard);

  EXPECT_CALL(
      *static_cast<::testing::NiceMock<payments::MockMandatoryReauthManager>*>(
          autofill_client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager()),
      ShouldOfferOptin)
      .Times(1)
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      *static_cast<::testing::NiceMock<payments::MockMandatoryReauthManager>*>(
          autofill_client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager()),
      StartOptInFlow)
      .Times(0);

  test_api(form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, extracted_credit_card,
                                  /*is_credit_card_upstream_enabled=*/true);
}

// Test that in the case where the MandatoryReauthManager denotes re-auth opt-in
// should be offered, but the card is a new card (presumed updated by the user
// after filling), re-auth opt-in is not offered.
TEST_F(FormDataImporterTest,
       ProcessExtractedCreditCard_MandatoryReauthNotOffered_NewCard) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalCard);
  test_api(form_data_importer())
      .set_credit_card_import_type(
          FormDataImporter::CreditCardImportType::kNewCard);

  EXPECT_CALL(
      *static_cast<::testing::NiceMock<payments::MockMandatoryReauthManager>*>(
          autofill_client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager()),
      ShouldOfferOptin)
      .Times(0);
  EXPECT_CALL(
      *static_cast<::testing::NiceMock<payments::MockMandatoryReauthManager>*>(
          autofill_client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager()),
      StartOptInFlow)
      .Times(0);

  test_api(form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, test::GetCreditCard2(),
                                  /*is_credit_card_upstream_enabled=*/true);
}

// Test that in the case where the MandatoryReauthManager denotes we should
// offer re-auth opt-in, we start the opt-in in credit card processing flow if
// the card is not a new card.
TEST_F(FormDataImporterTest,
       ProcessExtractedCreditCard_MandatoryReauthOffered) {
  CreditCard extracted_credit_card = test::GetCreditCard2();
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalCard);
  test_api(form_data_importer())
      .set_credit_card_import_type(
          FormDataImporter::CreditCardImportType::kLocalCard);

  EXPECT_CALL(
      *static_cast<::testing::NiceMock<payments::MockMandatoryReauthManager>*>(
          autofill_client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager()),
      ShouldOfferOptin)
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *static_cast<::testing::NiceMock<payments::MockMandatoryReauthManager>*>(
          autofill_client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager()),
      StartOptInFlow)
      .Times(1);

  EXPECT_TRUE(test_api(form_data_importer())
                  .ProcessExtractedCreditCard(
                      *form_structure, extracted_credit_card,
                      /*is_credit_card_upstream_enabled=*/true));

  // Ensure that we reset the record type at the end of the flow.
  EXPECT_FALSE(
      test_api(form_data_importer())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

// Test that in the case where the MandatoryReauthManager denotes we should
// offer re-auth opt-in, we start the opt-in in IBAN processing flow.
TEST_F(FormDataImporterTest, ProcessExtractedIban_MandatoryReauthOffered) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());
  form_data_importer()
      .SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalIban);

  EXPECT_CALL(*autofill_client_->GetPaymentsAutofillClient()
                   ->GetOrCreatePaymentsMandatoryReauthManager(),
              ShouldOfferOptin)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*autofill_client_->GetPaymentsAutofillClient()
                   ->GetOrCreatePaymentsMandatoryReauthManager(),
              StartOptInFlow);

  EXPECT_TRUE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));

  // Ensure that we reset the record type at the end of the flow.
  EXPECT_FALSE(
      test_api(form_data_importer())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

// Test that in the case where the MandatoryReauthManager denotes we should not
// offer re-auth opt-in, we do not start the opt-in in IBAN processing flow.
TEST_F(FormDataImporterTest, ProcessExtractedIban_MandatoryReauthNotOffered) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(CreateTestIbanFormData());

  EXPECT_CALL(*autofill_client_->GetPaymentsAutofillClient()
                   ->GetOrCreatePaymentsMandatoryReauthManager(),
              ShouldOfferOptin)
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*autofill_client_->GetPaymentsAutofillClient()
                   ->GetOrCreatePaymentsMandatoryReauthManager(),
              StartOptInFlow)
      .Times(0);

  EXPECT_TRUE(ExtractFormDataAndProcessIbanCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));

  // Ensure that we reset the record type at the end of the flow.
  EXPECT_FALSE(
      test_api(form_data_importer())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

// Test that ProceedWithSavingIfApplicable gets called for server cards with the
// correct pre-requisites set.
TEST_F(FormDataImporterTest,
       ProcessExtractedCreditCard_ProceedWithSavingIfApplicable_Server) {
  CreditCard card = test::WithCvc(test::GetCreditCard(), u"123");
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  test_api(form_data_importer())
      .set_credit_card_import_type(
          FormDataImporter::CreditCardImportType::kServerCard);

  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable);
  test_api(form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, card,
                                  /*is_credit_card_upstream_enabled=*/false);
}

// Test that ProceedWithSavingIfApplicable gets called for local cards with the
// correct pre-requisites set.
TEST_F(FormDataImporterTest,
       ProcessExtractedCreditCard_ProceedWithSavingIfApplicable_Local) {
  CreditCard card = test::WithCvc(test::GetCreditCard(), u"123");
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  test_api(form_data_importer())
      .set_credit_card_import_type(
          FormDataImporter::CreditCardImportType::kLocalCard);

  EXPECT_CALL(credit_card_save_manager(), ProceedWithSavingIfApplicable);
  test_api(form_data_importer())
      .ProcessExtractedCreditCard(*form_structure, card,
                                  /*is_credit_card_upstream_enabled=*/false);
}

// Test that Autofill will not try to import from a field that was filled with
// fallback.
TEST_F(FormDataImporterTest,
       GetObservedFieldValues_SkipFieldsFilledWithFallback) {
  AutofillField field;
  field.SetTypeTo(AutofillType(NAME_FIRST));
  field.set_value(u"First");

  base::flat_map<FieldType, std::u16string> observed_field_types =
      test_api(form_data_importer())
          .GetObservedFieldValues(
              std::to_array<const AutofillField*>({&field}));
  EXPECT_EQ(observed_field_types.size(), 1u);

  // Set the autofilled type of the field as something different from its
  // classified type, representing that the field was filled using this type as
  // fallback.
  field.set_autofilled_type(NAME_FULL);
  observed_field_types = test_api(form_data_importer())
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
  field.SetTypeTo(AutofillType(NAME_FIRST));
  field.set_value(u"First");
  {
    base::flat_map<FieldType, std::u16string> observed_field_types =
        test_api(form_data_importer())
            .GetObservedFieldValues(
                std::to_array<const AutofillField*>({&field}));
    EXPECT_TRUE(observed_field_types.empty());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list{
        features::kAutofillImportFromAutocompleteUnrecognized};
    base::flat_map<FieldType, std::u16string> observed_field_types =
        test_api(form_data_importer())
            .GetObservedFieldValues(
                std::to_array<const AutofillField*>({&field}));
    EXPECT_EQ(observed_field_types.size(), 1u);
  }
}

// Test case for credit card extraction.
class FormDataImporterTest_ExtractCreditCardFromForm
    : public FormDataImporterTest {
 public:
  enum class Mode { kDefaultValue, kAutofilled, kUserEdited };

  void PushField(FieldType field_type,
                 std::u16string value,
                 Mode mode = Mode::kDefaultValue) {
    AutofillField& f = test_api(form_).PushField();
    f.set_server_predictions({test::CreateFieldPrediction(field_type)});
    f.set_value(std::move(value));
    f.set_is_autofilled(mode == Mode::kAutofilled);
    f.set_is_user_edited(mode == Mode::kUserEdited);
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
  auto r = form_data_importer().ExtractCreditCardFromForm(form_);
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
  auto r = form_data_importer().ExtractCreditCardFromForm(form_);
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
  auto r = form_data_importer().ExtractCreditCardFromForm(form_);
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
  auto r = form_data_importer().ExtractCreditCardFromForm(form_);
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NAME_FULL, kLocale),
            u"Audrey Hepburn");
  EXPECT_EQ(r.card.GetInfo(FieldType::CREDIT_CARD_NUMBER, kLocale),
            u"4444333322221111");
  EXPECT_EQ(
      r.card.GetInfo(FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, kLocale),
      u"12/2020");
  EXPECT_FALSE(r.has_duplicate_credit_card_field_type);
}

}  // namespace
}  // namespace autofill
