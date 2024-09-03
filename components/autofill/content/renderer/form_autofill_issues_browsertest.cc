// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_issues.h"

#include "base/strings/string_number_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebLocalFrame;
using blink::WebString;
using blink::mojom::GenericIssueErrorType;

namespace autofill::form_issues {
namespace {

constexpr CallTimerState kCallTimerStateDummy = {
    .call_site = CallTimerState::CallSite::kUpdateFormCache,
    .last_autofill_agent_reset = {},
    .last_dom_content_loaded = {},
};

// Checks if the provided list `form_issues` contains a certain issue type.
// Optionally checks whether the expected issue type has the specified
// `violating_attr`.
bool FormIssuesContainIssueType(const std::vector<FormIssue>& form_issues,
                                GenericIssueErrorType expected_issue,
                                const std::string& violating_attr = "") {
  return std::ranges::any_of(form_issues, [&](const auto& form_issue) {
    return form_issue.issue_type == expected_issue &&
           (violating_attr.empty() ||
            violating_attr == form_issue.violating_node_attribute.Utf8());
  });
}

class FormAutofillIssuesTest : public content::RenderViewTest {
 public:
  FormAutofillIssuesTest() = default;
  ~FormAutofillIssuesTest() override = default;

  WebFormElement WebFormElementFromHTML(const char* html,
                                        const std::string& form_id = "target") {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    WebFormElement form_target =
        GetFormElementById(web_frame->GetDocument(), form_id);
    return form_target;
  }
};

TEST_F(FormAutofillIssuesTest, FormLabelHasNeitherForNorNestedInput) {
  constexpr char kHtml[] = R"(
       <form id=target>
        <input>
        <label> A label</label>
      </form>)";
  WebFormElement form_target = WebFormElementFromHTML(kHtml);

  std::vector<FormIssue> form_issues =
      GetFormIssuesForTesting(form_target.GetFormControlElements(), {});

  EXPECT_TRUE(FormIssuesContainIssueType(
      form_issues,
      GenericIssueErrorType::kFormLabelHasNeitherForNorNestedInput));
}

TEST_F(FormAutofillIssuesTest, FormDuplicateIdForInputError) {
  constexpr char kHtml[] = R"(
      <form id=target>
        <input id=id>
        <input id=id_2>
        <input id=id>
        <input id=id>
      </form>)";
  WebFormElement form_target = WebFormElementFromHTML(kHtml);

  std::vector<FormIssue> form_issues =
      GetFormIssuesForTesting(form_target.GetFormControlElements(), {});

  int duplicated_ids_issue_count =
      base::ranges::count_if(form_issues, [](const FormIssue& form_issue) {
        return form_issue.issue_type ==
                   GenericIssueErrorType::kFormDuplicateIdForInputError &&
               form_issue.violating_node_attribute.Utf8() == "id";
      });
  EXPECT_EQ(duplicated_ids_issue_count, 3);
}

TEST_F(FormAutofillIssuesTest, FormAriaLabelledByToNonExistingId) {
  constexpr char kHtml[] = R"(
      <form id=target>
        <input aria-labelledby=non_existing>
      </form>)";
  WebFormElement form_target = WebFormElementFromHTML(kHtml);
  std::vector<FormIssue> form_issues =
      GetFormIssuesForTesting(form_target.GetFormControlElements(), {});

  std::vector<FormIssue> form_issues_2 = GetFormIssuesForTesting(
      form_target.GetFormControlElements(), form_issues);
  EXPECT_TRUE(FormIssuesContainIssueType(
      form_issues, GenericIssueErrorType::kFormAriaLabelledByToNonExistingId,
      /*violating_attr=*/"aria-labelledby"));
}

TEST_F(FormAutofillIssuesTest, FormAutocompleteAttributeEmptyError) {
  constexpr char kHtml[] = R"(
      <form id=target>
        <input autocomplete>
      </form>)";
  WebFormElement form_target = WebFormElementFromHTML(kHtml);

  std::vector<FormIssue> form_issues =
      GetFormIssuesForTesting(form_target.GetFormControlElements(), {});

  EXPECT_TRUE(FormIssuesContainIssueType(
      form_issues, GenericIssueErrorType::kFormAutocompleteAttributeEmptyError,
      /*violating_attr=*/"autocomplete"));
}

TEST_F(FormAutofillIssuesTest,
       FormInputHasWrongButWellIntendedAutocompleteValueError) {
  constexpr char kHtml[] = R"(
      <form id=target>
        <input autocomplete=address-line-1>
      </form>)";
  WebFormElement form_target = WebFormElementFromHTML(kHtml);

  std::vector<FormIssue> form_issues =
      GetFormIssuesForTesting(form_target.GetFormControlElements(), {});

  EXPECT_TRUE(FormIssuesContainIssueType(
      form_issues,
      GenericIssueErrorType::
          kFormInputHasWrongButWellIntendedAutocompleteValueError,
      /*violating_attr=*/"autocomplete"));
}

// Having an autocomplete attribute that is too large does not trigger issues,
// even if it contains a substring that would match a "honest" developer
// error/typo. This is done to avoid large string comparisons during form
// parsing.
TEST_F(
    FormAutofillIssuesTest,
    FormInputHasWrongButWellIntendedAutocompleteValueError_LargeAutocompleteString_DoNotCalculateIssue) {
  std::string autocomplete(100, 'a');
  autocomplete += "address-line-1";
  constexpr char kHtml[] = R"(
      <form id=target>
        <input>
      </form>)";
  WebFormElement form_target = WebFormElementFromHTML(kHtml);
  form_target.GetFormControlElements()[0].SetAttribute(
      "autocomplete", WebString::FromUTF8(autocomplete));
  std::vector<FormIssue> form_issues =
      GetFormIssuesForTesting(form_target.GetFormControlElements(), {});

  EXPECT_FALSE(FormIssuesContainIssueType(
      form_issues,
      GenericIssueErrorType::
          kFormInputHasWrongButWellIntendedAutocompleteValueError,
      /*violating_attr=*/"autocomplete"));
}

TEST_F(FormAutofillIssuesTest, FormEmptyIdAndNameAttributesForInputError) {
  constexpr char kHtml[] = R"(
      <form id=target>
        <input>
      </form>)";
  WebFormElement form_target = WebFormElementFromHTML(kHtml);

  std::vector<FormIssue> form_issues =
      GetFormIssuesForTesting(form_target.GetFormControlElements(), {});

  EXPECT_TRUE(FormIssuesContainIssueType(
      form_issues,
      GenericIssueErrorType::kFormEmptyIdAndNameAttributesForInputError));
}

TEST_F(FormAutofillIssuesTest,
       FormInputAssignedAutocompleteValueToIdOrNameAttributeError) {
  constexpr char kHtml[] = R"(
      <form id=target>
        <input id=country>
      </form>)";

  WebFormElement form_target = WebFormElementFromHTML(kHtml);

  std::vector<FormIssue> form_issues =
      GetFormIssuesForTesting(form_target.GetFormControlElements(), {});

  EXPECT_TRUE(FormIssuesContainIssueType(
      form_issues,
      GenericIssueErrorType::
          kFormInputAssignedAutocompleteValueToIdOrNameAttributeError,
      /*violating_attr=*/"id"));
}

TEST_F(
    FormAutofillIssuesTest,
    FormInputAssignedAutocompleteValueToIdOrNameAttributeErrorUnownedControl) {
  constexpr char kHtml[] = R"(
      <div>
        <label for="country">Country</label>
        <input id=country>
      </div>)";
  LoadHTML(kHtml);
  WebLocalFrame* web_frame = GetMainFrame();

  std::vector<FormIssue> form_issues =
      GetFormIssuesForTesting(form_util::GetOwnedAutofillableFormControls(
                                  web_frame->GetDocument(), WebFormElement()),
                              {});

  EXPECT_TRUE(FormIssuesContainIssueType(
      form_issues,
      GenericIssueErrorType::
          kFormInputAssignedAutocompleteValueToIdOrNameAttributeError,
      /*violating_attr=*/"id"));
}

TEST_F(FormAutofillIssuesTest, FormLabelForNameError) {
  constexpr char kHtml[] = R"(
      <form id=target>
        <input id=id_0 name=name_0>
        <input id=id_1 name=name_1>
        <input id=id_2 name=name_2>
        <label for=id_0>correct label</label>
        <label for=name_1>incorrect label 1</label>
        <label for=name_2>incorrect label 2</label>
      </form>)";
  LoadHTML(kHtml);
  WebLocalFrame* web_frame = GetMainFrame();
  FormData form_data = *form_util::ExtractFormData(
      web_frame->GetDocument(), WebFormElementFromHTML(kHtml),
      *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy);

  std::vector<FormIssue> form_issues =
      CheckForLabelsWithIncorrectForAttributeForTesting(
          web_frame->GetDocument(), form_data.fields(), {});

  EXPECT_EQ(form_issues.size(), 2u);
  EXPECT_TRUE(FormIssuesContainIssueType(
      form_issues, GenericIssueErrorType::kFormLabelForNameError,
      /*violating_attr=*/"for"));
}

TEST_F(FormAutofillIssuesTest, FormLabelForMatchesNonExistingIdError) {
  constexpr char kHtml[] = R"(
      <form id=target>
        <label for=non_existing />
        <input id=id_0 name=name_0>
        <label for=id_0 />"
      </form>)";
  LoadHTML(kHtml);
  WebLocalFrame* web_frame = GetMainFrame();
  FormData form_data = *form_util::ExtractFormData(
      web_frame->GetDocument(), WebFormElementFromHTML(kHtml),
      *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy);

  std::vector<FormIssue> form_issues =
      CheckForLabelsWithIncorrectForAttributeForTesting(
          web_frame->GetDocument(), form_data.fields(), {});

  EXPECT_EQ(form_issues.size(), 1u);
  EXPECT_TRUE(FormIssuesContainIssueType(
      form_issues,
      GenericIssueErrorType::kFormLabelForMatchesNonExistingIdError,
      /*violating_attr=*/"for"));
}

}  // namespace
}  // namespace autofill::form_issues
