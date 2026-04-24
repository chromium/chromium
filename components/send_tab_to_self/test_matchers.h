// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_TEST_MATCHERS_H_
#define COMPONENTS_SEND_TAB_TO_SELF_TEST_MATCHERS_H_

#include <string>
#include <vector>

#include "components/autofill/core/common/signatures.h"
#include "components/send_tab_to_self/page_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace send_tab_to_self {

// Matches a `PageContext::FormField` by checking its attributes.
// The last argument matches the `FormFieldAutofillSignature` (which contains
// form and field signatures).
testing::Matcher<PageContext::FormField> MatchesFormField(
    testing::Matcher<std::u16string> id_attribute,
    testing::Matcher<std::u16string> name_attribute,
    testing::Matcher<std::string> form_control_type,
    testing::Matcher<std::u16string> value,
    testing::Matcher<PageContext::FormFieldAutofillSignature>
        autofill_signature);

// Helper to match `PageContext::FormFieldAutofillSignature`.
testing::Matcher<PageContext::FormFieldAutofillSignature>
MatchesAutofillSignature(
    testing::Matcher<autofill::FormSignature> form_signature,
    testing::Matcher<autofill::FieldSignature> field_signature);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_TEST_MATCHERS_H_
