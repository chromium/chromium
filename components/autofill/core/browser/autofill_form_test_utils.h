// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORM_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORM_TEST_UTILS_H_

#include <optional>
#include <vector>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace test {

namespace {

// Default form url.
constexpr char kFormUrl[] = "https://example.com/form.html";

// Default form action url.
constexpr char kFormActionUrl[] = "https://example.com/submit.html";

}  // namespace

namespace internal {

// Expected FormFieldData and type predictions are constructed based on these
// descriptions.
template <typename = void>
struct FieldDescription {
  FieldType role = FieldType::EMPTY_TYPE;
  // If the server type is not set explicitly, it is assumed to be given by the
  // role.
  std::optional<FieldType> server_type;
  // If the heuristic type is not set explicitly, it is assumed to be given by
  // the role.
  std::optional<FieldType> heuristic_type;
  std::optional<LocalFrameToken> host_frame;
  std::optional<FormSignature> host_form_signature;
  std::optional<FieldRendererId> renderer_id;
  bool is_focusable = true;
  bool is_visible = true;
  std::optional<std::u16string> label;
  std::optional<std::u16string> name;
  std::optional<std::u16string> name_attribute;
  std::optional<std::u16string> id_attribute;
  std::optional<std::u16string> value;
  std::optional<std::u16string> placeholder;
  std::optional<uint64_t> max_length;
  const std::string autocomplete_attribute;
  std::optional<AutocompleteParsingResult> parsed_autocomplete;
  const FormControlType form_control_type = FormControlType::kInputText;
  bool should_autocomplete = true;
  std::optional<bool> is_autofilled;
  std::optional<url::Origin> origin;
  std::vector<SelectOption> select_options = {};
  FieldPropertiesMask properties_mask = 0;
  FormFieldData::CheckStatus check_status =
      FormFieldData::CheckStatus::kNotCheckable;
};

// Attributes provided to the test form.
template <typename = void>
struct FormDescription {
  const std::string description_for_logging;
  std::vector<FieldDescription<>> fields;
  std::optional<LocalFrameToken> host_frame;
  std::optional<FormRendererId> renderer_id;
  const std::u16string name = u"TestForm";
  const std::string url = kFormUrl;
  const std::string action = kFormActionUrl;
  std::optional<url::Origin> main_frame_origin;
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
  // The implicit default value `std::nullopt` means no checking.
  std::optional<bool> is_complete_credit_card_form;
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
  FormDescription<> form_attributes;
  TestFormFlags<> form_flags;
  ExpectedFieldTypeValues<> expected_field_types;
};

}  // namespace internal

using FieldDescription = internal::FieldDescription<>;
using FormDescription = internal::FormDescription<>;
using FormStructureTestCase = internal::FormStructureTestCase<>;

// Describes the |form_data|. Use this in SCOPED_TRACE if other logging
// messages might refer to the form.
testing::Message DescribeFormData(const FormData& form_data);

// Returns the form field relevant to the |role|.
FormFieldData CreateFieldByRole(FieldType role);

// Creates a FormFieldData to be fed to the parser.
FormFieldData GetFormFieldData(const FieldDescription& fd);

// Creates a FormData to be fed to the parser.
FormData GetFormData(const FormDescription& test_form_attributes);

// Creates a FormData with `field_types`.
FormData GetFormData(const std::vector<FieldType>& field_types);

// Extracts the heuristic types from the form description. If the heuristic type
// is not explicitly set for a given field it is extracted from the field's
// role.
std::vector<FieldType> GetHeuristicTypes(
    const FormDescription& form_description);

// Extracts the server types from the form description. If the server type
// is not explicitly set for field it is extracted from the fiel's role.
std::vector<FieldType> GetServerTypes(const FormDescription& form_description);

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
