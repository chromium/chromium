// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/form_data_importer_test_utils.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field_test_api.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"
#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/language_code.h"
#include "url/gurl.h"

namespace autofill {

namespace {
// Define values for the default address profile.
constexpr char kDefaultFirstName[] = "Thomas";
constexpr char kDefaultLastName[] = "Anderson";
constexpr char kDefaultMail[] = "theone@thematrix.org";
constexpr char kDefaultAddressLine1[] = "21 Laussat St";
constexpr char kDefaultCity[] = "Los Angeles";
constexpr char kDefaultState[] = "California";
constexpr char kDefaultZip[] = "94102";
constexpr char kDefaultCountry[] = "US";
constexpr char kDefaultPhone[] = "+1 650-555-0000";

// Define values for a second address profile.
constexpr char kSecondFirstName[] = "Bruce";
constexpr char kSecondLastName[] = "Wayne";
constexpr char kSecondMail[] = "wayne@bruce.org";
constexpr char kSecondAddressLine1[] = "23 Main St";
constexpr char kSecondZip[] = "94106";
constexpr char kSecondCity[] = "Los Angeles";
constexpr char kSecondState[] = "California";
constexpr char kSecondPhone[] = "+1 651-666-1111";

// Define values for a third address profile.
constexpr char kThirdFirstName[] = "Homer";
constexpr char kThirdLastName[] = "Simpson";
constexpr char kThirdMail[] = "donut@whatever.net";
constexpr char kThirdAddressLine1[] = "742 Evergreen Terrace";
constexpr char kThirdZip[] = "65619";
constexpr char kThirdCity[] = "Springfield";
constexpr char kThirdState[] = "Oregon";
constexpr char kThirdPhone[] = "+1 850-777-2222";

constexpr char kDefaultPhoneGermany[] = "+49 89 123456";
constexpr char kDefaultPhoneMexico[] = "+52 55 1234 5678";
constexpr char kDefaultPhoneArmenia[] = "+374 10 123456";
}  // anonymous namespace

using test::CreateTestFormField;

constexpr char kDefaultCreditCardName[] = "Biggie Smalls";
constexpr char kDefaultCreditCardNumber[] = "4111 1111 1111 1111";
constexpr char kDefaultCreditCardExpMonth[] = "01";
constexpr char kDefaultCreditCardExpYear[] = "2999";

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

FormData ConstructFormDateFromTypeValuePairs(TypeValuePairs type_value_pairs,
                                             std::string url) {
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

std::unique_ptr<FormStructure> ConstructFormStructureFromFormData(
    const FormData& form,
    GeoIpCountryCode geo_country) {
  auto form_structure = std::make_unique<FormStructure>(form);
  const RegexPredictions regex_predictions = DetermineRegexTypes(
      geo_country, LanguageCode(""), form_structure->ToFormData(), nullptr,
      /*ignore_small_forms=*/true);
  regex_predictions.ApplyTo(form_structure->fields());
  form_structure->RationalizeAndAssignSections(geo_country, LanguageCode(""),
                                               nullptr);
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    test_api(*form_structure->field(i)).set_initial_value(u"");
  }
  return form_structure;
}

std::unique_ptr<FormStructure> ConstructFormStructureFromTypeValuePairs(
    TypeValuePairs type_value_pairs,
    std::string url) {
  FormData form = ConstructFormDateFromTypeValuePairs(type_value_pairs, url);
  return ConstructFormStructureFromFormData(form);
}

AutofillProfile ConstructProfileFromTypeValuePairs(
    TypeValuePairs type_value_pairs) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  for (const auto& [type, value] : type_value_pairs) {
    profile.SetRawInfoWithVerificationStatus(type, base::UTF8ToUTF16(value),
                                             VerificationStatus::kObserved);
  }
  if (!profile.FinalizeAfterImport()) {
    NOTREACHED();
  }
  return profile;
}

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
  auto it = std::ranges::find(pairs, type,
                              [](const auto& pair) { return pair.first; });
  CHECK(it != pairs.end());
  if (value.empty()) {
    pairs.erase(it);
  } else {
    it->second = value;
  }
}

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

TypeValuePairs GetDefaultCreditCardTypeValuePairs() {
  return {
      {CREDIT_CARD_NAME_FULL, kDefaultCreditCardName},
      {CREDIT_CARD_NUMBER, kDefaultCreditCardNumber},
      {CREDIT_CARD_EXP_MONTH, kDefaultCreditCardExpMonth},
      {CREDIT_CARD_EXP_4_DIGIT_YEAR, kDefaultCreditCardExpYear},
  };
}

AutofillProfile ConstructDefaultProfile() {
  return ConstructProfileFromTypeValuePairs(GetDefaultProfileTypeValuePairs());
}

AutofillProfile ConstructSecondProfile() {
  return ConstructProfileFromTypeValuePairs(GetSecondProfileTypeValuePairs());
}

AutofillProfile ConstructThirdProfile() {
  return ConstructProfileFromTypeValuePairs(GetThirdProfileTypeValuePairs());
}

std::unique_ptr<FormStructure> ConstructDefaultProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetDefaultProfileTypeValuePairs());
}

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

std::unique_ptr<FormStructure> ConstructSplitDefaultProfileFormStructure(
    int part) {
  return ConstructFormStructureFromTypeValuePairs(
      GetSplitDefaultProfileTypeValuePairs(part));
}

std::unique_ptr<FormStructure> ConstructDefaultCreditCardFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetDefaultCreditCardTypeValuePairs());
}

std::unique_ptr<FormStructure> ConstructSecondProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetSecondProfileTypeValuePairs());
}

std::unique_ptr<FormStructure> ConstructThirdProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetThirdProfileTypeValuePairs());
}

std::unique_ptr<FormStructure> ConstructShippingAndBillingFormStructure() {
  TypeValuePairs a = GetDefaultProfileTypeValuePairs();
  TypeValuePairs b = GetSecondProfileTypeValuePairs();
  a.reserve(a.size() + b.size());
  std::ranges::move(b, std::back_inserter(a));
  return ConstructFormStructureFromTypeValuePairs(a);
}

FormData ConstructDefaultFormData() {
  return ConstructFormDateFromTypeValuePairs(GetDefaultProfileTypeValuePairs());
}

FormData ConstructSplitDefaultFormData(int part) {
  return ConstructFormDateFromTypeValuePairs(
      GetSplitDefaultProfileTypeValuePairs(part));
}

void AddFullCreditCardForm(FormData* form,
                           const char* name,
                           const char* number,
                           const char* month,
                           const char* year) {
  std::vector<FormFieldData>& fields = test_api(*form).fields();
  if (name) {
    fields.push_back(CreateTestFormField("Name on card:", "name_on_card", name,
                                         FormControlType::kInputText));
  }
  if (number) {
    fields.push_back(CreateTestFormField("Card Number:", "card_number", number,
                                         FormControlType::kInputText));
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

ukm::SourceId ukm_source_id() {
  return 123;
}

FormDataImporterTestApi::ExtractedFormData
ExtractFormDataAndProcessAddressCandidates(
    FormDataImporter& form_data_importer,
    const FormStructure& form,
    bool profile_autofill_enabled,
    bool payment_methods_autofill_enabled) {
  FormDataImporterTestApi::ExtractedFormData extracted_data =
      test_api(form_data_importer)
          .ExtractFormData(form, profile_autofill_enabled,
                           payment_methods_autofill_enabled);
  test_api(form_data_importer.GetAddressFormDataImporter())
      .ProcessExtractedAddressProfiles(
          extracted_data.extracted_address_profiles,
          /*allow_prompt=*/true, ukm_source_id());
  return extracted_data;
}

}  // namespace autofill
