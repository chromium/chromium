// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_AUTOCOMPLETE_ATTRIBUTE_PROCESSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_AUTOCOMPLETE_ATTRIBUTE_PROCESSING_UTIL_H_

#include <string>

#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
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
// - Only "on", "off" or "false" is specified (currently ignored by Autofill).
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
    const AutofillField& field);

// Maps HTML_MODE_BILLING and HTML_MODE_SHIPPING to their string constants, as
// specified in the autocomplete standard.
base::StringPiece HtmlFieldModeToStringPiece(HtmlFieldMode mode);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_AUTOCOMPLETE_ATTRIBUTE_PROCESSING_UTIL_H_
