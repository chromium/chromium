// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORM_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORM_TEST_UTILS_H_

#include <vector>

#include "base/optional.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace test {

namespace {

// Default label assigned to fields.
constexpr char kLabelText[] = "label";

// Default name attribute assigned to fields.
constexpr char kNameText[] = "name";

// Default form url.
constexpr char kFormUrl[] = "http://www.foo.com/";

}  // namespace

namespace internal {

// Expected FormFieldData are constructed based on these descriptions.
template <typename = void>
struct FieldDataDescription {
  ServerFieldType role = ServerFieldType::EMPTY_TYPE;
  bool is_focusable = true;
  const char* label = kLabelText;
  const char* name = kNameText;
  const char* autocomplete_attribute = nullptr;
  const char* form_control_type = "text";
  bool should_autocomplete = true;
};

// Attributes provided to the test form.
template <typename = void>
struct FormAttributes {
  const char* description_for_logging = "";
  std::vector<FieldDataDescription<>> fields = {};
  const char* form_url = kFormUrl;
  bool is_formless_checkout = false;
  bool is_form_tag = true;
};

// Flags determining whether the corresponding check should be run on the test
// form.
template <typename = void>
struct FormFlags {
  // false means the function is not to be called.
  bool determine_heuristic_type = false;
  bool parse_query_response = false;
  // false means the corresponding check is not supposed to run.
  bool is_autofillable = false;
  bool should_be_parsed = false;
  bool should_be_queried = false;
  bool should_be_uploaded = false;
  bool has_author_specified_types = false;
  bool has_author_specified_upi_vpa_hint = false;
  // first value denotes whether the comparison is to be done while second
  // denotes EXPECT_TRUE for true and EXPECT_FALSE for false.
  std::pair<bool, bool> is_complete_credit_card_form = {false, false};
  // base::nullopt means no checking.
  base::Optional<int> field_count = base::nullopt;
  base::Optional<int> autofill_count = base::nullopt;
  base::Optional<int> section_count = base::nullopt;
  base::Optional<int> response_field_count = base::nullopt;
};

// Expected field type values to be verified with the test form.
template <typename = void>
struct ExpectedFieldTypeValues {
  std::vector<HtmlFieldType> expected_html_type = {};
  std::vector<AutofillField::PhonePart> expected_phone_part = {};
  std::vector<ServerFieldType> expected_heuristic_type = {};
  std::vector<ServerFieldType> expected_overall_type = {};
};

// Describes a test case for the parser.
template <typename = void>
struct FormStructureTestCase {
  FormAttributes<> form_attributes;
  FormFlags<> form_flags;
  ExpectedFieldTypeValues<> expected_field_types;
};

}  // namespace internal

using FieldDataDescription = internal::FieldDataDescription<>;
using FormAttributes = internal::FormAttributes<>;
using FormStructureTestCase = internal::FormStructureTestCase<>;

// Describes the |form_data|. Use this in SCOPED_TRACE if other logging
// messages might refer to the form.
testing::Message DescribeFormData(const FormData& form_data);

// Returns the form field relevant to the |role|.
FormFieldData CreateFieldByRole(ServerFieldType role);

// Creates a FormData to be fed to the parser.
FormData GetFormData(const FormAttributes& form_attributes);

class FormStructureTest : public testing::Test {
 protected:
  // Iterates over |test_cases|, creates a FormData for each, runs the parser
  // and checks the results.
  static void CheckFormStructureTestData(
      const std::vector<FormStructureTestCase>& test_cases);
};

}  // namespace test
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORM_TEST_UTILS_H_
