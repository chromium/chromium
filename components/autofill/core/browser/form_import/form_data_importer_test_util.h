// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_UTIL_H_

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"

namespace autofill {
namespace test {
// Define values for the default address profile.
inline constexpr std::string_view kDefaultFirstName = "Thomas";
inline constexpr std::string_view kDefaultLastName = "Anderson";
inline constexpr std::string_view kDefaultMail = "theone@thematrix.org";
inline constexpr std::string_view kDefaultAddressLine1 = "21 Laussat St";
inline constexpr std::string_view kDefaultCity = "Los Angeles";
inline constexpr std::string_view kDefaultState = "California";
inline constexpr std::string_view kDefaultZip = "94102";
inline constexpr std::string_view kDefaultCountry = "US";
inline constexpr std::string_view kDefaultPhone = "+1 650-555-0000";

// Define values for a second address profile.
inline constexpr std::string_view kSecondFirstName = "Bruce";
inline constexpr std::string_view kSecondLastName = "Wayne";
inline constexpr std::string_view kSecondMail = "wayne@bruce.org";
inline constexpr std::string_view kSecondAddressLine1 = "23 Main St";
inline constexpr std::string_view kSecondZip = "94106";
inline constexpr std::string_view kSecondCity = "Los Angeles";
inline constexpr std::string_view kSecondState = "California";
inline constexpr std::string_view kSecondPhone = "+1 651-666-1111";

inline constexpr std::string_view kDefaultCreditCardName = "Biggie Smalls";
inline constexpr std::string_view kDefaultCreditCardNumber =
    "4111 1111 1111 1111";
inline constexpr std::string_view kDefaultCreditCardExpMonth = "01";
inline constexpr std::string_view kDefaultCreditCardExpYear = "2999";
}  // namespace test

class AutofillProfile;
class FormData;
class FormStructure;

using TypeValuePairs = std::vector<std::pair<FieldType, std::string_view>>;
using TypeValuePairsSpan =
    base::span<const std::pair<FieldType, std::string_view>>;

// Constructs a FormData instance for `url` from a vector of type value pairs
// that defines a sequence of fields and the filled values.
// The field names and labels for the different types are relieved from
// `GetLabelAndNameForType(type)`
FormData ConstructFormDataFromTypeValuePairs(
    TypeValuePairsSpan type_value_pairs,
    std::string_view url = "https://www.foo.com");

// Fakes that a `form` has been seen (without its field value) and parsed and
// then values have been entered. Returns the resulting FormStructure.
std::unique_ptr<FormStructure> ConstructFormStructureFromFormData(
    const FormData& form,
    GeoIpCountryCode geo_country = GeoIpCountryCode(""));

// Constructs a FormStructure instance with fields and inserted values given by
// a vector of type and value pairs.
std::unique_ptr<FormStructure> ConstructFormStructureFromTypeValuePairs(
    TypeValuePairsSpan type_value_pairs,
    std::string_view url = "https://www.foo.com");

// Construct and finalizes an AutofillProfile based on a vector of type and
// value pairs. The values are set as `VerificationStatus::kObserved` and the
// profile is finalized in the end.
AutofillProfile ConstructProfileFromTypeValuePairs(
    TypeValuePairsSpan type_value_pairs);

// Returns a span of FieldType and value pairs used to construct the
// default AutofillProfile, or a FormStructure or FormData instance that carries
// that corresponding information.
constexpr TypeValuePairsSpan GetDefaultProfileTypeValuePairs() {
  static constexpr std::pair<FieldType, std::string_view> kPairs[] = {
      {NAME_FIRST, test::kDefaultFirstName},
      {NAME_LAST, test::kDefaultLastName},
      {EMAIL_ADDRESS, test::kDefaultMail},
      {PHONE_HOME_WHOLE_NUMBER, test::kDefaultPhone},
      {ADDRESS_HOME_LINE1, test::kDefaultAddressLine1},
      {ADDRESS_HOME_CITY, test::kDefaultCity},
      {ADDRESS_HOME_STATE, test::kDefaultState},
      {ADDRESS_HOME_ZIP, test::kDefaultZip},
      {ADDRESS_HOME_COUNTRY, test::kDefaultCountry},
  };
  return kPairs;
}

// Sets the value of `type` in `pairs` to `value`. If the `value` is empty, the
// `type` is removed entirely.
void SetValueForType(TypeValuePairs& pairs,
                     FieldType type,
                     std::string_view value);

// Same as `GetDefaultProfileTypeValuePairs()`, but split into two parts to test
// multi-step imports. No part by itself satisfies the import requirements.
// `part` specifies the requested half and can be either 1 or 2.
constexpr TypeValuePairsSpan GetSplitDefaultProfileTypeValuePairs(int part) {
  DCHECK(part == 1 || part == 2);
  if (part == 1) {
    static constexpr std::pair<FieldType, std::string_view> kPairs1[] = {
        {NAME_FIRST, test::kDefaultFirstName},
        {NAME_LAST, test::kDefaultLastName},
        {EMAIL_ADDRESS, test::kDefaultMail},
        {ADDRESS_HOME_CITY, test::kDefaultCity},
        {ADDRESS_HOME_STATE, test::kDefaultState},
        {ADDRESS_HOME_COUNTRY, test::kDefaultCountry},
    };
    return kPairs1;
  } else {
    static constexpr std::pair<FieldType, std::string_view> kPairs2[] = {
        {PHONE_HOME_WHOLE_NUMBER, test::kDefaultPhone},
        {ADDRESS_HOME_LINE1, test::kDefaultAddressLine1},
        {ADDRESS_HOME_ZIP, test::kDefaultZip},
    };
    return kPairs2;
  }
}

// Same as `GetDefaultProfileTypeValuePairs()` but with the second profile
// information.
constexpr TypeValuePairsSpan GetSecondProfileTypeValuePairs() {
  static constexpr std::pair<FieldType, std::string_view> kPairs[] = {
      {NAME_FIRST, test::kSecondFirstName},
      {NAME_LAST, test::kSecondLastName},
      {EMAIL_ADDRESS, test::kSecondMail},
      {PHONE_HOME_WHOLE_NUMBER, test::kSecondPhone},
      {ADDRESS_HOME_LINE1, test::kSecondAddressLine1},
      {ADDRESS_HOME_CITY, test::kSecondCity},
      {ADDRESS_HOME_STATE, test::kSecondState},
      {ADDRESS_HOME_ZIP, test::kSecondZip},
      {ADDRESS_HOME_COUNTRY, test::kDefaultCountry},
  };
  return kPairs;
}

// Same as `GetDefaultProfileTypeValuePairs()`, but for credit cards.
constexpr TypeValuePairsSpan GetDefaultCreditCardTypeValuePairs() {
  static constexpr std::pair<FieldType, std::string_view> kPairs[] = {
      {CREDIT_CARD_NAME_FULL, test::kDefaultCreditCardName},
      {CREDIT_CARD_NUMBER, test::kDefaultCreditCardNumber},
      {CREDIT_CARD_EXP_MONTH, test::kDefaultCreditCardExpMonth},
      {CREDIT_CARD_EXP_4_DIGIT_YEAR, test::kDefaultCreditCardExpYear},
  };
  return kPairs;
}

// Returns the default AutofillProfile used in this test file.
AutofillProfile ConstructDefaultProfile();

// Returns the second AutofillProfile used in this test file.
AutofillProfile ConstructSecondProfile();

// Returns a form with the default profile. The AutofillProfile that is imported
// from this form should be similar to the profile create by calling
// `ConstructDefaultProfile()`.
std::unique_ptr<FormStructure> ConstructDefaultProfileFormStructure();

// Same as `ConstructDefaultFormStructure()` but split into two parts to test
// multi-step imports (see `GetSplitDefaultProfileTypeValuePairs()`).
std::unique_ptr<FormStructure> ConstructSplitDefaultProfileFormStructure(
    int part);

// Constructs a FormStructure with two address sections by concatenating
// the default profile and second profile form structures.
std::unique_ptr<FormStructure> ConstructShippingAndBillingFormStructure();

// Constructs a `FormData` instance that carries the information of the default
// profile.
FormData ConstructDefaultFormData();

// Adds a full credit card form to `form`, given the parameters `name`,
// `number`, `month`, and `year`. The parameters represent field values and are
// optional, and the parameters that are passed in will be the ones that are
// set, and its corresponding field will be created in `form`.
void AddFullCreditCardForm(FormData* form,
                           const char* name,
                           const char* number,
                           const char* month,
                           const char* year);

// Returns a dummy UKM source ID for testing purposes.
ukm::SourceId ukm_source_id();

// Convenience wrapper that calls
// `FormDataImporter::ExtractFormData()` and subsequently
// processes the candidates for address profile import. Returns the result of
// `FormDataImporter::ExtractFormData()`.
FormDataImporterTestApi::ExtractedFormData
ExtractFormDataAndProcessAddressCandidates(
    FormDataImporter& form_data_importer,
    const FormStructure& form,
    bool profile_autofill_enabled,
    bool payment_methods_autofill_enabled);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_UTIL_H_
