// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_issues.h"

#include "base/compiler_specific.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

using blink::WebDocument;
using blink::WebString;
using blink::mojom::GenericIssueErrorType;
using testing::_;
using testing::AnyNumber;
using testing::MockFunction;

namespace autofill::form_issues {
namespace {

using MockEmit = MockFunction<void(const WebDocument& document,
                                   GenericIssueErrorType issue_type,
                                   int violating_node,
                                   WebString violating_node_attribute)>;

template <typename R, typename... Args>
auto as_invokable(MockFunction<R(Args...)>& fun LIFETIME_BOUND) {
  return [&fun](Args... args) { fun.Call(std::forward<Args>(args)...); };
}

class FormAutofillIssuesTest : public content::RenderViewTest {
 public:
  FormAutofillIssuesTest() = default;
  ~FormAutofillIssuesTest() override = default;

  WebDocument GetDocument() { return GetMainFrame()->GetDocument(); }

  FormData ExtractTargetForm(std::string_view id) {
    static constexpr CallTimerState kCallTimerStateDummy = {
        .call_site = CallTimerState::CallSite::kUpdateFormCache,
        .last_autofill_agent_reset = {},
        .last_dom_content_loaded = {},
    };
    return *form_util::ExtractFormData(
        GetDocument(), GetFormElementById(GetDocument(), id),
        *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy,
        /*button_titles_cache=*/nullptr);
  }
};

TEST_F(FormAutofillIssuesTest, FormLabelHasNeitherForNorNestedInputError) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
       <form>
        <input>
        <label> A label</label>
      </form>)");
  MockEmit emit;
  EXPECT_CALL(emit, Call).Times(AnyNumber());
  EXPECT_CALL(emit, Call(GetDocument(),
                         kFormLabelHasNeitherForNorNestedInputError, _, _));
  EmitFormIssues(GetDocument(), {}, as_invokable(emit));
}

TEST_F(FormAutofillIssuesTest, FormDuplicateIdForInputError) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <form>
        <input id=id>
        <input id=id_2>
        <input id=id>
        <input id=id>
      </form>)");
  MockEmit emit;
  EXPECT_CALL(emit, Call(GetDocument(), kFormDuplicateIdForInputError, _,
                         WebString(u"id")))
      .Times(3);
  EmitFormIssues(GetDocument(), {}, as_invokable(emit));
}

TEST_F(FormAutofillIssuesTest, FormAriaLabelledByToNonExistingIdError) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <form>
        <input aria-labelledby=non_existing>
      </form>)");
  MockEmit emit;
  EXPECT_CALL(emit, Call).Times(AnyNumber());
  EXPECT_CALL(emit, Call(GetDocument(), kFormAriaLabelledByToNonExistingIdError,
                         _, WebString(u"aria-labelledby")));
  EmitFormIssues(GetDocument(), {}, as_invokable(emit));
}

TEST_F(FormAutofillIssuesTest, FormAutocompleteAttributeEmptyError) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <form>
        <input autocomplete>
      </form>)");
  MockEmit emit;
  EXPECT_CALL(emit, Call).Times(AnyNumber());
  EXPECT_CALL(emit, Call(GetDocument(), kFormAutocompleteAttributeEmptyError, _,
                         WebString(u"autocomplete")));
  EmitFormIssues(GetDocument(), {}, as_invokable(emit));
}

TEST_F(FormAutofillIssuesTest,
       FormInputHasWrongButWellIntendedAutocompleteValueError) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <form>
        <input autocomplete=address-line-1>
      </form>)");
  MockEmit emit;
  EXPECT_CALL(emit, Call).Times(AnyNumber());
  EXPECT_CALL(emit,
              Call(GetDocument(),
                   kFormInputHasWrongButWellIntendedAutocompleteValueError, _,
                   WebString(u"autocomplete")));
  EmitFormIssues(GetDocument(), {}, as_invokable(emit));
}

// Having an autocomplete attribute that is too large does not trigger issues,
// even if it contains a substring that would match a "honest" developer
// error/typo. This is done to avoid large string comparisons during form
// parsing.
TEST_F(
    FormAutofillIssuesTest,
    FormInputHasWrongButWellIntendedAutocompleteValueError_LargeAutocompleteString_DoNotCalculateIssue) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <form>
        <input id=t>
      </form>)");
  GetElementById(GetDocument(), "t")
      .SetAttribute("autocomplete", WebString::FromUTF8(std::string(100, 'a') +
                                                        "address-line-1"));
  MockEmit emit;
  EXPECT_CALL(emit,
              Call(GetDocument(),
                   kFormInputHasWrongButWellIntendedAutocompleteValueError, _,
                   WebString(u"autocomplete")))
      .Times(0);
  EmitFormIssues(GetDocument(), {}, as_invokable(emit));
}

TEST_F(FormAutofillIssuesTest, FormEmptyIdAndNameAttributesForInputError) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <form>
        <input>
      </form>)");
  MockEmit emit;
  EXPECT_CALL(emit, Call(GetDocument(),
                         kFormEmptyIdAndNameAttributesForInputError, _, _));
  EmitFormIssues(GetDocument(), {}, as_invokable(emit));
}

TEST_F(FormAutofillIssuesTest,
       FormInputAssignedAutocompleteValueToIdOrNameAttributeError) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <form>
        <input id=country>
      </form>)");
  MockEmit emit;
  EXPECT_CALL(
      emit, Call(GetDocument(),
                 kFormInputAssignedAutocompleteValueToIdOrNameAttributeError, _,
                 WebString("id")));
  EmitFormIssues(GetDocument(), {}, as_invokable(emit));
}

TEST_F(
    FormAutofillIssuesTest,
    FormInputAssignedAutocompleteValueToIdOrNameAttributeErrorUnownedControl) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <div>
        <label for="country">Country</label>
        <input id=country>
      </div>)");
  MockEmit emit;
  EXPECT_CALL(
      emit, Call(GetDocument(),
                 kFormInputAssignedAutocompleteValueToIdOrNameAttributeError, _,
                 WebString("id")));
  EmitFormIssues(GetDocument(), {}, as_invokable(emit));
}

TEST_F(FormAutofillIssuesTest, FormLabelForNameError) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <form id=f>
        <input id=id_0 name=name_0>
        <input id=id_1 name=name_1>
        <input id=id_2 name=name_2>
        <label for=id_0>correct label</label>
        <label for=name_1>incorrect label 1</label>
        <label for=name_2>incorrect label 2</label>
      </form>)");
  MockEmit emit;
  EXPECT_CALL(emit,
              Call(GetDocument(), kFormLabelForNameError, _, WebString("for")))
      .Times(2);
  EmitFormIssues(GetDocument(), base::span_from_ref(ExtractTargetForm("f")),
                 as_invokable(emit));
}

TEST_F(FormAutofillIssuesTest, FormLabelForMatchesNonExistingIdError) {
  using enum GenericIssueErrorType;
  LoadHTML(R"(
      <form id=f>
        <label for=non_existing />
        <input id=id_0 name=name_0>
        <label for=id_0 />
      </form>)");
  MockEmit emit;
  EXPECT_CALL(emit, Call(GetDocument(), kFormLabelForMatchesNonExistingIdError,
                         _, WebString("for")));
  EmitFormIssues(GetDocument(), base::span_from_ref(ExtractTargetForm("f")),
                 as_invokable(emit));
}

}  // namespace
}  // namespace autofill::form_issues
