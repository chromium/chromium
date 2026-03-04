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

// Returns the default AutofillProfile used in this test file.
AutofillProfile ConstructDefaultProfile();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_UTILS_H_
