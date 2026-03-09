// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_UTILS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/form_data_importer_test_api.h"

namespace autofill {

class AutofillProfile;
class FormData;
class FormStructure;

using TypeValuePairs = std::vector<std::pair<FieldType, std::string>>;

// For a given FieldType `type` returns a pair of field name and label that
// should be parsed into this type by our field type parsers.
std::pair<std::string, std::string> GetLabelAndNameForType(FieldType type);

// Constructs a FormData instance for `url` from a vector of type value pairs
// that defines a sequence of fields and the filled values.
// The field names and labels for the different types are relieved from
// `GetLabelAndNameForType(type)`
FormData ConstructFormDateFromTypeValuePairs(
    TypeValuePairs type_value_pairs,
    std::string url = "https://www.foo.com");

// Fakes that a `form` has been seen (without its field value) and parsed and
// then values have been entered. Returns the resulting FormStructure.
std::unique_ptr<FormStructure> ConstructFormStructureFromFormData(
    const FormData& form,
    GeoIpCountryCode geo_country = GeoIpCountryCode(""));

// Constructs a FormStructure instance with fields and inserted values given by
// a vector of type and value pairs.
std::unique_ptr<FormStructure> ConstructFormStructureFromTypeValuePairs(
    TypeValuePairs type_value_pairs,
    std::string url = "https://www.foo.com");

// Construct and finalizes an AutofillProfile based on a vector of type and
// value pairs. The values are set as `VerificationStatus::kObserved` and the
// profile is finalized in the end.
AutofillProfile ConstructProfileFromTypeValuePairs(
    TypeValuePairs type_value_pairs);

// Returns a vector of FieldType and value pairs used to construct the
// default AutofillProfile, or a FormStructure or FormData instance that carries
// that corresponding information.
TypeValuePairs GetDefaultProfileTypeValuePairs();

// Sets the value of `type` in `pairs` to `value`. If the `value` is empty, the
// `type` is removed entirely.
void SetValueForType(TypeValuePairs& pairs,
                     FieldType type,
                     const std::string& value);

// Wraps `GetDefaultProfileTypeValuePairs()` but replaces `kDefaultCountry` with
// `country`. If `country` is empty, ADDRESS_HOME_COUNTRY is removed entirely.
TypeValuePairs GetDefaultProfileTypeValuePairsWithOverriddenCountry(
    const std::string& country);

// Same as `GetDefaultProfileTypeValuePairs()`, but split into two parts to test
// multi-step imports. No part by itself satisfies the import requirements.
// `part` specifies the requested half and can be either 1 or 2.
TypeValuePairs GetSplitDefaultProfileTypeValuePairs(int part);

// Same as `GetDefaultProfileTypeValuePairs()` but with the second profile
// information.
TypeValuePairs GetSecondProfileTypeValuePairs();

// Same as `GetDefaultProfileTypeValuePairs()` but with the third profile
// information.
TypeValuePairs GetThirdProfileTypeValuePairs();

// Same as `GetDefaultProfileTypeValuePairs()`, but for credit cards.
TypeValuePairs GetDefaultCreditCardTypeValuePairs();

// Returns the default AutofillProfile used in this test file.
AutofillProfile ConstructDefaultProfile();

// Returns the second AutofillProfile used in this test file.
AutofillProfile ConstructSecondProfile();

// Returns the third AutofillProfile used in this test file.
AutofillProfile ConstructThirdProfile();

// Returns a form with the default profile. The AutofillProfile that is imported
// from this form should be similar to the profile create by calling
// `ConstructDefaultProfile()`.
std::unique_ptr<FormStructure> ConstructDefaultProfileFormStructure();

// Constructs a form structure containing only an email field, set to
// `kDefaultMail`. This is useful for testing multi-step complements.
std::unique_ptr<FormStructure> ConstructDefaultEmailFormStructure();

// Same as `ConstructDefaultFormStructure()` but split into two parts to test
// multi-step imports (see `GetSplitDefaultProfileTypeValuePairs()`).
std::unique_ptr<FormStructure> ConstructSplitDefaultProfileFormStructure(
    int part);

// Same as `ConstructDefaultFormStructure()` but for credit cards.
std::unique_ptr<FormStructure> ConstructDefaultCreditCardFormStructure();

// Same as `ConstructDefaultFormStructure()` but for the second profile.
std::unique_ptr<FormStructure> ConstructSecondProfileFormStructure();

// Same as `ConstructDefaultFormStructure()` but for the third profile.
std::unique_ptr<FormStructure> ConstructThirdProfileFormStructure();

// Constructs a FormStructure with two address sections by concatenating
// the default profile and second profile form structures.
std::unique_ptr<FormStructure> ConstructShippingAndBillingFormStructure();

// Constructs a `FormData` instance that carries the information of the default
// profile.
FormData ConstructDefaultFormData();

// Same as `ConstructDefaultFormData()` but split into two parts to test multi-
// step imports (see `GetSplitDefaultProfileTypeValuePairs()`).
FormData ConstructSplitDefaultFormData(int part);

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

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_UTILS_H_
