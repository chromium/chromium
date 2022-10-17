// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORM_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORM_TEST_UTILS_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {
namespace test {

namespace {

// Default form url.
constexpr char kFormUrl[] = "http://example.com/form.html";

// Default form action url.
constexpr char kFormActionUrl[] = "http://example.com/submit.html";

}  // namespace

namespace internal {

// Expected FormFieldData and type predictions are constructed based on these
// descriptions.
template <typename = void>
struct FieldDescription {
  ServerFieldType role = ServerFieldType::EMPTY_TYPE;
  // If the server type is not set explcitly, it is assumed to be given by the
  // role.
  absl::optional<ServerFieldType> server_type;
  // If the heuristic type is not set explcitly, it is assumed to be given by
  // the role.
  absl::optional<ServerFieldType> heuristic_type;
  absl::optional<LocalFrameToken> host_frame;
  absl::optional<FieldRendererId> unique_renderer_id;
  bool is_focusable = true;
  bool is_visible = true;
  absl::optional<std::u16string> label;
  absl::optional<std::u16string> name;
  absl::optional<std::u16string> value;
  absl::optional<std::u16string> placeholder;
  const std::string autocomplete_attribute;
  absl::optional<AutocompleteParsingResult> parsed_autocomplete;
  const std::string form_control_type = "text";
  bool should_autocomplete = true;
  absl::optional<bool> is_autofilled;
  absl::optional<url::Origin> origin;
  std::vector<SelectOption> select_options = {};
  FieldPropertiesMask properties_mask = 0;
};

// Attributes provided to the test form.
template <typename = void>
struct FormDescription {
  const std::string description_for_logging;
  std::vector<FieldDescription<>> fields;
  absl::optional<LocalFrameToken> host_frame;
  absl::optional<FormRendererId> unique_renderer_id;
  const std::u16string name = u"TestForm";
  const std::string url = kFormUrl;
  const std::string action = kFormActionUrl;
  absl::optional<url::Origin> main_frame_origin;
  bool is_form_tag = true;
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
  bool has_author_specified_upi_vpa_hint = false;
  // The implicit default value `absl::nullopt` means no checking.
  absl::optional<bool> is_complete_credit_card_form;
  absl::optional<int> field_count;
  absl::optional<int> autofill_count;
  absl::optional<int> section_count;
  absl::optional<int> response_field_count;
};

// Expected field type values to be verified with the test form.
template <typename = void>
struct ExpectedFieldTypeValues {
  std::vector<HtmlFieldType> expected_html_type = {};
  std::vector<ServerFieldType> expected_heuristic_type = {};
  std::vector<ServerFieldType> expected_overall_type = {};
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
FormFieldData CreateFieldByRole(ServerFieldType role);

// Creates a FormData to be fed to the parser.
FormData GetFormData(const FormDescription& test_form_attributes);

// Extracts the heuristic types from the form description. If the heuristic type
// is not explicitly set for a given field it is extracted from the field's
// role.
std::vector<ServerFieldType> GetHeuristicTypes(
    const FormDescription& form_description);

// Extracts the server types from the form description. If the server type
// is not explicitly set for field it is extracted from the fiel's role.
std::vector<ServerFieldType> GetServerTypes(
    const FormDescription& form_description);

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
