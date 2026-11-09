// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/form_data_importer_test_util.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/to_vector.h"
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
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/language_code.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using test::CreateTestFormField;

// For a given FieldType `type` returns a pair of field name and label that
// should be parsed into this type by our field type parsers.
std::pair<std::string_view, std::string_view> GetLabelAndNameForType(
    FieldType type) {
  static constexpr auto kNameTypeMap =
      base::MakeFixedFlatMap<FieldType,
                             std::pair<std::string_view, std::string_view>>({
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
      });
  auto it = kNameTypeMap.find(type);
  if (it == kNameTypeMap.end()) {
    NOTIMPLEMENTED() << " field name and label is missing for "
                     << FieldTypeToStringView(type);
    return {std::string_view(), std::string_view()};
  }
  return it->second;
}

}  // anonymous namespace

FormData ConstructFormDataFromTypeValuePairs(
    TypeValuePairsSpan type_value_pairs,
    std::string_view url) {
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
    TypeValuePairsSpan type_value_pairs,
    std::string_view url) {
  FormData form = ConstructFormDataFromTypeValuePairs(type_value_pairs, url);
  return ConstructFormStructureFromFormData(form);
}

AutofillProfile ConstructProfileFromTypeValuePairs(
    TypeValuePairsSpan type_value_pairs) {
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

// Sets the value of `type` in `pairs` to `value`. If the `value` is empty, the
// `type` is removed entirely.
void SetValueForType(TypeValuePairs& pairs,
                     FieldType type,
                     std::string_view value) {
  auto it = std::ranges::find(pairs, type,
                              &std::pair<FieldType, std::string_view>::first);

  if (value.empty() && it != pairs.end()) {
    pairs.erase(it);
  } else if (it != pairs.end()) {
    it->second = value;
  } else if (!value.empty()) {
    pairs.emplace_back(type, value);
  }
}

AutofillProfile ConstructDefaultProfile() {
  return ConstructProfileFromTypeValuePairs(GetDefaultProfileTypeValuePairs());
}

AutofillProfile ConstructSecondProfile() {
  return ConstructProfileFromTypeValuePairs(GetSecondProfileTypeValuePairs());
}

std::unique_ptr<FormStructure> ConstructDefaultProfileFormStructure() {
  return ConstructFormStructureFromTypeValuePairs(
      GetDefaultProfileTypeValuePairs());
}

std::unique_ptr<FormStructure> ConstructSplitDefaultProfileFormStructure(
    int part) {
  return ConstructFormStructureFromTypeValuePairs(
      GetSplitDefaultProfileTypeValuePairs(part));
}

std::unique_ptr<FormStructure> ConstructShippingAndBillingFormStructure() {
  TypeValuePairs a = base::ToVector(GetDefaultProfileTypeValuePairs());
  base::Extend(a, GetSecondProfileTypeValuePairs());
  return ConstructFormStructureFromTypeValuePairs(a);
}

FormData ConstructDefaultFormData() {
  return ConstructFormDataFromTypeValuePairs(GetDefaultProfileTypeValuePairs());
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
