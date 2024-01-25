// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOCOMPLETE_PARSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOCOMPLETE_PARSING_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "components/autofill/core/common/html_field_types.h"

namespace autofill {

// The autocomplete attribute consists of several components, as described at
// http://is.gd/whatwg_autocomplete. Autofill supports part of the specification
// and parses the following tokens:
// [section-*] [shipping|billing] [type_hint] field_type [webauthn]
// The parsing extracts these components from `field.autocomplete_attribute` or
// returns std::nullopt, if the parsing fails. The latter happens if:
// - The autocomplete value is empty or contains more than 5 tokens.
// - The type_hint doesn't match the field_type.
// - If ShouldIgnoreAutocompleteAttribute(autocomplete) is true.
// An unrecognizable field_type doesn't stop parsing and yields
// HtmlFieldType::kUnrecognized instead.
struct AutocompleteParsingResult {
  std::string ToString() const;

  bool operator==(const AutocompleteParsingResult&) const;

  // `section` corresponds to the string after "section-".
  std::string section;
  HtmlFieldMode mode = HtmlFieldMode::kNone;
  // Type hints are parsed and validated, but otherwise unused.
  HtmlFieldType field_type = HtmlFieldType::kUnspecified;
  // Whether the field has a `webauthn` token.
  bool webauthn = false;
};

std::optional<AutocompleteParsingResult> ParseAutocompleteAttribute(
    std::string_view autocomplete_attribute);

// Checks if `autocomplete_attribute` could not be recognized but was
// nonetheless found as well intended. This will therefore return true for
// values such as "first-name", "last-name" and "password".
bool IsAutocompleteTypeWrongButWellIntended(
    std::string_view autocomplete_attribute);

// Checks if `autocomplete` is one of "on", "off" or "false". These values are
// currently ignored by Autofill.
bool ShouldIgnoreAutocompleteAttribute(std::string_view autocomplete);

// Parses `value` as an HTML field type and converts it to the corresponding
// HtmlFieldType, if it is supposed by Autofill.
// HtmlFieldType::kUnspecified is returned if `value` is empty. Otherwise
// HtmlFieldType::kUnrecognized is returned.
HtmlFieldType FieldTypeFromAutocompleteAttributeValue(std::string value);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOCOMPLETE_PARSING_UTIL_H_
