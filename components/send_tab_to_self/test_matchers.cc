// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/test_matchers.h"

namespace send_tab_to_self {

testing::Matcher<PageContext::FormField> MatchesFormField(
    testing::Matcher<std::u16string> id_attribute,
    testing::Matcher<std::u16string> name_attribute,
    testing::Matcher<std::string> form_control_type,
    testing::Matcher<std::u16string> value,
    testing::Matcher<PageContext::FormFieldAutofillSignature>
        autofill_signature) {
  return testing::AllOf(
      testing::Field("id_attribute", &PageContext::FormField::id_attribute,
                     id_attribute),
      testing::Field("name_attribute", &PageContext::FormField::name_attribute,
                     name_attribute),
      testing::Field("form_control_type",
                     &PageContext::FormField::form_control_type,
                     form_control_type),
      testing::Field("value", &PageContext::FormField::value, value),
      testing::Field("autofill_signature",
                     &PageContext::FormField::autofill_signature,
                     autofill_signature));
}

testing::Matcher<PageContext::FormFieldAutofillSignature>
MatchesAutofillSignature(
    testing::Matcher<autofill::FormSignature> form_signature,
    testing::Matcher<autofill::FieldSignature> field_signature) {
  return testing::AllOf(
      testing::Field("form_signature",
                     &PageContext::FormFieldAutofillSignature::form_signature,
                     form_signature),
      testing::Field("field_signature",
                     &PageContext::FormFieldAutofillSignature::field_signature,
                     field_signature));
}

}  // namespace send_tab_to_self
