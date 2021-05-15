// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORM_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FORM_TEST_UTILS_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {
namespace test {

namespace {

// Default label assigned to fields.
constexpr char16_t kLabelText[] = u"label";

// Default name attribute assigned to fields.
constexpr char16_t kNameText[] = u"name";

// Default form url.
constexpr char kFormUrl[] = "http://example.com/form.html";

// Default form action url.
constexpr char kFormActionUrl[] = "http://example.com/submit.html";

}  // namespace

namespace internal {

// Expected FormFieldData are constructed based on these descriptions.
template <typename = void>
struct FieldDataDescription {
  ServerFieldType role = ServerFieldType::EMPTY_TYPE;
  bool is_focusable = true;
  const base::StringPiece16 label = kLabelText;
  const base::StringPiece16 name = kNameText;
  absl::optional<const char16_t*> value;
  const base::StringPiece autocomplete_attribute;
  const base::StringPiece form_control_type = "text";
  bool should_autocomplete = true;
  absl::optional<bool> is_autofilled;
};

// Attributes provided to the test form.
template <typename = void>
struct TestFormAttributes {
  const base::StringPiece description_for_logging;
  std::vector<FieldDataDescription<>> fields;
  absl::optional<FormRendererId> unique_renderer_id;
  const base::StringPiece16 name = u"TestForm";
  const base::StringPiece url = kFormUrl;
  const base::StringPiece action = kFormActionUrl;
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
  // first value denotes whether the comparison is to be done while second
  // denotes EXPECT_TRUE for true and EXPECT_FALSE for false.
  std::pair<bool, bool> is_complete_credit_card_form = {false, false};
  // The implicit default value `absl::nullopt` means no checking.
  absl::optional<int> field_count;
  absl::optional<int> autofill_count;
  absl::optional<int> section_count;
  absl::optional<int> response_field_count;
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
  TestFormAttributes<> form_attributes;
  TestFormFlags<> form_flags;
  ExpectedFieldTypeValues<> expected_field_types;
};

}  // namespace internal

using FieldDataDescription = internal::FieldDataDescription<>;
using TestFormAttributes = internal::TestFormAttributes<>;
using FormStructureTestCase = internal::FormStructureTestCase<>;

// Describes the |form_data|. Use this in SCOPED_TRACE if other logging
// messages might refer to the form.
testing::Message DescribeFormData(const FormData& form_data);

// Returns the form field relevant to the |role|.
FormFieldData CreateFieldByRole(ServerFieldType role);

// Creates a FormData to be fed to the parser.
FormData GetFormData(const TestFormAttributes& test_form_attributes);

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
