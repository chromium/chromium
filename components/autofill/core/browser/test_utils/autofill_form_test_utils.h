// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_AUTOFILL_FORM_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_AUTOFILL_FORM_TEST_UTILS_H_

#include <optional>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/test_utils/autofill_form_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::test {

namespace internal {

struct CreateFieldByRole {
  // Allows injection of `FieldType` into struct `FieldDescription` defined in
  // common/ layer for the definition of a role of a field.
  using FieldType = FieldType;

  static constexpr FieldType RoleDefaultValue = EMPTY_TYPE;

  // Returns the form field initialized according to the `role`.
  FormFieldData operator()(FieldType role) const;
};

// Flags determining whether the corresponding check should be run on the test
// form.
template <typename = void>
struct TestFormFlags {
  // false means the function is not to be called.
  bool determine_heuristic_type = false;
  bool parse_query_response = false;
  // false means the corresponding check is not supposed to run.
  bool is_autofillable = false;
  bool should_be_parsed = false;
  bool should_be_queried = false;
  bool should_be_uploaded = false;
  bool has_author_specified_types = false;
  // The implicit default value `std::nullopt` means no checking. The first
  // value is the argument for `IsCompleteCreditCardForm()` specifying the
  // required completeness level of the credit card form. The second value is
  // the expected result of `IsCompleteCreditCardForm()`.
  std::optional<std::pair<FormStructure::CreditCardFormCompleteness, bool>>
      is_complete_credit_card_form;
  std::optional<int> field_count;
  std::optional<int> autofill_count;
  std::optional<int> section_count;
  std::optional<int> response_field_count;
};

// Expected field type values to be verified with the test form.
template <typename = void>
struct ExpectedFieldTypeValues {
  std::vector<HtmlFieldType> expected_html_type = {};
  std::vector<FieldType> expected_heuristic_type = {};
  std::vector<FieldType> expected_overall_type = {};
};

// Describes a test case for the parser.
template <typename = void>
struct FormStructureTestCase {
  FormDescription<FieldDescription<CreateFieldByRole>> form_attributes;
  TestFormFlags<> form_flags;
  ExpectedFieldTypeValues<> expected_field_types;
};

}  // namespace internal

// Replaces `CommonFieldDescription`. It provides the additional ability to
// initialize a `FormFieldData` using `role`.
using FieldDescription =
    internal::FieldDescription<internal::CreateFieldByRole>;
// Replaces `CommonFormDescription`. It uses `FieldDescription` instead of
// `CommonFieldDescription` for its `fields`.
using FormDescription = internal::FormDescription<FieldDescription>;

using FormStructureTestCase = internal::FormStructureTestCase<>;

// Describes the `form_data`. Use this in `SCOPED_TRACE` if other logging
// messages might refer to the form.
testing::Message DescribeFormData(const FormData& form_data);

// Overloads for templated function above to serve as deduction guides. This is
// used to overrule the default template arguments of `GetFormFieldData<>` and
// `GetFormData<>`. If this header is included, any implicit creation of the
// `description` (e.g. using designed initializers), will use the
// `FieldDescription` defined in this header file instead of
// `CommonFieldDescription` (same for `GetFormData`).
inline FormFieldData GetFormFieldData(const FieldDescription& description) {
  return GetFormFieldData<FieldDescription>(description);
}
inline FormData GetFormData(const FormDescription& description) {
  return GetFormData<FormDescription>(description);
}

// Creates a `FormData` with `field_types`.
FormData GetFormData(const std::vector<FieldType>& field_types);

// Extracts the heuristic types from the form description. If the heuristic type
// is not explicitly set for a given field it is extracted from the field's
// role.
std::vector<FieldType> GetHeuristicTypes(
    const FormDescription& form_description);

// Extracts the server types from the form description. If the server type
// is not explicitly set for field it is extracted from the field's role.
std::vector<FieldType> GetServerTypes(const FormDescription& form_description);

class FormStructureTest : public testing::Test {
 protected:
  // Iterates over |test_cases|, creates a FormData for each, runs the parser
  // and checks the results.
  static void CheckFormStructureTestData(
      const std::vector<FormStructureTestCase>& test_cases);
};

}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_AUTOFILL_FORM_TEST_UTILS_H_
