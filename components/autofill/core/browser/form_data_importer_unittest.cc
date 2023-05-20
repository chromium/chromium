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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UTF8ToUTF16;
using testing::_;

namespace autofill {

namespace {

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
constexpr char kDefaultPhone[] = "+1 650-555-0000";
constexpr char kDefaultPhoneAlternativeFormatting[] = "650-555-0000";
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
constexpr char kSecondPhone[] = "+1 651-666-1111";
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
constexpr char kThirdPhone[] = "+1 851-777-2222";

constexpr char kDefaultCreditCardName[] = "Biggie Smalls";
constexpr char kDefaultCreditCardNumber[] = "4111 1111 1111 1111";
constexpr char kDefaultCreditCardExpMonth[] = "01";
constexpr char kDefaultCreditCardExpYear[] = "2999";

// For a given ServerFieldType |type| returns a pair of field name and label
// that should be parsed into this type by our field type parsers.
std::pair<std::string, std::string> GetLabelAndNameForType(
    ServerFieldType type) {
  static const std::map<ServerFieldType, std::pair<std::string, std::string>>
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
                     << AutofillType(type).ToString();
    return {std::string(), std::string()};
  }
  return it->second;
}

using TypeValuePairs = std::vector<std::pair<ServerFieldType, std::string>>;

// Constructs a FormData instance for |url| from a vector of type value pairs
// that defines a sequence of fields and the filled values.
// The field names and labels for the different types are relieved from
// |GetLabelAndNameForType(type)|
FormData ConstructFormDateFromTypeValuePairs(
    TypeValuePairs type_value_pairs,
    std::string url = "https://www.foo.com") {
  FormData form;
  form.url = GURL(url);

  FormFieldData field;
  for (const auto& [type, value] : type_value_pairs) {
    const auto& [name, label] = GetLabelAndNameForType(type);
    test::CreateTestFormField(
        name.c_str(), label.c_str(), value.c_str(),
        type == ADDRESS_HOME_STREET_ADDRESS ? "textarea" : "text", &field);
    form.fields.push_back(field);
  }

  return form;
}

// Constructs a FormStructure instance from a FormData instance and determines
// the heuristic types.
std::unique_ptr<FormStructure> ConstructFormStructureFromFormData(
    const FormData& form) {
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  AutofillProfile profile;
  for (const auto& [type, value] : type_value_pairs) {
    profile.SetRawInfoWithVerificationStatus(type, base::UTF8ToUTF16(value),
                                             VerificationStatus::kObserved);
  }
  if (!profile.FinalizeAfterImport())
    NOTREACHED();
  return profile;
}

// Returns a vector of ServerFieldType and value pairs used to construct the
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
                     ServerFieldType type,
                     const std::string& value) {
  auto it = base::ranges::find(pairs, type,
                               [](const auto& pair) { return pair.first; });
  DCHECK(it != pairs.end());
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
  form.fields[0].autocomplete_attribute = autocomplete;
  form.fields[0].parsed_autocomplete = ParseAutocompleteAttribute(autocomplete);
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

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

enum UserMode { USER_MODE_NORMAL, USER_MODE_INCOGNITO };

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() = default;
  ~PersonalDataLoadedObserverMock() override = default;

  MOCK_METHOD(void, OnPersonalDataChanged, (), (override));
  MOCK_METHOD(void, OnPersonalDataFinishedProfileTasks, (), (override));
};

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
  std::vector<::testing::Matcher<T*>> matchers;
  for (const T& expected : expected_values)
    matchers.push_back(::testing::Pointee(ComparesEqual(expected)));
  return ::testing::UnorderedElementsAreArray(matchers);
}

template <typename... Matchers>
auto UnorderedElementsCompareEqual(Matchers... matchers) {
  return ::testing::UnorderedElementsAre(
      ::testing::Pointee(ComparesEqual(std::move(matchers)))...);
}

}  // anonymous namespace

class MockVirtualCardEnrollmentManager
    : public TestVirtualCardEnrollmentManager {
 public:
  MockVirtualCardEnrollmentManager(
      TestPersonalDataManager* personal_data_manager,
      payments::TestPaymentsClient* payments_client,
      TestAutofillClient* autofill_client)
      : TestVirtualCardEnrollmentManager(personal_data_manager,
                                         payments_client,
                                         autofill_client) {}
  MOCK_METHOD(
      void,
      InitVirtualCardEnroll,
      (const CreditCard& credit_card,
       VirtualCardEnrollmentSource virtual_card_enrollment_source,
       absl::optional<
           payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails>
           get_details_for_enrollment_response_details,
       PrefService* user_prefs,
       VirtualCardEnrollmentManager::RiskAssessmentFunction
           risk_assessment_function,
       VirtualCardEnrollmentManager::VirtualCardEnrollmentFieldsLoadedCallback
           virtual_card_enrollment_fields_loaded_callback),
      (override));
};

class FormDataImporterTestBase {
 public:
  using ExtractedFormData = FormDataImporter::ExtractedFormData;
  using AddressProfileImportCandidate =
      FormDataImporter::AddressProfileImportCandidate;

 protected:
  FormDataImporterTestBase() : autofill_table_(nullptr) {}

  void ResetPersonalDataManager(UserMode user_mode) {
    // Before invalidating the `personal_data_manager_`, the `autofill_client_`s
    // FormDataImporter needs to be reset, because it stores a weak pointer to
    // `personal_data_manager_` that otherwise points to garbage.
    autofill_client_->set_test_form_data_importer(nullptr);

    if (personal_data_manager_) {
      personal_data_manager_->Shutdown();
    }
    personal_data_manager_ =
        std::make_unique<PersonalDataManager>(kLocale, "US");
    personal_data_manager_->set_auto_accept_address_imports_for_testing(true);
    personal_data_manager_->Init(
        scoped_refptr<AutofillWebDataService>(autofill_database_service_),
        /*account_database=*/nullptr,
        /*pref_service=*/prefs_.get(),
        /*local_state=*/prefs_.get(),
        /*identity_manager=*/identity_test_env_.identity_manager(),
        /*history_service=*/nullptr,
        /*sync_service=*/nullptr,
        /*strike_database=*/nullptr,
        /*image_fetcher=*/nullptr,
        /*is_off_the_record=*/(user_mode == USER_MODE_INCOGNITO));
    personal_data_manager_->AddObserver(&personal_data_observer_);

    WaitForOnPersonalDataChanged();

    // Reconstruct the `form_data_importer()` with the new
    // `personal_data_manager_`.
    autofill_client_->set_test_form_data_importer(
        std::make_unique<FormDataImporter>(autofill_client_.get(),
                                           /*payments::PaymentsClient=*/nullptr,
                                           personal_data_manager_.get(),
                                           kLocale));

    auto virtual_card_enrollment_manager =
        std::make_unique<MockVirtualCardEnrollmentManager>(
            nullptr, nullptr, autofill_client_.get());
    virtual_card_enrollment_manager_ = virtual_card_enrollment_manager.get();
    form_data_importer().virtual_card_enrollment_manager_ =
        std::move(virtual_card_enrollment_manager);
  }

  void SetUpHelper() {
    prefs_ = test::PrefServiceForTesting();
    base::FilePath path(WebDatabase::kInMemoryPath);
    web_database_ = new WebDatabaseService(
        path, base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // Hacky: hold onto a pointer but pass ownership.
    autofill_table_ = new AutofillTable;
    web_database_->AddTable(std::unique_ptr<WebDatabaseTable>(autofill_table_));
    web_database_->LoadDatabase();
    autofill_database_service_ = new AutofillWebDataService(
        web_database_, base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    autofill_database_service_->Init(base::NullCallback());

    autofill_client_ = std::make_unique<TestAutofillClient>();
    autofill_client_->set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());

    test::DisableSystemServices(prefs_.get());
    // This will also initialize the `form_data_importer()`.
    ResetPersonalDataManager(USER_MODE_NORMAL);

    // Reset the deduping pref to its default value.
    personal_data_manager_->pref_service_->SetInteger(
        prefs::kAutofillLastVersionDeduped, 0);
  }

  void TearDownHelper() {
    if (personal_data_manager_) {
      personal_data_manager_->Shutdown();
    }
  }

  // Set server credit cards to `autofill_table_` and wait to make sure
  // everything is set up correctly.
  void SetServerCreditCardAndWait(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(autofill_table_, server_cards);
    personal_data_manager_->Refresh();
    WaitForOnPersonalDataChanged();
  }

  FormDataImporter& form_data_importer() {
    return *autofill_client_->GetFormDataImporter();
  }

  // Helper method that will add credit card fields in |form|, according to the
  // specified values. If a value is nullptr, the corresponding field won't get
  // added (empty string will add a field with an empty string as the value).
  void AddFullCreditCardForm(FormData* form,
                             const char* name,
                             const char* number,
                             const char* month,
                             const char* year) {
    FormFieldData field;
    if (name) {
      test::CreateTestFormField("Name on card:", "name_on_card", name, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (number) {
      test::CreateTestFormField("Card Number:", "card_number", number, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (month) {
      test::CreateTestFormField("Exp Month:", "exp_month", month, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (year) {
      test::CreateTestFormField("Exp Year:", "exp_year", year, "text", &field);
      form->fields.push_back(field);
    }
  }

  // Helper method that returns a form with full credit card information.
  FormData CreateFullCreditCardForm(const char* name,
                                    const char* number,
                                    const char* month,
                                    const char* year) {
    FormData form;
    form.url = GURL("https://www.foo.com");
    AddFullCreditCardForm(&form, name, number, month, year);
    return form;
  }

  // Helper methods that simply forward the call to the private member (to avoid
  // having to friend every test that needs to access the private
  // PersonalDataManager::ImportAddressProfile or ExtractCreditCard).
  void ExtractAddressProfiles(bool extraction_successful,
                              const FormStructure& form,
                              bool skip_waiting_on_pdm = false,
                              bool allow_save_prompts = true) {
    std::vector<FormDataImporter::AddressProfileImportCandidate>
        address_profile_import_candidates;

    EXPECT_EQ(extraction_successful,
              form_data_importer().ExtractAddressProfiles(
                  form, &address_profile_import_candidates) > 0);

    if (!extraction_successful) {
      EXPECT_FALSE(form_data_importer().ProcessAddressProfileImportCandidates(
          address_profile_import_candidates, allow_save_prompts));
      return;
    }

    if (skip_waiting_on_pdm) {
      EXPECT_EQ(form_data_importer().ProcessAddressProfileImportCandidates(
                    address_profile_import_candidates, allow_save_prompts),
                allow_save_prompts);
      return;
    }

    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
    EXPECT_EQ(form_data_importer().ProcessAddressProfileImportCandidates(
                  address_profile_import_candidates, allow_save_prompts),
              allow_save_prompts);
    run_loop.Run();
  }

  // Verifies that the stored profiles in the PersonalDataManager equal
  // `expected_profiles` with respect to `AutofillProfile::Compare`.
  // Note, that order is taken into account.
  void VerifyExpectationForExtractedAddressProfiles(
      const std::vector<AutofillProfile>& expected_profiles) {
    EXPECT_THAT(personal_data_manager_->GetProfiles(),
                UnorderedElementsCompareEqualArray(expected_profiles));
  }

  // Convenience wrapper that calls
  // `FormDataImporter::ExtractFormData()` and subsequently
  // processes the candidates for address profile import. Returns the result of
  // `FormDataImporter::ExtractFormData()`.
  ExtractedFormData ExtractFormDataAndProcessAddressCandidates(
      const FormStructure& form,
      bool profile_autofill_enabled,
      bool payment_methods_autofill_enabled) {
    ExtractedFormData extracted_data = form_data_importer().ExtractFormData(
        form, profile_autofill_enabled, payment_methods_autofill_enabled);
    form_data_importer().ProcessAddressProfileImportCandidates(
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
  // `FormDataImporter::ProcessIBANImportCandidate()`.
  bool ExtractFormDataAndProcessIBANCandidates(
      const FormStructure& form,
      bool profile_autofill_enabled,
      bool payment_methods_autofill_enabled) {
    ExtractedFormData extracted_data = form_data_importer().ExtractFormData(
        form, profile_autofill_enabled, payment_methods_autofill_enabled);
    return extracted_data.iban_import_candidate &&
           form_data_importer().ProcessIBANImportCandidate(
               extracted_data.iban_import_candidate.value());
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

  absl::optional<CreditCard> ExtractCreditCard(const FormStructure& form) {
    return form_data_importer().ExtractCreditCard(form);
  }

  void SubmitFormAndExpectImportedCardWithData(const FormData& form,
                                               const char* exp_name,
                                               const char* exp_cc_num,
                                               const char* exp_cc_month,
                                               const char* exp_cc_year) {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(nullptr, nullptr);
    absl::optional<CreditCard> extracted_credit_card =
        ExtractCreditCard(form_structure);
    ASSERT_TRUE(extracted_credit_card);
    personal_data_manager_->OnAcceptedLocalCreditCardSave(
        *extracted_credit_card);

    WaitForOnPersonalDataChanged();
    CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                        test::kEmptyOrigin);
    test::SetCreditCardInfo(&expected, exp_name, exp_cc_num, exp_cc_month,
                            exp_cc_year, "");
    EXPECT_THAT(personal_data_manager_->GetCreditCards(),
                UnorderedElementsCompareEqual(expected));
  }

  void WaitForOnPersonalDataChanged() {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    run_loop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<PrefService> prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<AutofillWebDataService> autofill_database_service_;
  scoped_refptr<WebDatabaseService> web_database_;
  raw_ptr<AutofillTable> autofill_table_;  // weak ref
  PersonalDataLoadedObserverMock personal_data_observer_;
  // `personal_data_manager_` needs to be destroyed before `autofill_client_`,
  // as the destructor of the clients FormDataImporter relies on it.
  std::unique_ptr<PersonalDataManager> personal_data_manager_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  raw_ptr<MockVirtualCardEnrollmentManager> virtual_card_enrollment_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Parameters of the FormDataImporterTest fixture.
using AutofillEnableSupportForApartmentNumbers = bool;
using AutofillFillIbanFields = bool;

class FormDataImporterTest
    : public FormDataImporterTestBase,
      public testing::Test,
      public testing::WithParamInterface<
          std::tuple<AutofillEnableSupportForApartmentNumbers,
                     AutofillFillIbanFields>> {
 public:
  using ExtractedFormData = FormDataImporter::ExtractedFormData;

 private:
  void SetUp() override {
    InitializeFeatures();
    SetUpHelper();
  }

  void TearDown() override { TearDownHelper(); }

  void InitializeFeatures() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    // Always enable parsing IBAN fields from the form.
    enabled_features.push_back(features::kAutofillParseIBANFields);

    (std::get<0>(GetParam()) ? enabled_features : disabled_features)
        .push_back(features::kAutofillEnableSupportForApartmentNumbers);
    (std::get<1>(GetParam()) ? enabled_features : disabled_features)
        .push_back(features::kAutofillFillIbanFields);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
};

// Tests that the country is not complemented if a country is part of the form.
TEST_P(FormDataImporterTest, ComplementCountry_PartOfForm) {
  AutofillProfile kDefaultGermanProfile =
      ConstructDefaultProfileWithOverriddenCountry("DE");
  kDefaultGermanProfile.ClearFields({ADDRESS_HOME_STATE});
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry("Germany"));
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {kDefaultGermanProfile});
}

// Tests that the complemented country prefers the variation country code over
// the app locale (US). The form's country field is left empty.
TEST_P(FormDataImporterTest, ComplementCountry_VariationCountryCode) {
  AutofillProfile kDefaultGermanProfile =
      ConstructDefaultProfileWithOverriddenCountry("DE");
  kDefaultGermanProfile.ClearFields({ADDRESS_HOME_STATE});
  autofill_client_->SetVariationConfigCountryCode("DE");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry(""));
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {kDefaultGermanProfile});
}

// Tests that without a variation country code, the country is complemented by
// the app locale. The form's country field is left empty.
TEST_P(FormDataImporterTest, ComplementCountry_VariationConfigCountryCode) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry(""));
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructDefaultProfileWithOverriddenCountry("US")});
}

// Tests that the country is complemented before parsing the phone number. This
// is important, since the phone number validation relies on the profile's
// country for nationally formatted numbers.
TEST_P(FormDataImporterTest, ComplementCountry_PhoneNumberParsing) {
  // This is a nationally formatted German phone number, which libphonenumber
  // doesn't parse under the "US" region.
  const char* kNationalNumber = "01578 7912345";
  const char* kHistogramName = "Autofill.ProfileImport.PhoneNumberParsed";

  AutofillProfile expected_profile =
      ConstructDefaultProfileWithOverriddenCountry("DE");
  // In Germany, state information is not imported.
  expected_profile.ClearFields({ADDRESS_HOME_STATE});

  // Create an address form with `kNationalNumber` and without a country field.
  TypeValuePairs type_value_pairs =
      GetDefaultProfileTypeValuePairsWithOverriddenCountry("");
  SetValueForType(type_value_pairs, PHONE_HOME_WHOLE_NUMBER, kNationalNumber);
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);

  // The complement country feature prefers the variation country code, so the
  // imported country will have country = "DE" assigned.
  autofill_client_->SetVariationConfigCountryCode("DE");

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

// Tests that invalid countries in submitted forms are ignored, and that the
// complement country logic overwrites it. In this case, expect the country to
// default to the locale's country "US".
TEST_P(FormDataImporterTest, InvalidCountry) {
  // Due to the extra 'A', the country of this `form_structure` is invalid.
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry("USAA"));
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that invalid phone numbers are removed and importing continues.
TEST_P(FormDataImporterTest, InvalidPhoneNumber) {
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, PHONE_HOME_WHOLE_NUMBER, "invalid");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);

  auto profile_without_number = ConstructDefaultProfile();
  profile_without_number.ClearFields({PHONE_HOME_WHOLE_NUMBER});
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {profile_without_number});
}

// ImportAddressProfiles tests.
TEST_P(FormDataImporterTest, ImportStructuredNameProfile) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
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

TEST_P(FormDataImporterTest,
       ImportStructuredAddressProfile_StreetNameAndHouseNumber) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  form.fields.push_back(field);
  test::CreateTestFormField("Street name:", "street_name", "Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("House number:", "house_number", "21", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"21");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Laussat St");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"21 Laussat St");

  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            VerificationStatus::kFormatted);
}

TEST_P(
    FormDataImporterTest,
    ImportStructuredAddressProfile_StreetNameAndHouseNumberAndApartmentNumber) {
  // This test is only applicable for enabled support for apartment numbers.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForApartmentNumbers)) {
    return;
  }
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  form.fields.push_back(field);
  test::CreateTestFormField("Street name:", "street_name", "Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("House number:", "house_number", "21", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Apartment", "apartment", "101", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"21");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Laussat St");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"21 Laussat St APT 101");
  EXPECT_EQ(results[0]->GetRawInfo(ADDRESS_HOME_APT_NUM), u"101");
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kObserved);
  EXPECT_EQ(results[0]->GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS),
            VerificationStatus::kFormatted);
}

TEST_P(FormDataImporterTest,
       ImportStructuredAddressProfile_GermanStreetNameAndHouseNumber) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  form.fields.push_back(field);
  test::CreateTestFormField("Street name:", "street_name", "Hermann Strasse",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("House number:", "house_number", "23", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Munich", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Germany", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "80992", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
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

// ImportAddressProfiles tests.
TEST_P(FormDataImporterTest, ImportStructuredNameAddressProfile) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Pablo Diego Ruiz y Picasso",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractAddressProfiles(/*extraction_successful=*/true, form_structure);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());

  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Pablo Diego Ruiz y Picasso");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Pablo Diego");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Ruiz y Picasso");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_FIRST), u"Ruiz");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_CONJUNCTION), u"y");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST_SECOND), u"Picasso");
}

TEST_P(FormDataImporterTest, ImportAddressProfiles) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_P(FormDataImporterTest, ImportSecondAddressProfiles) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSecondProfileFormStructure();
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {ConstructSecondProfile()});
}

TEST_P(FormDataImporterTest, ImportThirdAddressProfiles) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructThirdProfileFormStructure();
  ExtractAddressProfilesAndVerifyExpectation(*form_structure,
                                             {ConstructThirdProfile()});
}

// Test that with dependent locality parsing enabled, dependent locality fields
// are imported.
TEST_P(FormDataImporterTest, ImportAddressProfiles_DependentLocality) {
  base::test::ScopedFeatureList dependent_locality_feature;
  dependent_locality_feature.InitAndEnableFeature(
      features::kAutofillEnableDependentLocalityParsing);

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
TEST_P(FormDataImporterTest, ImportAddressProfiles_DontAllowPrompt) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  ExtractAddressProfiles(/*extraction_successful=*/true, *form_structure,
                         /*skip_waiting_on_pdm=*/true,
                         /*allow_save_prompts=*/false);
  VerifyExpectationForExtractedAddressProfiles({});
}

TEST_P(FormDataImporterTest, ImportAddressProfileFromUnifiedSection) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();

  // Assign the address field another section than the other fields.
  form_structure->field(4)->section =
      Section::FromAutocomplete({.section = "another_section"});

  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_BadEmail) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();

  // Change the value of the email field.
  ASSERT_EQ(form_structure->field(2)->Type().GetStorableType(), EMAIL_ADDRESS);
  form_structure->field(2)->value = u"bogus";

  // Verify that there was no import.
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that a 'confirm email' field does not block profile import.
TEST_P(FormDataImporterTest, ImportAddressProfiles_TwoEmails) {
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
TEST_P(FormDataImporterTest, ImportAddressProfiles_TwoDifferentEmails) {
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
TEST_P(FormDataImporterTest, ImportAddressProfiles_MultiplePhoneNumbers) {
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
TEST_P(FormDataImporterTest,
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

  form_data.fields[3].max_length = 3;
  form_data.fields[4].max_length = 3;
  form_data.fields[5].max_length = 4;
  form_data.fields[6].max_length = 3;
  form_data.fields[7].max_length = 3;
  form_data.fields[8].max_length = 4;

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
TEST_P(FormDataImporterTest, ImportAddressProfiles_NotEnoughFilledFields) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          {{NAME_FIRST, kDefaultFirstName},
           {NAME_LAST, kDefaultLastName},
           {CREDIT_CARD_NUMBER, kDefaultCreditCardNumber}});

  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
  // Also verify that there was no import of a credit card.
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_MinimumAddressUSA) {
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

TEST_P(FormDataImporterTest, ImportAddressProfiles_MinimumAddressGB) {
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

TEST_P(FormDataImporterTest, ImportAddressProfiles_MinimumAddressGI) {
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

TEST_P(FormDataImporterTest,
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
  form_data.fields[3].max_length = 3;
  form_data.fields[4].max_length = 3;
  form_data.fields[5].max_length = 4;

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
TEST_P(FormDataImporterTest, ImportAddressProfiles_UnFocussableFields) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultProfileFormStructure();
  // Set the Address line field as unfocusable.
  form_structure->field(4)->is_focusable = false;
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_MultilineAddress) {
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

TEST_P(FormDataImporterTest,
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

TEST_P(FormDataImporterTest, ImportAddressProfiles_TwoValidProfilesSameForm) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructShippingAndBillingFormStructure();
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructDefaultProfile(), ConstructSecondProfile()});
}

TEST_P(FormDataImporterTest,
       ImportAddressProfiles_OneValidProfileSameForm_PartsHidden) {
  FormData form_data = ConstructDefaultFormData();

  FormData hidden_second_form = form_data;
  for (FormFieldData& field : hidden_second_form.fields) {
    // Reset the values and make the field non focusable.
    field.value = u"";
    field.is_focusable = false;
  }

  // Append the fields of the second form to the first form.
  form_data.fields.insert(form_data.fields.end(),
                          hidden_second_form.fields.begin(),
                          hidden_second_form.fields.end());

  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form_data);
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_SameProfileWithConflict) {
  TypeValuePairs initial_type_value_pairs{
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      {ADDRESS_HOME_COUNTRY, kDefaultCountry},
      {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneDomesticFormatting},
  };
  AutofillProfile initial_profile =
      ConstructProfileFromTypeValuePairs(initial_type_value_pairs);

  std::unique_ptr<FormStructure> initial_form_structure =
      ConstructFormStructureFromTypeValuePairs(initial_type_value_pairs);
  ExtractAddressProfilesAndVerifyExpectation(*initial_form_structure,
                                             {initial_profile});

  // Create a second form structure with an additional country and a differently
  // formatted phone number
  TypeValuePairs conflicting_type_value_pairs = {
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      // The phone number is spelled differently.
      {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneAlternativeFormatting},
      // Country information is added.
      {ADDRESS_HOME_COUNTRY, "US"}};
  AutofillProfile conflicting_profile =
      ConstructProfileFromTypeValuePairs(conflicting_type_value_pairs);

  // Verify that the initial profile and the conflicting profile are not the
  // same.
  ASSERT_FALSE(initial_profile.Compare(conflicting_profile) == 0);
  std::unique_ptr<FormStructure> conflicting_form_structure =
      ConstructFormStructureFromTypeValuePairs(conflicting_type_value_pairs);

  TypeValuePairs resulting_type_value_pairs{
      {NAME_FULL, kDefaultFullName},
      {ADDRESS_HOME_LINE1, kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, kDefaultCity},
      {ADDRESS_HOME_STATE, kDefaultState},
      {ADDRESS_HOME_ZIP, kDefaultZip},
      // The phone number remains in domestic format.
      {PHONE_HOME_WHOLE_NUMBER, kDefaultPhoneDomesticFormatting},
      // Country information is added.
      {ADDRESS_HOME_COUNTRY, "US"}};

  // Verify that extracting the conflicting profile will result in an update of
  // the existing profile rather than creating a new one.
  ExtractAddressProfilesAndVerifyExpectation(
      *conflicting_form_structure,
      {ConstructProfileFromTypeValuePairs(resulting_type_value_pairs)});
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_MissingInfoInOld) {
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

TEST_P(FormDataImporterTest, ImportAddressProfiles_MissingInfoInNew) {
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
  ExtractAddressProfilesAndVerifyExpectation(*superset_form_structure,
                                             {superset_profile});
}

TEST_P(FormDataImporterTest, ImportAddressProfiles_InsufficientAddress) {
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

// Tests that a profile with an empty name is not imported.
TEST_P(FormDataImporterTest, ImportAddressProfiles_MissingName) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillRequireNameForProfileImport);
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, NAME_FIRST, "");
  SetValueForType(type_value_pairs, NAME_LAST, "");
  ImportAddressProfileAndVerifyImportOfNoProfile(
      *ConstructFormStructureFromTypeValuePairs(type_value_pairs));
}

// Tests that a profile is imported if the country can be translated using the
// page language.
TEST_P(FormDataImporterTest, ImportAddressProfiles_LocalizedCountryName) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry("Armenien"));

  // Set up language state mock.
  autofill_client_->GetLanguageState()->SetSourceLanguage("");
  // Verify that the country code is not determined from the country value if
  // the page language is not set. This results in an import of the default
  // profile.
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
  // Remove the imported profile again, so it doesn't affect the expectation
  // below.
  personal_data_manager_->ClearAllLocalData();

  // Set the page language to match the localized country value and try again.
  autofill_client_->GetLanguageState()->SetSourceLanguage("de");
  ExtractAddressProfilesAndVerifyExpectation(
      *form_structure, {ConstructDefaultProfileWithOverriddenCountry("AM")});
}

// Tests that a profile is created for countries with composed names.
TEST_P(FormDataImporterTest,
       ImportAddressProfiles_CompleteComposedCountryName) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1",
                            "No. 43 Bo Aung Gyaw Street", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Yangon", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "11181", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Myanmar [Burma]", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractAddressProfiles(/*extraction_successful=*/true, form_structure);

  AutofillProfile expected;
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr,
                       "No. 43 Bo Aung Gyaw Street", nullptr, "Yangon", "",
                       "11181", "MM", nullptr);
  EXPECT_THAT(personal_data_manager_->GetProfiles(),
              UnorderedElementsCompareEqual(expected));
}

// TODO(crbug.com/634131): Create profiles if part of a standalone part of a
// composed country name is present. Currently this is treated as an invalid
// country, which is ignored on import.
TEST_P(FormDataImporterTest,
       ImportAddressProfiles_IncompleteComposedCountryName) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(
          GetDefaultProfileTypeValuePairsWithOverriddenCountry(
              "Myanmar"));  // Missing the [Burma] part
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// ExtractCreditCard tests.

// Tests that a valid credit card is extracted.
TEST_P(FormDataImporterTest, ExtractCreditCard_Valid) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  base::HistogramTester histogram_tester;
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure);
  EXPECT_TRUE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->GetCreditCards(),
              UnorderedElementsCompareEqual(expected));
}

// Tests that an invalid credit card number is not extracted.
TEST_P(FormDataImporterTest, ExtractCreditCard_InvalidCardNumber) {
  FormData form = CreateFullCreditCardForm("Jim Johansen", "1000000000000000",
                                           "02", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  base::HistogramTester histogram_tester;
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure);
  EXPECT_FALSE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_EXPIRATION_DATE_ONLY,
                                      1);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

// Tests that a credit card with an empty expiration can be extracted due to the
// expiration date fix flow.
TEST_P(FormDataImporterTest,
       ExtractCreditCard_InvalidExpiryDate_EditableExpirationExpOn) {
  FormData form =
      CreateFullCreditCardForm("Smalls Biggie", "4111-1111-1111-1111", "", "");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  base::HistogramTester histogram_tester;
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure);
  EXPECT_TRUE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_CARD_NUMBER_ONLY, 1);
}

// Tests that an expired credit card can be extracted due to the expiration date
// fix flow.
TEST_P(FormDataImporterTest,
       ExtractCreditCard_ExpiredExpiryDate_EditableExpirationExpOn) {
  FormData form = CreateFullCreditCardForm("Smalls Biggie",
                                           "4111-1111-1111-1111", "01", "2000");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  base::HistogramTester histogram_tester;
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure);
  EXPECT_TRUE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_CARD_NUMBER_ONLY, 1);
}

// Tests that a valid credit card is extracted when the option text for month
// select can't be parsed but its value can.
TEST_P(FormDataImporterTest, ExtractCreditCard_MonthSelectInvalidText) {
  // Add a single valid credit card form with an invalid option value.
  FormData form = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "Feb (2)", "2999");
  // Add option values and contents to the expiration month field.
  ASSERT_EQ(u"exp_month", form.fields[2].name);
  form.fields[2].options = {
      {.value = u"1", .content = u"Jan (1)"},
      {.value = u"2", .content = u"Feb (2)"},
      {.value = u"3", .content = u"Mar (3)"},
  };

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  base::HistogramTester histogram_tester;
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure);
  EXPECT_TRUE(extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  WaitForOnPersonalDataChanged();

  // See that the invalid option text was converted to the right value.
  CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "02",
                          "2999", "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->GetCreditCards(),
              UnorderedElementsCompareEqual(expected));
}

TEST_P(FormDataImporterTest, ExtractCreditCard_TwoValidCards) {
  // Start with a single valid credit card form.
  std::unique_ptr<FormStructure> form_structure1 =
      ConstructDefaultCreditCardFormStructure();
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(*form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second different valid credit card.
  FormData form2 =
      CreateFullCreditCardForm("", "5500 0000 0000 0004", "02", "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);

  absl::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(form_structure2);
  EXPECT_TRUE(extracted_credit_card2);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(
      *extracted_credit_card2);

  WaitForOnPersonalDataChanged();

  CreditCard expected2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                       test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "", "5500000000000004", "02", "2999",
                          "");  // Imported cards have no billing info.
  // We ignore the order because multiple profiles or credit cards that
  // are added to the SQLite DB within the same second will be returned in GUID
  // (i.e., random) order.
  EXPECT_THAT(personal_data_manager_->GetCreditCards(),
              UnorderedElementsCompareEqual(expected, expected2));
}

// This form has the expiration year as one field with MM/YY.
TEST_P(FormDataImporterTest, ExtractCreditCard_Month2DigitYearCombination) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John MMYY",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Date:", "exp_date", "05/45", "text", "cc-exp",
                            5, &field);
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John MMYY", "4111111111111111",
                                          "05", "2045");
}

// This form has the expiration year as one field with MM/YYYY.
TEST_P(FormDataImporterTest, ExtractCreditCard_Month4DigitYearCombination) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John MMYYYY",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Date:", "exp_date", "05/2045", "text",
                            "cc-exp", 7, &field);
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John MMYYYY",
                                          "4111111111111111", "05", "2045");
}

// This form has the expiration year as one field with M/YYYY.
TEST_P(FormDataImporterTest, ExtractCreditCard_1DigitMonth4DigitYear) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John MYYYY",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Date:", "exp_date", "5/2045", "text", "cc-exp",
                            &field);
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John MYYYY",
                                          "4111111111111111", "05", "2045");
}

// This form has the expiration year as a 2-digit field.
TEST_P(FormDataImporterTest, ExtractCreditCard_2DigitYear) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John Smith",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "05", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "45", "text", &field);
  field.max_length = 2;
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John Smith",
                                          "4111111111111111", "05", "2045");
}

// Tests that a credit card is extracted when the card matches a masked server
// card.
TEST_P(FormDataImporterTest,
       ExtractCreditCard_DuplicateServerCards_ExtractMaskedCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Type the same data as the masked card into a form.
  FormData form = CreateFullCreditCardForm("John Dillinger", "4111111111111111",
                                           "01", "2999");

  // The card should not be offered to be saved locally because the feature flag
  // is disabled.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure);
  EXPECT_TRUE(extracted_credit_card);
  ASSERT_TRUE(extracted_credit_card.value().record_type() ==
              CreditCard::MASKED_SERVER_CARD);
}

// Tests that a credit card is extracted when it matches a full server
// card.
TEST_P(FormDataImporterTest,
       ExtractCreditCard_DuplicateServerCards_ExtractFullCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "");  // Imported cards have no billing info.
  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Type the same data as the unmasked card into a form.
  FormData form =
      CreateFullCreditCardForm("Clyde Barrow", "378282246310005", "04", "2999");

  // The card should not be offered to be saved locally because it only matches
  // the full server card.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure);
  EXPECT_TRUE(extracted_credit_card);
  EXPECT_EQ(extracted_credit_card.value().record_type(),
            CreditCard::RecordType::FULL_SERVER_CARD);
}

TEST_P(FormDataImporterTest, ExtractCreditCard_SameCreditCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2 =
      CreateFullCreditCardForm("Biggie Smalls", "4111 1111 1111 1111", "01",
                               /* different year */ "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(form_structure2);
  EXPECT_TRUE(extracted_credit_card2);

  WaitForOnPersonalDataChanged();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2999".
  CreditCard expected2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                       test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

TEST_P(FormDataImporterTest, ExtractCreditCard_ShouldReturnLocalCard) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2 =
      CreateFullCreditCardForm("Biggie Smalls", "4111 1111 1111 1111", "01",
                               /* different year */ "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(form_structure2);
  EXPECT_TRUE(extracted_credit_card2);
  // The local card is returned after an update.
  EXPECT_TRUE(extracted_credit_card2);

  WaitForOnPersonalDataChanged();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2999".
  CreditCard expected2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                       test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

TEST_P(FormDataImporterTest, ExtractCreditCard_EmptyCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2998");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(nullptr, nullptr);

  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  EXPECT_THAT(personal_data_manager_->GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second credit card with no number.
  FormData form2 =
      CreateFullCreditCardForm("Biggie Smalls",
                               /* no number */ nullptr, "01", "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(form_structure2);
  EXPECT_FALSE(extracted_credit_card2);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                       test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

TEST_P(FormDataImporterTest, ExtractCreditCard_MissingInfoInNew) {
  // Start with a single valid credit card form.
  FormData form1 = CreateFullCreditCardForm(
      "Biggie Smalls", "4111-1111-1111-1111", "01", "2999");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure1);
  EXPECT_TRUE(extracted_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*extracted_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");
  EXPECT_THAT(personal_data_manager_->GetCreditCards(),
              UnorderedElementsCompareEqual(expected));

  // Add a second different valid credit card where the name is missing but
  // the credit card number matches.
  FormData form2 = CreateFullCreditCardForm(
      /* missing name */ nullptr, "4111-1111-1111-1111", "01", "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card2 =
      ExtractCreditCard(form_structure2);
  EXPECT_TRUE(extracted_credit_card2);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                       test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));

  // Add a third credit card where the expiration date is missing.
  FormData form3 =
      CreateFullCreditCardForm("Johnny McEnroe", "5555555555554444",
                               /* no month */ nullptr,
                               /* no year */ nullptr);

  FormStructure form_structure3(form3);
  form_structure3.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card3 =
      ExtractCreditCard(form_structure3);
  EXPECT_FALSE(extracted_credit_card3);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                       test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected3, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");
  const std::vector<CreditCard*>& results3 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results3.size());
  EXPECT_THAT(*results3[0], ComparesEqual(expected3));
}

TEST_P(FormDataImporterTest, ExtractCreditCard_MissingInfoInOld) {
  // Start with a single valid credit card stored via the preferences.
  // Note the empty name.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "", "4111111111111111" /* Visa */,
                          "01", "2998", "1");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results1 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(saved_credit_card, *results1[0]);

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form =
      CreateFullCreditCardForm("Biggie Smalls", "4111-1111-1111-1111", "01",
                               /* different year */ "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure);
  EXPECT_TRUE(extracted_credit_card);

  WaitForOnPersonalDataChanged();

  // Expect that the newer information is saved.  In this case the year is
  // added to the existing credit card.
  CreditCard expected2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                       test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "1");
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(expected2));
}

// We allow the user to store a credit card number with separators via the UI.
// We should not try to re-aggregate the same card with the separators stripped.
TEST_P(FormDataImporterTest, ExtractCreditCard_SameCardWithSeparators) {
  // Start with a single valid credit card stored via the preferences.
  // Note the separators in the credit card number.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results1 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_THAT(*results1[0], ComparesEqual(saved_credit_card));

  // Import the same card info, but with different separators in the number.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111-1111-1111-1111", "01", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure);
  EXPECT_TRUE(extracted_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that no new card is saved.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_THAT(*results2[0], ComparesEqual(saved_credit_card));
}

// Ensure that if a verified credit card already exists, aggregated credit cards
// cannot modify it in any way.
TEST_P(FormDataImporterTest,
       ExtractCreditCard_ExistingVerifiedCardWithConflict) {
  // Start with a verified credit card.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2998", "");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_manager_->AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with conflicting expiration year.
  FormData form =
      CreateFullCreditCardForm("Biggie Smalls", "4111 1111 1111 1111", "01",
                               /* different year */ "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  absl::optional<CreditCard> extracted_credit_card =
      ExtractCreditCard(form_structure);
  EXPECT_TRUE(extracted_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that the saved credit card is not modified.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(credit_card));
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set and
// reset correctly.
TEST_P(FormDataImporterTest,
       ExtractFormData_SecondImportResetsCreditCardRecordType) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(saved_credit_card));

  // Simulate a form submission with the same card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kLocalCard because
  // upload was offered and the card is a local card already on the device.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kLocalCard);

  // Second form is filled with a new card so
  // `FormDataImporterTest::credit_card_import_type_` should be
  // reset. Simulate a form submission with a new card.
  FormData form2 = CreateFullCreditCardForm("Biggie Smalls", "4012888888881881",
                                            "01", "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data2 = ExtractFormDataAndProcessAddressCandidates(
      form_structure2, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data2.extracted_credit_card);
  // |credit_card_import_type_| should be kNewCard because the
  // imported card is not already on the device.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kNewCard);

  // Third form is an address form and set `payment_methods_autofill_enabled` to
  // be false so that the ExtractCreditCard won't be called.
  // `FormDataImporterTest::credit_card_import_type_` should
  // still be reset even if ExtractCreditCard is not called. Simulate a form
  // submission with no card.
  FormData form3;
  form3.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "bogus@example.com", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form3.fields.push_back(field);
  FormStructure form_structure3(form3);
  form_structure3.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data3 = ExtractFormDataAndProcessAddressCandidates(
      form_structure3, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/false);
  // |credit_card_import_type_| should be NO_CARD because no
  // valid card was imported from the form.
  EXPECT_NE(0u, extracted_data3.address_profile_import_candidates.size());
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_P(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NewCard) {
  // Simulate a form submission with a new credit card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNewCard because the
  // imported card is not already on the device.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kNewCard);
}

// Ensures that `credit_card_import_type_` is set correctly.
TEST_P(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_LocalCard) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(saved_credit_card));

  // Simulate a form submission with the same card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kLocalCard because
  // upload was offered and the card is a local card already on the device.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kLocalCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_P(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_MaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "Biggie Smalls",
                          "1111" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with the same masked server card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be SERVER_CARD.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kServerCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_P(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_FullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Biggie Smalls",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with the same full server card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls", "378282246310005",
                                           "04", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be SERVER_CARD.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kServerCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_P(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NoCard_InvalidCardNumber) {
  // Simulate a form submission using a credit card with an invalid card number.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1112", "01", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNoCard because no
  // valid card was successfully imported from the form.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_P(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NoCard_VirtualCard) {
  // Simulate a form submission using a credit card that is known as a virtual
  // card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "2999");
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  form_data_importer().CacheFetchedVirtualCard(u"1111");
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNoCard because the
  // card imported from the form was a virtual card.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_P(
    FormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_NewCard_ExpiredCard_WithExpDateFixFlow) {
  // Simulate a form submission with an expired credit card.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "01", "1999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNewCard because card
  // was successfully imported from the form via the expiration date fix flow.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kNewCard);
}

// Ensures that
// `FormDataImporterTest::credit_card_import_type_` is set
// correctly.
TEST_P(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_NoCard_NoCardOnForm) {
  // Simulate a form submission with no credit card on form.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "bogus@example.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
  // |credit_card_import_type_| should be kNoCard because the
  // form doesn't have credit card section.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kNoCard);
}

// Ensures that `credit_card_import_type_` is set as kServerCard when the flag
// `kAutofillOfferToSaveCardWithSameLastFour` is turned on and a full server
// card is found with the same number.
TEST_P(FormDataImporterTest,
       ExtractFormData_ExtractCreditCardRecordType_ServerCardWithSameLastFour) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillOfferToSaveCardWithSameLastFour);
  // Add a valid server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::FULL_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  SetServerCreditCardAndWait(server_cards);
  ASSERT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with the same card number but different
  // expiration date.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "02", "2999");
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // `credit_card_import_type_` should be kServerCard because a server card with
  // the same card number was found.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kServerCard);
}

// Ensures that `credit_card_import_type_` is set as kNewCard when the flag
// `kAutofillOfferToSaveCardWithSameLastFour` is turned on when there is a
// masked server card with the same last four but different expiration date.
TEST_P(
    FormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_MaskedServerCardWithSameLastFour) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillOfferToSaveCardWithSameLastFour);
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  SetServerCreditCardAndWait(server_cards);
  ASSERT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with the card with same last four but different
  // expiration date.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "02", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  ASSERT_EQ(extracted_data.extracted_credit_card->expiration_month(), 2);
  // `credit_card_import_type_` should be kNewCard because a server card with
  // the same card number was found, but they have different expiration date.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
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
// 2. The extracted credit card's expiraion date is not the same as any of the
//    the masked server cards.
TEST_P(
    FormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_TwoMaskedServerCardWithSameLastFour) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillOfferToSaveCardWithSameLastFour);
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2111", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "a124");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "02", "2112", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(2U, personal_data_manager_->GetCreditCards().size());

  {
    // A user fills/enters the card's information on a checkout form but changes
    // the expiration date of the card. Ensure that an expiration date mismatch
    // is recorded.
    FormData form = CreateFullCreditCardForm(
        "Clyde Barrow", "4444 3333 2222 1111", "04", "2345");

    base::HistogramTester histogram_tester;
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(nullptr, nullptr);
    auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
        form_structure, /*profile_autofill_enabled=*/true,
        /*payment_methods_autofill_enabled=*/true);
    ASSERT_TRUE(extracted_data.extracted_credit_card);
    // `credit_card_import_type_` should be kNewCard because a masked server
    // card with the same card number was found, but they have different
    // expiration date.
    ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
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
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(nullptr, nullptr);
    auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
        form_structure, /*profile_autofill_enabled=*/true,
        /*payment_methods_autofill_enabled=*/true);
    ASSERT_TRUE(extracted_data.extracted_credit_card);
    ASSERT_TRUE(
        extracted_data.extracted_credit_card->Compare(server_cards[1]) == 0);
    // `credit_card_import_type_` should be kServerCard because a masked server
    // card with the same card number and expiration date was found.
    ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
                FormDataImporter::CreditCardImportType::kServerCard);
    histogram_tester.ExpectUniqueSample(
        "Autofill.SubmittedServerCardExpirationStatus",
        AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
  }
}

// Ensures that `credit_card_import_type_` is set as kServerCard and the first
// card is matched when the flag `kAutofillOfferToSaveCardWithSameLastFour `is
// turned off and there are two masked server card with the same last four
// saved, where the second one has a matching expiration date.
TEST_P(
    FormDataImporterTest,
    ExtractFormData_ExtractCreditCardRecordType_TwoMaskedServerCardWithSameLastFour_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillOfferToSaveCardWithSameLastFour);
  // Add two masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "04", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  SetServerCreditCardAndWait(server_cards);
  ASSERT_EQ(2U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with the card with same last four but different
  // expiration date.
  FormData form = CreateFullCreditCardForm("Biggie Smalls",
                                           "4111 1111 1111 1111", "04", "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  ASSERT_EQ(extracted_data.extracted_credit_card->Compare(server_cards[0]), 0);
  // `credit_card_import_type_` should be kNewCard because a server card with
  // the same card number was found, but they have different expiration date.
  ASSERT_TRUE(form_data_importer().credit_card_import_type_for_testing() ==
              FormDataImporter::CreditCardImportType::kServerCard);
}

// ExtractFormData tests (both addresses and credit cards).

// Test that a form with both address and credit card sections imports the
// address and the credit card.
TEST_P(FormDataImporterTest, ExtractFormData_OneAddressOneCreditCard) {
  FormData form = ConstructDefaultFormData();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that the address has been saved.
  AutofillProfile expected_address = ConstructDefaultProfile();
  const std::vector<AutofillProfile*>& results_addr =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_THAT(*results_addr[0], ComparesEqual(expected_address));

  // Test that the credit card has also been saved.
  CreditCard expected_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results_cards.size());
  EXPECT_THAT(*results_cards[0], ComparesEqual(expected_card));
}

// Test that a form with two address sections and a credit card section does not
// import the address but does import the credit card.
TEST_P(FormDataImporterTest, ExtractFormData_TwoAddressesOneCreditCard) {
  FormData form = ConstructDefaultFormDataWithTwoAddresses();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillRepeatedly(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .Times(testing::AnyNumber());
  // Still returns true because the credit card import was successful.
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  run_loop.Run();

  ASSERT_TRUE(extracted_data.extracted_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that both addresses have been saved.
  EXPECT_EQ(2U, personal_data_manager_->GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(expected_card));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_P(FormDataImporterTest, ExtractFormData_ImportIbanRecordType_NoIban) {
  // Simulate a form submission with no IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.iban_import_candidate);
}

TEST_P(FormDataImporterTest, ExtractFormData_SubmittingIbanFormUpdatesPref) {
  // Simulate a form submission with a new IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");
  // The pref should always start disabled.
  ASSERT_FALSE(personal_data_manager_->IsAutofillHasSeenIbanPrefEnabled());

  test::CreateTestIbanFormData(&form);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  if (base::FeatureList::IsEnabled(features::kAutofillFillIbanFields)) {
    // Submitting the IBAN form permanently enables the pref.
    EXPECT_TRUE(personal_data_manager_->IsAutofillHasSeenIbanPrefEnabled());
  } else {
    // With the IBAN feature disabled, its form submission is not detected, and
    // the pref remains unchanged.
    EXPECT_FALSE(personal_data_manager_->IsAutofillHasSeenIbanPrefEnabled());
  }
}

TEST_P(FormDataImporterTest,
       ExtractFormData_SubmittingCreditCardFormDoesNotUpdateIbanPref) {
  ASSERT_FALSE(personal_data_manager_->IsAutofillHasSeenIbanPrefEnabled());
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // Submitting the credit card form won't enable the pref, even if the flag is
  // on.
  EXPECT_FALSE(personal_data_manager_->IsAutofillHasSeenIbanPrefEnabled());
}

TEST_P(FormDataImporterTest,
       ExtractFormData_ImportIbanRecordType_IbanAutofill_NewInvalidIban) {
  // Simulate a form submission with a new IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");

  // Invalid Kuwait IBAN with incorrect IBAN length.
  // KW16 will be converted into 203216, and the remainder on 97 is 1.
  test::CreateTestIbanFormData(&form, "KW1600000000000000000");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);

  // IBAN candidate is empty as the value is invalid.
  ASSERT_FALSE(extracted_data.iban_import_candidate);
}

TEST_P(FormDataImporterTest,
       ExtractFormData_ImportIbanRecordType_IbanAutofill_NewIban) {
  // Simulate a form submission with a new IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");

  test::CreateTestIbanFormData(&form);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  if (base::FeatureList::IsEnabled(features::kAutofillFillIbanFields)) {
    ASSERT_TRUE(extracted_data.iban_import_candidate);
  } else {
    ASSERT_FALSE(extracted_data.iban_import_candidate);
  }
}

TEST_P(FormDataImporterTest, ExtractFormData_ImportIbanRecordType_LocalIban) {
  IBAN iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data_manager_->AddIBAN(iban);

  WaitForOnPersonalDataChanged();

  const std::vector<IBAN*>& results = personal_data_manager_->GetLocalIBANs();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(iban));

  // Simulate a form submission with the same IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");

  test::CreateTestIbanFormData(&form);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.iban_import_candidate);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Test that a form with both address and credit card sections imports only the
// the credit card if addresses are disabled.
TEST_P(FormDataImporterTest, ExtractFormData_AddressesDisabledOneCreditCard) {
  FormData form = ConstructDefaultFormData();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/false,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that addresses were not saved.
  EXPECT_EQ(0U, personal_data_manager_->GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(expected_card));
}

// Test that a form with both address and credit card sections imports only the
// the address if credit cards are disabled.
TEST_P(FormDataImporterTest, ExtractFormData_OneAddressCreditCardDisabled) {
  FormData form = ConstructDefaultFormData();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/false);
  ASSERT_FALSE(extracted_data.extracted_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that the address has been saved.
  AutofillProfile expected_address = ConstructDefaultProfile();
  const std::vector<AutofillProfile*>& results_addr =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_THAT(*results_addr[0], ComparesEqual(expected_address));

  // Test that the credit card was not saved.
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(0U, results_cards.size());
}

// Test that a form with both address and credit card sections imports nothing
// if both addressed and credit cards are disabled.
TEST_P(FormDataImporterTest, ExtractFormData_AddressCreditCardDisabled) {
  FormData form = ConstructDefaultFormData();
  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/false,
      /*payment_methods_autofill_enabled=*/false);
  ASSERT_FALSE(extracted_data.extracted_credit_card);

  // Test that addresses were not saved.
  EXPECT_EQ(0U, personal_data_manager_->GetProfiles().size());

  // Test that the credit card was not saved.
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(0U, results_cards.size());
}

TEST_P(FormDataImporterTest, DuplicateMaskedServerCard) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1881" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "");

  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(2U, personal_data_manager_->GetCreditCards().size());

  // A valid credit card form. A user re-enters one of their masked cards.
  // We should not offer to save locally.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John Dillinger",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4012888888881881",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2999", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
}

// Tests that a credit card form that is hidden after receiving input still
// imports the card.
TEST_P(FormDataImporterTest, ExtractFormData_HiddenCreditCardFormAfterEntered) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;

  test::CreateTestFormField("Name on card:", "name_on_card", "Biggie Smalls",
                            "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2999", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(
      *extracted_data.extracted_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that the credit card has been saved.
  CreditCard expected_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(*results[0], ComparesEqual(expected_card));
}

// Ensures that no UPI ID value is returned when there's a credit card and no
// UPI ID.
TEST_P(FormDataImporterTest,
       ExtractFormData_DontSetUpiIdWhenOnlyCreditCardExists) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      *form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  ASSERT_FALSE(extracted_data.extracted_upi_id.has_value());
}

TEST_P(FormDataImporterTest,
       DuplicateFullServerCardWhileContainingLocalCardCopies) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1881" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");

  SetServerCreditCardAndWait(server_cards);

  // Add two local cards to the credit cards to ensure that in the case where we
  // have separate copies of a server card and a local card, we still only set
  // |imported_credit_card| to the server card details as we want the server
  // to be the source of truth. Adding two cards also helps us ensure that we
  // will update both.
  for (int i = 0; i < 2; i++) {
    CreditCard local_card = test::GetCreditCard();
    test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                            "378282246310005" /* American Express */, "05",
                            "2999", "1");
    local_card.set_record_type(CreditCard::RecordType::LOCAL_CARD);
    personal_data_manager_->AddCreditCard(local_card);
  }

  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(4U, personal_data_manager_->GetCreditCards().size());

  // A user re-types (or fills with) an unmasked card. Don't offer to save
  // here, either. Since it's unmasked, we know for certain that it's the same
  // card.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "378282246310005",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2999", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  // Ensure that we imported the server version of the card, not the local
  // version.
  ASSERT_TRUE(extracted_data.extracted_credit_card->record_type() ==
              CreditCard::FULL_SERVER_CARD);

  // Check that both of the local cards we have added were updated.
  int matched_local_cards = 0;
  for (const CreditCard* card : personal_data_manager_->GetCreditCards()) {
    if (card->record_type() == CreditCard::RecordType::LOCAL_CARD) {
      matched_local_cards++;
      EXPECT_EQ(card->expiration_month(), 4);
    }
  }
  EXPECT_EQ(matched_local_cards, 2);
}

TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_FullServerCardMatch) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form.  Ensure that
  // an expiration date match is recorded.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2111", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date month.
TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationMonth) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2111", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date year.
TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationYear) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "08", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_FALSE(extracted_data.extracted_credit_card);
}

// Ensure that we still offer to save if we have different cards stored as a
// server card and user submitted an invalid expiration date year.
TEST_P(
    FormDataImporterTest,
    Metrics_SubmittedDifferentServerCardExpirationStatus_EmptyExpirationYear) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4111111111111111" /* Visa */, "04", "2111", "1");

  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "08", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
}

TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_FullServerCardMismatch) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form but changes
  // the expiration date of the card.  Ensure that an expiration date mismatch
  // is recorded.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2345", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH, 1);
}

TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMatch) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2111", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form.  Ensure that
  // an expiration date match is recorded.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2111", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
}

TEST_P(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMismatch) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2111", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCreditCardAndWait(server_cards);
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form but changes
  // the expiration date of the card.  Ensure that an expiration date mismatch
  // is recorded.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2345", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH, 1);
}

TEST_P(FormDataImporterTest, ExtractUpiId) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "user@indianbank", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/false,
      /*payment_methods_autofill_enabled=*/true);
  ASSERT_TRUE(extracted_data.extracted_upi_id.has_value());
  EXPECT_EQ(extracted_data.extracted_upi_id.value(), "user@indianbank");
}

TEST_P(FormDataImporterTest, ExtractUpiIdDisabled) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "user@indianbank", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/false,
      /*payment_methods_autofill_enabled=*/false);
  ASSERT_FALSE(extracted_data.extracted_upi_id.has_value());
}

TEST_P(FormDataImporterTest, ExtractUpiIdIgnoreNonUpiId) {
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "user@gmail.com", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  auto extracted_data = ExtractFormDataAndProcessAddressCandidates(
      form_structure, /*profile_autofill_enabled=*/false,
      /*payment_methods_autofill_enabled=*/false);
  ASSERT_FALSE(extracted_data.extracted_upi_id.has_value());
}

TEST_P(FormDataImporterTest, SilentlyUpdateExistingProfileByIncompleteProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillSilentProfileUpdateForInsufficientImport);

  AutofillProfile profile;
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

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Middle name:", "middle_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Mitchell Morrison",
                            "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractAddressProfiles(/*extraction_successful=*/false, form_structure);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_NE(0, profile.Compare(*results[0]));
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Marion Mitchell Morrison");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Marion");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Mitchell Morrison");
}

TEST_P(
    FormDataImporterTest,
    SilentlyUpdateExistingProfileByIncompleteProfile_DespiteDisallowedPrompts) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillSilentProfileUpdateForInsufficientImport);

  AutofillProfile profile;
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

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Middle name:", "middle_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Mitchell Morrison",
                            "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractAddressProfiles(/*extraction_successful=*/false, form_structure,
                         /*skip_waiting_on_pdm=*/false,
                         /*allow_save_prompts=*/false);

  // Expect that no new profile is saved and the existing profile is updated.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_NE(0, profile.Compare(*results[0]));
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FULL), u"Marion Mitchell Morrison");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_FIRST), u"Marion");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_MIDDLE), u"");
  EXPECT_EQ(results[0]->GetRawInfo(NAME_LAST), u"Mitchell Morrison");
}

TEST_P(FormDataImporterTest, UnusableIncompleteProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillSilentProfileUpdateForInsufficientImport);

  AutofillProfile profile;
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

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://www.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Middle name:", "middle_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Mitch Morrison", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
  ExtractAddressProfiles(/*extraction_successful=*/false, form_structure,
                         /*skip_waiting_on_pdm=*/true);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
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
TEST_P(FormDataImporterTest, RemoveInaccessibleProfileValuesMetrics) {
  // State is setting-inaccessible in Germany. Expect that when importing a
  // German profile with a state, the state information is removed.
  TypeValuePairs type_value_pairs =
      GetDefaultProfileTypeValuePairsWithOverriddenCountry("DE");
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
TEST_P(FormDataImporterTest, MultiStepImport) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSplitDefaultProfileFormStructure(/*part=*/1);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  form_structure = ConstructSplitDefaultProfileFormStructure(/*part=*/2);
  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that a complemented country is discarded in favour of an observed one.
TEST_P(FormDataImporterTest, MultiStepImport_ComplementCountryEarly) {
  // Import a profile fragment with country information.
  TypeValuePairs type_value_pairs =
      GetSplitDefaultProfileTypeValuePairs(/*part=*/1);
  EXPECT_TRUE(base::Contains(
      type_value_pairs,
      std::pair<ServerFieldType, std::string>(ADDRESS_HOME_COUNTRY, "US")));
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  // Now import a profile without a country. The country is thus be complemented
  // to the variation country "DE".
  autofill_client_->SetVariationConfigCountryCode("DE");
  type_value_pairs = GetSplitDefaultProfileTypeValuePairs(/*part=*/2);
  EXPECT_FALSE(base::Contains(type_value_pairs, ADDRESS_HOME_COUNTRY,
                              [](auto& pair) { return pair.first; }));
  form_structure = ConstructFormStructureFromTypeValuePairs(type_value_pairs);

  ExtractAddressProfileAndVerifyExtractionOfDefaultProfile(*form_structure);
}

// Tests that when multi-step complements are enabled, complete profiles those
// import was accepted are added as a multi-step candidate. This enables
// complementing the profile with additional information on further pages.
TEST_P(FormDataImporterTest, MultiStepImport_Complement) {
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
TEST_P(FormDataImporterTest, MultiStepImport_Complement_ExternalUpdate) {
  // Extract the default profile without an email address.
  TypeValuePairs type_value_pairs = GetDefaultProfileTypeValuePairs();
  SetValueForType(type_value_pairs, EMAIL_ADDRESS, "");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromTypeValuePairs(type_value_pairs);
  ExtractFormDataAndProcessAddressCandidates(*form_structure);
  VerifyExpectationForExtractedAddressProfiles(
      {ConstructProfileFromTypeValuePairs(type_value_pairs)});

  // Update the profile's ZIP through external means.
  AutofillProfile profile = *personal_data_manager_->GetProfiles()[0];
  profile.SetInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"12345", kLocale,
                                        VerificationStatus::kObserved);
  personal_data_manager_->UpdateProfile(profile);
  WaitForOnPersonalDataChanged();

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
TEST_P(FormDataImporterTest, MultiStepImport_Complement_ExternalRemove) {
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
      personal_data_manager_->GetProfiles()[0]->guid());
  WaitForOnPersonalDataChanged();

  // Expect that the removed profile cannot be updated with an email address.
  form_structure = ConstructDefaultEmailFormStructure();
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that multi-step candidate profiles from different origins are not
// merged.
TEST_P(FormDataImporterTest, MultiStepImport_DifferentOrigin) {
  FormData form = ConstructSplitDefaultFormData(/*part=*/1);
  form.url = GURL("https://www.foo.com");
  std::unique_ptr<FormStructure> form_structure =
      ConstructFormStructureFromFormData(form);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  form = ConstructSplitDefaultFormData(/*part=*/2);
  form.url = GURL("https://wwww.bar.com");
  form_structure = ConstructFormStructureFromFormData(form);
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that multi-step candidates profiles are invalidated after some TTL.
TEST_P(FormDataImporterTest, MultiStepImport_TTL) {
  TestAutofillClock test_clock;

  std::unique_ptr<FormStructure> form_structure =
      ConstructSplitDefaultProfileFormStructure(/*part=*/1);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  test_clock.Advance(kMultiStepImportTTL + base::Minutes(1));

  form_structure = ConstructSplitDefaultProfileFormStructure(/*part=*/2);
  ImportAddressProfileAndVerifyImportOfNoProfile(*form_structure);
}

// Tests that multi-step candidates profiles are cleared if the browsing history
// is deleted.
TEST_P(FormDataImporterTest, MultiStepImport_DeleteOnBrowsingHistoryCleared) {
  std::unique_ptr<FormStructure> form_structure =
      ConstructSplitDefaultProfileFormStructure(/*part=*/1);
  ExtractAddressProfilesAndVerifyExpectation(*form_structure, {});

  personal_data_manager_->OnURLsDeleted(
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
TEST_P(FormDataImporterTest, FormAssociator) {
  base::test::ScopedFeatureList form_association_feature;
  form_association_feature.InitAndEnableFeature(
      features::kAutofillAssociateForms);

  std::unique_ptr<FormStructure> form_structure =
      ConstructShippingAndBillingFormStructure();
  FormSignature form_signature = form_structure->form_signature();
  // Don't use `ExtractAddressProfileAndVerifyExtractionOfDefaultProfile()`, as
  // this function assumes we know it's an address form already. Form
  // associations are tracked in `ExtractFormData()` instead.
  ExtractFormDataAndProcessAddressCandidates(*form_structure);

  absl::optional<FormStructure::FormAssociations> associations =
      form_data_importer().GetFormAssociations(form_signature);
  // Expect the same form signature for the two most recent address form, as
  // `form_structure` consists of two sections.
  EXPECT_TRUE(associations);
  EXPECT_EQ(associations->last_address_form_submitted, form_signature);
  EXPECT_EQ(associations->second_last_address_form_submitted, form_signature);
  EXPECT_FALSE(associations->last_credit_card_form_submitted);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_P(FormDataImporterTest,
       ProcessIBANImportCandidate_ShouldOfferLocalSave_NewIBAN) {
  IBAN iban_import_candidate = test::GetIBAN();

  EXPECT_EQ(
      base::FeatureList::IsEnabled(features::kAutofillFillIbanFields),
      form_data_importer().ProcessIBANImportCandidate(iban_import_candidate));
}

TEST_P(FormDataImporterTest,
       ExtractFormData_ProcessIBANImportCandidate_NoIban) {
  // Simulate a form submission with a new IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");

  test::CreateTestIbanFormData(&form, "");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  ASSERT_FALSE(ExtractFormDataAndProcessIBANCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}

TEST_P(
    FormDataImporterTest,
    ExtractFormData_ProcessIBANImportCandidate_PaymentMethodsSettingDisabled) {
  // Simulate a form submission with a new IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");

  test::CreateTestIbanFormData(&form);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  ASSERT_FALSE(ExtractFormDataAndProcessIBANCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/false));
}

TEST_P(FormDataImporterTest,
       ExtractFormData_ProcessIBANImportCandidate_NewIban) {
  // Simulate a form submission with a new IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");

  test::CreateTestIbanFormData(&form);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  ASSERT_EQ(base::FeatureList::IsEnabled(features::kAutofillFillIbanFields),
            ExtractFormDataAndProcessIBANCandidates(
                form_structure, /*profile_autofill_enabled=*/true,
                /*payment_methods_autofill_enabled=*/true));
}

TEST_P(FormDataImporterTest,
       ExtractFormData_ProcessIBANImportCandidate_LocalIban) {
  IBAN iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data_manager_->AddIBAN(iban);

  WaitForOnPersonalDataChanged();
  // Simulate a form submission with a new IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");

  test::CreateTestIbanFormData(&form);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  ASSERT_FALSE(ExtractFormDataAndProcessIBANCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}

TEST_P(FormDataImporterTest,
       ExtractFormData_ProcessIBANImportCandidate_MaxStrikes) {
  IBANSaveStrikeDatabase iban_save_strike_database =
      IBANSaveStrikeDatabase(autofill_client_->GetStrikeDatabase());

  iban_save_strike_database.AddStrikes(
      iban_save_strike_database.GetMaxStrikesLimit(),
      IBANSaveManager::GetPartialIbanHashString(
          test::GetStrippedValue(test::kIbanValue)));

  // Simulate a form submission with a new IBAN.
  FormData form;
  form.url = GURL("https://www.foo.com");

  test::CreateTestIbanFormData(&form);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  ASSERT_FALSE(ExtractFormDataAndProcessIBANCandidates(
      form_structure, /*profile_autofill_enabled=*/true,
      /*payment_methods_autofill_enabled=*/true));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Runs the suite with the features `kAutofillEnableSupportForApartmentNumbers`
// and `kAutofillFillIbanFields` enabled and disabled.
INSTANTIATE_TEST_SUITE_P(,
                         FormDataImporterTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class FormDataImporterNonParameterizedTest : public FormDataImporterTestBase,
                                             public testing::Test {
 private:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }
};

TEST_F(FormDataImporterNonParameterizedTest,
       ProcessExtractedCreditCard_EmptyCreditCard) {
  absl::optional<CreditCard> extracted_credit_card;
  absl::optional<std::string> extracted_upi_id;
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();

  // `form_data_importer()`'s `credit_card_import_type_` is set
  // to kLocalCard because we need to make sure we do not return early in the
  // kNewCard case, and kLocalCard with upstream enabled but empty
  // |imported_credit_card| is the most likely scenario for a crash.
  form_data_importer().set_credit_card_import_type_for_testing(
      FormDataImporter::CreditCardImportType::kLocalCard);

  // We need a sync service so that
  // LocalCardMigrationManager::ShouldOfferLocalCardMigration() does not crash.
  syncer::TestSyncService sync_service;
  personal_data_manager_->SetSyncServiceForTest(&sync_service);

  EXPECT_FALSE(form_data_importer().ProcessExtractedCreditCard(
      *form_structure, extracted_credit_card, extracted_upi_id,
      /*payment_methods_autofill_enabled=*/true,
      /*is_credit_card_upstream_enabled=*/true));
  personal_data_manager_->SetSyncServiceForTest(nullptr);
}

#if !BUILDFLAG(IS_IOS)
TEST_F(FormDataImporterNonParameterizedTest,
       ProcessExtractedCreditCard_VirtualCardEligible) {
  CreditCard imported_credit_card = test::GetMaskedServerCard();
  imported_credit_card.SetNetworkForMaskedCard(kAmericanExpressCard);
  imported_credit_card.set_instrument_id(1111);
  imported_credit_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE);
  absl::optional<CreditCard> extracted_credit_card = imported_credit_card;
  absl::optional<std::string> extracted_upi_id;
  std::unique_ptr<FormStructure> form_structure =
      ConstructDefaultCreditCardFormStructure();

  form_data_importer().set_credit_card_import_type_for_testing(
      FormDataImporter::CreditCardImportType::kServerCard);
  form_data_importer().SetFetchedCardInstrumentId(2222);

  // We need a sync service so that
  // LocalCardMigrationManager::ShouldOfferLocalCardMigration() does not
  // crash.
  syncer::TestSyncService sync_service;
  personal_data_manager_->SetSyncServiceForTest(&sync_service);

  EXPECT_CALL(*virtual_card_enrollment_manager_,
              InitVirtualCardEnroll(_, VirtualCardEnrollmentSource::kDownstream,
                                    _, _, _, _))
      .Times(0);
  EXPECT_FALSE(form_data_importer().ProcessExtractedCreditCard(
      *form_structure, extracted_credit_card, extracted_upi_id,
      /*payment_methods_autofill_enabled=*/true,
      /*is_credit_card_upstream_enabled=*/true));

  form_data_importer().SetFetchedCardInstrumentId(1111);
  EXPECT_CALL(*virtual_card_enrollment_manager_,
              InitVirtualCardEnroll(_, VirtualCardEnrollmentSource::kDownstream,
                                    _, _, _, _))
      .Times(1);
  EXPECT_TRUE(form_data_importer().ProcessExtractedCreditCard(
      *form_structure, extracted_credit_card, extracted_upi_id,
      /*payment_methods_autofill_enabled=*/true,
      /*is_credit_card_upstream_enabled=*/true));

  personal_data_manager_->SetSyncServiceForTest(nullptr);
}
#endif

TEST_F(FormDataImporterNonParameterizedTest,
       ShouldOfferUploadCardOrLocalCardSave) {
  // Should not offer save for null cards.
  absl::optional<CreditCard> extracted_credit_card;
  EXPECT_FALSE(form_data_importer().ShouldOfferUploadCardOrLocalCardSave(
      extracted_credit_card,
      /*is_credit_card_upload_enabled=*/false));

  extracted_credit_card = test::GetCreditCard();

  // Should not offer save for local cards if upstream is not enabled.
  form_data_importer().set_credit_card_import_type_for_testing(
      FormDataImporter::CreditCardImportType::kLocalCard);
  EXPECT_FALSE(form_data_importer().ShouldOfferUploadCardOrLocalCardSave(
      extracted_credit_card,
      /*is_credit_card_upload_enabled=*/false));

  // Should offer save for local cards if upstream is enabled.
  EXPECT_TRUE(form_data_importer().ShouldOfferUploadCardOrLocalCardSave(
      extracted_credit_card,
      /*is_credit_card_upload_enabled=*/true));

  // Should not offer save for server cards.
  form_data_importer().set_credit_card_import_type_for_testing(
      FormDataImporter::CreditCardImportType::kServerCard);
  EXPECT_FALSE(form_data_importer().ShouldOfferUploadCardOrLocalCardSave(
      extracted_credit_card,
      /*is_credit_card_upload_enabled=*/true));

  // Should always offer save for new cards; upload save if it is enabled, local
  // save otherwise.
  form_data_importer().set_credit_card_import_type_for_testing(
      FormDataImporter::CreditCardImportType::kNewCard);
  EXPECT_TRUE(form_data_importer().ShouldOfferUploadCardOrLocalCardSave(
      extracted_credit_card,
      /*is_credit_card_upload_enabled=*/true));
  EXPECT_TRUE(form_data_importer().ShouldOfferUploadCardOrLocalCardSave(
      extracted_credit_card,
      /*is_credit_card_upload_enabled=*/false));
}

}  // namespace autofill
