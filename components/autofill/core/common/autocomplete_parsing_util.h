// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOCOMPLETE_PARSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOCOMPLETE_PARSING_UTIL_H_

#include <string>

#include "base/strings/string_piece.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// The autocomplete attribute consists of several components, as described at
// http://is.gd/whatwg_autocomplete. Autofill supports part of the specification
// and parses the following tokens:
// [section-*] [shipping|billing] [type_hint] field_type [webauthn]
// The parsing extracts these components from `field.autocomplete_attribute` or
// returns absl::nullopt, if the parsing fails. The latter happens if:
// - The autocomplete value is empty or contains more than 5 tokens.
// - The type_hint doesn't match the field_type.
// - If ShouldIgnoreAutocompleteAttribute(autocomplete) is true.
// An unrecognizable field_type doesn't stop parsing and yields
// HTML_TYPE_UNRECOGNIZED instead.
struct AutocompleteParsingResult {
  // `section` corresponds to the string after "section-".
  std::string section;
  HtmlFieldMode mode;
  // Type hints are parsed and validated, but otherwise unused.
  HtmlFieldType field_type;
  // webauthn is parsed, but otherwise unused.
};
absl::optional<AutocompleteParsingResult> ParseAutocompleteAttribute(
    const FormFieldData& field);

// Checks if `autocomplete` is one of "on", "off" or "false". These values are
// currently ignored by Autofill.
bool ShouldIgnoreAutocompleteAttribute(base::StringPiece autocomplete);

// Parses `value` as an HTML field type and converts it to the corresponding
// HtmlFieldType, if it is supposed by Autofill. Rationalization based on the
// `field` is done.
// HTML_TYPE_UNSPECIFIED is returned if `value` is empty, or if `value` is
// supposed to be ignored by `kAutofillIgnoreUnmappableAutocompleteValues`.
// Otherwise HTML_TYPE_UNRECOGNIZED is returned.
HtmlFieldType FieldTypeFromAutocompleteAttributeValue(
    std::string value,
    const FormFieldData& field);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOCOMPLETE_PARSING_UTIL_H_
