// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "media/base/sequence.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_select_element.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/public/web/win/web_font_rendering.h"
#endif

using ::base::ASCIIToUTF16;
using ::blink::WebAutofillState;
using ::blink::WebDocument;
using ::blink::WebElement;
using ::blink::WebFormControlElement;
using ::blink::WebFormElement;
using ::blink::WebInputElement;
using ::blink::WebLocalFrame;
using ::blink::WebSelectElement;
using ::blink::WebString;
using ::testing::_;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Property;

namespace autofill::form_util {
namespace {

const std::array kFirstLastEmailIdFieldsMatchers = {
    test::FormFieldDescriptionEq(
        {.name = u"firstname", .name_attribute = u"", .id_attribute = u"firstname"}),
    test::FormFieldDescriptionEq(
        {.name = u"lastname", .name_attribute = u"", .id_attribute = u"lastname"}),
    test::FormFieldDescriptionEq(
        {.name = u"email", .name_attribute = u"", .id_attribute = u"email"})};

const std::array kCreditCardAutofilledFieldsMatchers = {
    test::FormFieldDescriptionEq({.label = u"Credit Card Number",
                                  .name = u"cc",
                                  .name_attribute = u"",
                                  .id_attribute = u"cc",
                                  .value = u"1111-2222-3333-4444",
                                  .placeholder = u"Credit Card Number",
                                  .is_autofilled_according_to_renderer = true}),
    test::FormFieldDescriptionEq({.label = u"Expiration Date",
                                  .name = u"expiration_date",
                                  .name_attribute = u"",
                                  .id_attribute = u"expiration_date",
                                  .value = u"03/2030",
                                  .placeholder = u"Expiration Date",
                                  .is_autofilled_according_to_renderer = true}),
    test::FormFieldDescriptionEq(
        {.label = u"Full Name",
         .name = u"name",
         .name_attribute = u"",
         .id_attribute = u"name",
         .value = u"John Smith",
         .placeholder = u"Full Name",
         .is_autofilled_according_to_renderer = false})};

void ApplyFieldsAction(
    const blink::WebDocument& document,
    base::span<const FormFieldData> fields,
    mojom::ActionPersistence action_persistence,
    mojom::FormActionType action_type = mojom::FormActionType::kFill) {
  std::vector<FormFieldData::FillData> filling_fields;
  filling_fields.reserve(fields.size());
  for (const FormFieldData& field : fields) {
    filling_fields.emplace_back(field);
  }
  form_util::ApplyFieldsAction(document, filling_fields, action_type,
                               action_persistence,
                               *base::MakeRefCounted<FieldDataManager>());
}

static constexpr CallTimerState kExtractFormDataCallTimerStateDummy = {
    .call_site = CallTimerState::CallSite::kUpdateFormCache,
    .last_autofill_agent_reset = {},
    .last_dom_content_loaded = {},
};
static constexpr CallTimerState kUpdateFormCacheCallTimerStateDummy = {
    .call_site = CallTimerState::CallSite::kExtractForms,
    .last_autofill_agent_reset = {},
    .last_dom_content_loaded = {},
};

FormData FindForm(const blink::WebFormControlElement& element) {
  if (auto p = FindFormAndFieldForFormControlElement(
          element, *base::MakeRefCounted<FieldDataManager>(),
          kExtractFormDataCallTimerStateDummy, /*button_titles_cache=*/nullptr,
          /*form_cache=*/{})) {
    return p->first;
  }
  return FormData();
}

class FormAutofillTest : public test::AutofillRendererTest {
 public:
  FormAutofillTest() = default;

  FormAutofillTest(const FormAutofillTest&) = delete;
  FormAutofillTest& operator=(const FormAutofillTest&) = delete;

  ~FormAutofillTest() override = default;

  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    form_cache_.emplace(&autofill_agent());

#if BUILDFLAG(IS_WIN)
    // Autofill uses the system font to render suggestion previews. On Windows
    // an extra step is required to ensure that the system font is configured.
    blink::WebFontRendering::SetMenuFontMetrics(
        blink::WebString::FromAscii("Arial"), 12);
#endif
  }

  void TearDown() override {
    form_cache_.reset();
    test::AutofillRendererTest::TearDown();
  }

  FormCache::UpdateFormCacheResult UpdateFormCache() {
    return form_cache_->UpdateFormCache(
        *base::MakeRefCounted<FieldDataManager>(),
        kUpdateFormCacheCallTimerStateDummy);
  }

  void TestFillFormMaxLength(const char* html, bool unowned) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Find the form that contains the input element.
    FormData form = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name());
      EXPECT_EQ(GURL("http://abc.com"), form.action());
    }

    EXPECT_THAT(form.fields(),
                testing::ElementsAre(
                    test::FormFieldDescriptionEq(
                        {.name = u"firstname",
                         .id_attribute = u"firstname",
                         .max_length = 5,
                         .is_autofilled_according_to_renderer = false}),
                    test::FormFieldDescriptionEq(
                        {.name = u"lastname",
                         .id_attribute = u"lastname",
                         .max_length = 7,
                         .is_autofilled_according_to_renderer = false}),
                    test::FormFieldDescriptionEq(
                        {.name = u"email",
                         .id_attribute = u"email",
                         .max_length = 9,
                         .is_autofilled_according_to_renderer = false})));

    // Fill the form.
    test_api(form).field(0).set_value(u"Brother");
    test_api(form).field(1).set_value(u"Jonathan");
    test_api(form).field(2).set_value(u"brotherj@example.com");
    test_api(form).field(0).set_is_autofilled_according_to_renderer(true);
    test_api(form).field(1).set_is_autofilled_according_to_renderer(true);
    test_api(form).field(2).set_is_autofilled_according_to_renderer(true);
    ExecuteJavaScriptForTests("document.getElementById('firstname').focus();");
    ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                      mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2 = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form2.name());
      EXPECT_EQ(GURL("http://abc.com"), form2.action());
    }

    EXPECT_THAT(form2.fields(),
                testing::ElementsAre(
                    test::FormFieldDescriptionEq(
                        {.name = u"firstname",
                         .id_attribute = u"firstname",
                         .value = u"Broth",
                         .max_length = 5,
                         .is_autofilled_according_to_renderer = true}),
                    test::FormFieldDescriptionEq(
                        {.name = u"lastname",
                         .id_attribute = u"lastname",
                         .value = u"Jonatha",
                         .max_length = 7,
                         .is_autofilled_according_to_renderer = true}),
                    test::FormFieldDescriptionEq(
                        {.name = u"email",
                         .id_attribute = u"email",
                         .value = u"brotherj@",
                         .max_length = 9,
                         .is_autofilled_according_to_renderer = true})));
  }

  void TestFillFormNegativeMaxLength(const char* html, bool unowned) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Find the form that contains the input element.
    FormData form = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name());
      EXPECT_EQ(GURL("http://abc.com"), form.action());
    }

    EXPECT_THAT(form.fields(),
                ElementsAreArray(kFirstLastEmailIdFieldsMatchers));

    // Fill the form.
    test_api(form).field(0).set_value(u"Brother");
    test_api(form).field(1).set_value(u"Jonathan");
    test_api(form).field(2).set_value(u"brotherj@example.com");
    ExecuteJavaScriptForTests("document.getElementById('firstname').focus();");
    ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                      mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2 = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form2.name());
      EXPECT_EQ(GURL("http://abc.com"), form2.action());
    }

    EXPECT_THAT(
        form2.fields(),
        testing::ElementsAre(
            test::FormFieldDescriptionEq({.name = u"firstname",
                                          .id_attribute = u"firstname",
                                          .value = u"Brother"}),
            test::FormFieldDescriptionEq({.name = u"lastname",
                                          .id_attribute = u"lastname",
                                          .value = u"Jonathan"}),
            test::FormFieldDescriptionEq({.name = u"email",
                                          .id_attribute = u"email",
                                          .value = u"brotherj@example.com"})));
  }

  void TestFillFormEmptyName(const char* html, bool unowned) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Find the form that contains the input element.
    FormData form = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name());
      EXPECT_EQ(GURL("http://abc.com"), form.action());
    }

    EXPECT_THAT(form.fields(),
                ElementsAreArray(kFirstLastEmailIdFieldsMatchers));

    // Fill the form.
    test_api(form).field(0).set_value(u"Wyatt");
    test_api(form).field(1).set_value(u"Earp");
    test_api(form).field(2).set_value(u"wyatt@example.com");
    ExecuteJavaScriptForTests("document.getElementById('firstname').focus();");
    ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                      mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2 = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form2.name());
      EXPECT_EQ(GURL("http://abc.com"), form2.action());
    }

    EXPECT_THAT(
        form2.fields(),
        testing::ElementsAre(
            test::FormFieldDescriptionEq({.name = u"firstname",
                                          .id_attribute = u"firstname",
                                          .value = u"Wyatt"}),
            test::FormFieldDescriptionEq({.name = u"lastname",
                                          .id_attribute = u"lastname",
                                          .value = u"Earp"}),
            test::FormFieldDescriptionEq({.name = u"email",
                                          .id_attribute = u"email",
                                          .value = u"wyatt@example.com"})));
  }

  void TestFillFormEmptyFormNames(const char* html, bool unowned) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    const size_t expected_size = unowned ? 1 : 2;
    ASSERT_EQ(expected_size, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("apple");

    // Find the form that contains the input element.
    FormData form = FindForm(input_element);
    if (!unowned) {
      EXPECT_TRUE(form.name().empty());
      EXPECT_EQ(GURL("http://abc.com"), form.action());
    }

    const size_t unowned_offset = unowned ? 3 : 0;
    ASSERT_EQ(unowned_offset + 3, form.fields().size());
    EXPECT_THAT(base::span(form.fields()).subspan(unowned_offset, 3U),
                testing::ElementsAre(
                    test::FormFieldDescriptionEq(
                        {.name = u"apple",
                         .id_attribute = u"apple",
                         .is_autofilled_according_to_renderer = false}),
                    test::FormFieldDescriptionEq(
                        {.name = u"banana",
                         .id_attribute = u"banana",
                         .is_autofilled_according_to_renderer = false}),
                    test::FormFieldDescriptionEq(
                        {.name = u"cantelope",
                         .id_attribute = u"cantelope",
                         .is_autofilled_according_to_renderer = false})));

    // Fill the form.
    test_api(form).field(unowned_offset + 0).set_value(u"Red");
    test_api(form).field(unowned_offset + 1).set_value(u"Yellow");
    test_api(form).field(unowned_offset + 2).set_value(u"Also Yellow");
    test_api(form)
        .field(unowned_offset + 0)
        .set_is_autofilled_according_to_renderer(true);
    test_api(form)
        .field(unowned_offset + 1)
        .set_is_autofilled_according_to_renderer(true);
    test_api(form)
        .field(unowned_offset + 2)
        .set_is_autofilled_according_to_renderer(true);
    ExecuteJavaScriptForTests("document.getElementById('apple').focus();");
    ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                      mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2 = FindForm(input_element);
    if (!unowned) {
      EXPECT_TRUE(form2.name().empty());
      EXPECT_EQ(GURL("http://abc.com"), form2.action());
    }

    ASSERT_EQ(unowned_offset + 3, form2.fields().size());
    EXPECT_THAT(base::span(form2.fields()).subspan(unowned_offset, 3U),
                testing::ElementsAre(
                    test::FormFieldDescriptionEq(
                        {.name = u"apple",
                         .id_attribute = u"apple",
                         .value = u"Red",
                         .is_autofilled_according_to_renderer = true}),
                    test::FormFieldDescriptionEq(
                        {.name = u"banana",
                         .id_attribute = u"banana",
                         .value = u"Yellow",
                         .is_autofilled_according_to_renderer = true}),
                    test::FormFieldDescriptionEq(
                        {.name = u"cantelope",
                         .id_attribute = u"cantelope",
                         .value = u"Also Yellow",
                         .is_autofilled_according_to_renderer = true})));
  }

  void TestFillFormNonEmptyField(const char* html,
                                 bool unowned,
                                 const char* initial_lastname,
                                 const char* initial_email,
                                 const char* placeholder_firstname,
                                 const char* placeholder_lastname,
                                 const char* placeholder_email) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Simulate typing by modifying the field value.
    constexpr std::string_view kNewFirstnameValue = "Wy";
    input_element.SetValue(WebString::FromAscii(kNewFirstnameValue));

    // Find the form that contains the input element.
    FormData form = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name());
      EXPECT_EQ(GURL("http://abc.com"), form.action());
    }

    const std::u16string firstname_label =
        placeholder_firstname ? ASCIIToUTF16(placeholder_firstname) : u"";
    const std::u16string firstname_placeholder =
        placeholder_firstname ? ASCIIToUTF16(placeholder_firstname) : u"";
    const std::u16string lastname_label =
        initial_lastname
            ? ASCIIToUTF16(initial_lastname)
            : (placeholder_lastname ? ASCIIToUTF16(placeholder_lastname) : u"");
    const std::u16string lastname_placeholder =
        placeholder_lastname ? ASCIIToUTF16(placeholder_lastname) : u"";
    const std::u16string email_label =
        initial_email
            ? ASCIIToUTF16(initial_email)
            : (placeholder_email ? ASCIIToUTF16(placeholder_email) : u"");
    const std::u16string email_placeholder =
        placeholder_email ? ASCIIToUTF16(placeholder_email) : u"";
    EXPECT_THAT(
        form.fields(),
        testing::ElementsAre(
            test::FormFieldDescriptionEq(
                {.label = firstname_label,
                 .name = u"firstname",
                 .id_attribute = u"firstname",
                 .value = base::UTF8ToUTF16(kNewFirstnameValue),
                 .placeholder = firstname_placeholder,
                 .is_autofilled_according_to_renderer = false}),
            test::FormFieldDescriptionEq(
                {.label = lastname_label,
                 .name = u"lastname",
                 .id_attribute = u"lastname",
                 .value = lastname_label,
                 .placeholder = !initial_lastname ? lastname_placeholder : u"",
                 .is_autofilled_according_to_renderer = false}),
            test::FormFieldDescriptionEq(
                {.label = email_label,
                 .name = u"email",
                 .id_attribute = u"email",
                 .value = email_label,
                 .placeholder = !initial_email ? email_placeholder : u"",
                 .is_autofilled_according_to_renderer = false})));

    // Preview the form and verify that the cursor position has been updated.
    test_api(form).field(0).set_value(u"Wyatt");
    test_api(form).field(1).set_value(u"Earp");
    test_api(form).field(2).set_value(u"wyatt@example.com");
    test_api(form).field(0).set_is_autofilled_according_to_renderer(true);
    test_api(form).field(1).set_is_autofilled_according_to_renderer(true);
    test_api(form).field(2).set_is_autofilled_according_to_renderer(true);
    ExecuteJavaScriptForTests("document.getElementById('firstname').focus();");
    ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                      mojom::ActionPersistence::kPreview);
    // The selection should be set after the second character.
    EXPECT_EQ(2u, input_element.SelectionStart());
    EXPECT_EQ(2u, input_element.SelectionEnd());

    // Fill the form.
    ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                      mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2 = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form2.name());
      EXPECT_EQ(GURL("http://abc.com"), form2.action());
    }

    EXPECT_THAT(form2.fields(),
                testing::ElementsAre(
                    test::FormFieldDescriptionEq(
                        {.label = firstname_placeholder,
                         .name = u"firstname",
                         .id_attribute = u"firstname",
                         .value = u"Wyatt",
                         .placeholder = firstname_placeholder,
                         .is_autofilled_according_to_renderer = true}),
                    test::FormFieldDescriptionEq(
                        {.label = lastname_placeholder,
                         .name = u"lastname",
                         .id_attribute = u"lastname",
                         .value = u"Earp",
                         .placeholder = lastname_placeholder,
                         .is_autofilled_according_to_renderer = true}),
                    test::FormFieldDescriptionEq(
                        {.label = email_placeholder,
                         .name = u"email",
                         .id_attribute = u"email",
                         .value = u"wyatt@example.com",
                         .placeholder = email_placeholder,
                         .is_autofilled_according_to_renderer = true})));

    // Verify that the cursor position has been updated.
    EXPECT_EQ(5u, input_element.SelectionStart());
    EXPECT_EQ(5u, input_element.SelectionEnd());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // We use a fresh `FormCache` in this fixture because the `AutofillAgent`'s
  // cache is used and populated by `AutofillAgent`.
  std::optional<FormCache> form_cache_;
};

// We should be able to extract a normal text field.
TEST_F(FormAutofillTest, WebFormControlElementToFormField) {
  LoadHTML(R"(<input id=element value=value>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");

  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq({.name = u"element",
                                                    .id_attribute = u"element",
                                                    .value = u"value"}));
}

// We should be able to extract a text field with autocomplete="off".
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutocompleteOff) {
  LoadHTML(R"(<input id=element value=value autocomplete=off>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);

  EXPECT_THAT(result,
              test::FormFieldDescriptionEq({.name = u"element",
                                            .id_attribute = u"element",
                                            .value = u"value",
                                            .autocomplete_attribute = "off"}));
}

// We should be able to extract a text field with maxlength specified.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldMaxLength) {
  LoadHTML(R"(<input id=element value=value maxlength=5>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq({.name = u"element",
                                                    .id_attribute = u"element",
                                                    .value = u"value",
                                                    .max_length = 5}));
}

// We should be able to extract a text field that has been autofilled.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutofilled) {
  LoadHTML(R"(<input id=element value=value>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebInputElement element = GetInputElementById("element");
  element.SetAutofillState(WebAutofillState::kAutofilled);
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq(
                          {.name = u"element",
                           .id_attribute = u"element",
                           .value = u"value",
                           .is_autofilled_according_to_renderer = true}));
}

// We should be able to extract a <select> field.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldSelect) {
  LoadHTML(R"(<select id=element>
                <option value=CA>California</option>
                <option value=TX>Texas</option>
              </select>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);

  EXPECT_THAT(result,
              test::FormFieldDescriptionEq(
                  {.name = u"element",
                   .id_attribute = u"element",
                   .value = u"CA",
                   .max_length = 0,
                   .form_control_type = FormControlType::kSelectOne,
                   .select_options = {{{.value = u"CA", .text = u"California"},
                                       {.value = u"TX", .text = u"Texas"}}}}));
}

// We copy extra attributes for the select field.
TEST_F(FormAutofillTest,
       WebFormControlElementToFormFieldSelect_ExtraAttributes) {
  LoadHTML(R"(<select id=element autocomplete=off>
                <option value=CA>California</option>
                <option value=TX>Texas</option>
              </select>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  element.SetAutofillState(WebAutofillState::kAutofilled);

  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);

  // Check that the extra attributes have been copied to `result`.
  EXPECT_THAT(result, test::FormFieldDescriptionEq(
                          {.is_focusable = true,
                           .is_visible = true,
                           .name = u"element",
                           .id_attribute = u"element",
                           .value = u"CA",
                           .max_length = 0,
                           .autocomplete_attribute = "off",
                           .form_control_type = FormControlType::kSelectOne,
                           .text_direction = base::i18n::LEFT_TO_RIGHT}));
}

// When faced with <select> field with *many* options, we should trim them to a
// reasonable number.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldLongSelect) {
  std::string html = R"(<select id=element>)";
  for (size_t i = 0; i < 2 * kMaxListSize; ++i) {
    html += base::StringPrintf("<option value='%" PRIuS
                               "'>"
                               "%" PRIuS "</option>",
                               i, i);
  }
  html += "</select>";
  LoadHTML(html.c_str());

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_TRUE(frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);

  EXPECT_TRUE(result.options().empty());
}

// Test that we use the aria-label as the content if the <option> has no text.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldSelectAriaLabel) {
  LoadHTML(
      R"(<select id=element>
         <option aria-label='usa'><img></option>
         <option aria-label='uk'><img></option>
         </select>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);
  WebFormControlElement element = GetFormControlElementById("element");

  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);
  ASSERT_EQ(2u, result.options().size());
  EXPECT_EQ(u"usa", result.options()[0].text);
  EXPECT_EQ(u"uk", result.options()[1].text);
}

// Test that the content for the <option> can be computed when the <option>s
// have nested HTML nodes.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldSelectNestedNodes) {
  LoadHTML(
      R"(<select id=element>
           <option><div><img><b>+1</b> (Canada)</div></option>
         </select>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);
  WebFormControlElement element = GetFormControlElementById("element");

  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);
  ASSERT_EQ(1u, result.options().size());
  EXPECT_EQ(u"+1 (Canada)", result.options()[0].text);
}

// We should be able to extract a <textarea> field.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldTextArea) {
  LoadHTML(R"(<textarea id=element>This element's value
spans multiple lines.</textarea>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq(
                          {.name = u"element",
                           .id_attribute = u"element",
                           .value = u"This element's value\n"
                                    u"spans multiple lines.",
                           .form_control_type = FormControlType::kTextArea}));
}

// We should be able to extract an <input type=month> field.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldMonthInput) {
  LoadHTML(R"(<input type=month id=element value='2011-12'>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result_sans_value;
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq(
                          {.name = u"element",
                           .id_attribute = u"element",
                           .value = u"2011-12",
                           .max_length = 0,
                           .form_control_type = FormControlType::kInputMonth}));
}

// We should be able to extract password fields.
TEST_F(FormAutofillTest, WebFormControlElementToPasswordFormField) {
  LoadHTML(R"(<form name=TestForm action='http://cnn.com'>
                <input type=password id=password value=secret>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("password");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);

  EXPECT_THAT(result,
              test::FormFieldDescriptionEq(
                  {.name = u"password",
                   .id_attribute = u"password",
                   .value = u"secret",
                   .form_control_type = FormControlType::kInputPassword}));
}

// We should be able to extract the autocompletetype attribute.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutocompletetype) {
  std::string html =
      R"(<input id=absent>
         <input id=empty autocomplete=''>
         <input id=off autocomplete=off>
         <input id=regular autocomplete=email>
         <input id='multi-valued' autocomplete='billing email'>
         <input id=experimental x-autocompletetype='email'>
         <input type=month id=month autocomplete='cc-exp'>
         <select id=select autocomplete=state>
           <option value=CA>California</option>
           <option value=TX>Texas</option>
         </select>
         <textarea id=textarea autocomplete='street-address'>
           Some multi-
           lined value
         </textarea>)";
  html += "<input id=malicious autocomplete='" + std::string(10000, 'x') + "'>";
  LoadHTML(html.c_str());

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  struct TestCase {
    const std::string element_id;
    FormControlType form_control_type;
    const std::string autocomplete_attribute;
    const std::string value;
  };
  TestCase test_cases[] = {
      // An absent attribute is equivalent to an empty one.
      {"absent", FormControlType::kInputText, "", ""},
      // Make sure there are no issues parsing an empty attribute.
      {"empty", FormControlType::kInputText, "", ""},
      // Make sure there are no issues parsing an attribute value that isn't a
      // type hint.
      {"off", FormControlType::kInputText, "off", ""},
      // Common case: exactly one type specified.
      {"regular", FormControlType::kInputText, "email", ""},
      // Verify that we correctly extract multiple tokens as well.
      {"multi-valued", FormControlType::kInputText, "billing email", ""},
      // Verify that <input type=month> fields are supported.
      {"month", FormControlType::kInputMonth, "cc-exp", ""},
      // We previously extracted this data from the experimental
      // 'x-autocompletetype' attribute.  Now that the field type hints are part
      // of the spec under the autocomplete attribute, we no longer support the
      // experimental version.
      {"experimental", FormControlType::kInputText, "", ""},
      // <select> elements should behave no differently from text fields here.
      {"select", FormControlType::kSelectOne, "state", "CA"},
      // <textarea> elements should also behave no differently from text fields.
      {"textarea", FormControlType::kTextArea, "street-address",
       "           Some multi-\n           lined value\n         "},
      // Very long attribute values should be replaced by a default string, to
      // prevent malicious websites from DOSing the browser process.
      {"malicious", FormControlType::kInputText, "x-max-data-length-exceeded"},
  };

  WebDocument document = frame->GetDocument();
  for (auto& test_case : test_cases) {
    WebFormControlElement element =
        GetFormControlElementById(test_case.element_id);
    FormFieldData result;
    WebFormControlElementToFormFieldForTesting(WebFormElement(), element,
                                               nullptr, &result);

    SCOPED_TRACE(test_case.element_id);

    EXPECT_THAT(
        result,
        test::FormFieldDescriptionEq(
            {.name = ASCIIToUTF16(test_case.element_id),
             .id_attribute = ASCIIToUTF16(test_case.element_id),
             .value = ASCIIToUTF16(test_case.value),
             .max_length =
                 (test_case.form_control_type == FormControlType::kInputText ||
                  test_case.form_control_type == FormControlType::kTextArea)
                     ? FormFieldData::kDefaultMaxLength
                     : 0,
             .autocomplete_attribute = test_case.autocomplete_attribute,
             .parsed_autocomplete =
                 ParseAutocompleteAttribute(test_case.autocomplete_attribute),
             .form_control_type = test_case.form_control_type}));
  }
}

TEST_F(FormAutofillTest, DetectTextDirectionFromDirectStyle) {
  LoadHTML(R"(<style>input{direction:rtl}</style>
              <form>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormAutofillTest, DetectTextDirectionFromDirectDIRAttribute) {
  LoadHTML(R"(<form>
                <input dir=rtl type=text id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormAutofillTest, DetectTextDirectionFromParentStyle) {
  LoadHTML(R"(<style>form {direction: rtl}</style>
              <form>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormAutofillTest, DetectTextDirectionFromParentDIRAttribute) {
  LoadHTML(R"(<form dir=rtl>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormAutofillTest, DetectTextDirectionWhenStyleAndDIRAttributeMixed) {
  LoadHTML(R"(<style>input{direction:ltr}</style>
              <form dir=rtl>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, result.text_direction());
}

TEST_F(FormAutofillTest, TextAlignOverridesDirection) {
  // text-align: right
  LoadHTML(R"(<style>input{direction:ltr;text-align:right}</style>
              <form>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());

  // text-align: left
  LoadHTML(R"(<style>input{direction:rtl;text-align:left}</style>
              <form>
                <input id=element>
              </form>)");

  frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  element = GetFormControlElementById("element");
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, result.text_direction());
}

TEST_F(FormAutofillTest,
       DetectTextDirectionWhenParentHasBothDIRAttributeAndStyle) {
  LoadHTML(R"(<style>form{direction:ltr}</style>
              <form dir=rtl>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, result.text_direction());
}

TEST_F(FormAutofillTest, DetectTextDirectionWhenAncestorHasInlineStyle) {
  LoadHTML(R"(<form style='direction:ltr'>
                <span dir=rtl>
                  <input id=element>
                </span>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(element.Form(), element, nullptr,
                                             &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormAutofillTest, FillFormMaxLength) {
  TestFillFormMaxLength(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname maxlength=5>
           <input id=lastname maxlength=7>
           <input id=email maxlength=9>
           <input type=submit name='reply-send' value=Send>
         </form>)",
      false);
}

TEST_F(FormAutofillTest, FillFormMaxLengthForUnownedForm) {
  TestFillFormMaxLength(
      R"(<head><title>delivery recipient info</title></head>
         <input id=firstname maxlength=5>
         <input id=lastname maxlength=7>
         <input id=email maxlength=9>
         <input type=submit name='reply-send' value=Send>)",
      true);
}

// This test uses negative values of the maxlength attribute for input elements.
// In this case, the maxlength of the input elements is set to the default
// maxlength (defined in WebKit.)
TEST_F(FormAutofillTest, FillFormNegativeMaxLength) {
  TestFillFormNegativeMaxLength(
      R"(<head><title>delivery recipient info</title></head>
         <form name=TestForm action='http://abc.com'>
           <input id=firstname maxlength='-1'>
           <input id=lastname maxlength='-10'>
           <input id=email maxlength='-13'>
           <input type=submit name='reply-send' value=Send>
         </form>)",
      false);
}

TEST_F(FormAutofillTest, FillFormNegativeMaxLengthForUnownedForm) {
  TestFillFormNegativeMaxLength(
      R"(<head><title>delivery recipient info</title></head>
         <input id=firstname maxlength='-1'>
         <input id=lastname maxlength='-10'>
         <input id=email maxlength='-13'>
         <input type=submit name='reply-send' value=Send>)",
      true);
}

TEST_F(FormAutofillTest, FillFormEmptyName) {
  TestFillFormEmptyName(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname>
           <input id=lastname>
           <input id=email>
           <input type=submit value=Send>
         </form>)",
      false);
}

TEST_F(FormAutofillTest, FillFormEmptyNameForUnownedForm) {
  TestFillFormEmptyName(
      R"(<head><title>delivery recipient info</title></head>
         <input id=firstname>
         <input id=lastname>
         <input id=email>
         <input type=submit value=Send>)",
      true);
}

TEST_F(FormAutofillTest, FillFormEmptyFormNames) {
  TestFillFormEmptyFormNames(
      R"(<form action='http://abc.com'>
           <input id=firstname>
           <input id=middlename>
           <input id=lastname>
           <input type=submit value=Send>
         </form>
         <form action='http://abc.com'>
           <input id=apple>
           <input id=banana>
           <input id=cantelope>
           <input type=submit value=Send>
         </form>)",
      false);
}

TEST_F(FormAutofillTest, FillFormEmptyFormNamesForUnownedForm) {
  TestFillFormEmptyFormNames(
      R"(<head><title>enter delivery preferences</title></head>
         <input id=firstname>
         <input id=middlename>
         <input id=lastname>
         <input id=apple>
         <input id=banana>
         <input id=cantelope>
         <input type=submit value=Send>)",
      true);
}

TEST_F(FormAutofillTest, ThreePartPhone) {
  LoadHTML(R"(<form name=TestForm action='http://cnn.com'>
                Phone:
                <input name=dayphone1>
                -
                <input name=dayphone2>
                -
                <input name=dayphone3>
                ext.:
                <input name=dayphone4>
                <input type=submit name='reply-send' value=Send>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  std::vector<WebFormElement> forms = frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());

  std::optional<FormData> form = ExtractFormData(forms.front());
  ASSERT_TRUE(form);
  EXPECT_EQ(form->name(), u"TestForm");
  EXPECT_EQ(form->action(), GURL("http://cnn.com"));

  EXPECT_THAT(
      form->fields(),
      ElementsAre(
          test::FormFieldDescriptionEq({.label = u"Phone:",
                                        .name = u"dayphone1",
                                        .name_attribute = u"dayphone1"}),
          test::FormFieldDescriptionEq({.label = u"",
                                        .name = u"dayphone2",
                                        .name_attribute = u"dayphone2"}),
          test::FormFieldDescriptionEq({.label = u"",
                                        .name = u"dayphone3",
                                        .name_attribute = u"dayphone3"}),
          test::FormFieldDescriptionEq({.label = u"ext.:",
                                        .name = u"dayphone4",
                                        .name_attribute = u"dayphone4"})));
}

TEST_F(FormAutofillTest, MaxLengthFields) {
  LoadHTML(R"(<form name=TestForm action='http://cnn.com'>
                Phone:
                <input maxlength=3 name=dayphone1>
                -
                <input maxlength=3 name=dayphone2>
                -
                <input maxlength=4 size=5 name=dayphone3>
                ext.:
                <input maxlength=5 name=dayphone4>
                <input name=default1>
                <input maxlength='-1' name=invalid1>
                <input type=submit name='reply-send' value=Send>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  std::vector<WebFormElement> forms = frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());

  std::optional<FormData> form = ExtractFormData(forms.front());
  ASSERT_TRUE(form);
  EXPECT_EQ(form->name(), u"TestForm");
  EXPECT_EQ(form->action(), GURL("http://cnn.com"));

  EXPECT_THAT(
      form->fields(),
      ElementsAre(test::FormFieldDescriptionEq({.label = u"Phone:",
                                                .name = u"dayphone1",
                                                .name_attribute = u"dayphone1",
                                                .max_length = 3}),
                  test::FormFieldDescriptionEq({.label = u"",
                                                .name = u"dayphone2",
                                                .name_attribute = u"dayphone2",
                                                .max_length = 3}),
                  test::FormFieldDescriptionEq({.label = u"",
                                                .name = u"dayphone3",
                                                .name_attribute = u"dayphone3",
                                                .max_length = 4}),
                  test::FormFieldDescriptionEq({.label = u"ext.:",
                                                .name = u"dayphone4",
                                                .name_attribute = u"dayphone4",
                                                .max_length = 5}),
                  test::FormFieldDescriptionEq(
                      {.label = u"",
                       .name = u"default1",
                       .name_attribute = u"default1",
                       .max_length = FormFieldData::kDefaultMaxLength}),
                  test::FormFieldDescriptionEq(
                      {.label = u"",
                       .name = u"invalid1",
                       .name_attribute = u"invalid1",
                       .max_length = FormFieldData::kDefaultMaxLength})));
}

// This test re-creates the experience of typing in a field then selecting a
// profile from the Autofill suggestions popup.  The field that is being typed
// into should be filled even though it's not technically empty.
TEST_F(FormAutofillTest, FillFormNonEmptyField) {
  TestFillFormNonEmptyField(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname>
           <input id=lastname>
           <input id=email>
           <input type=submit value=Send>
         </form>)",
      false, nullptr, nullptr, nullptr, nullptr, nullptr);
}

TEST_F(FormAutofillTest, FillFormNonEmptyFieldsWithDefaultValues) {
  TestFillFormNonEmptyField(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname value='Enter first name'>
           <input id=lastname value='Enter last name'>
           <input id=email value='Enter email'>
           <input type=submit value=Send>
         </form>)",
      false, "Enter last name", "Enter email", nullptr, nullptr, nullptr);
}

// Tests that loading, dynamically editing, and then autofilling the form in
// an HTML string yields a specific result.
//
// The form contains the fields first name, last name, phone, credit card
// number, city, and state, each with the placeholder attribute set.
//
// Each field's value is modified dynamically. The second one is explicitly
// marked as user-edited; the other ones are not. The third and fourth field's
// values are typical placeholder values that are expected to be ignored.
TEST_F(FormAutofillTest, FillFormModifyValues) {
  LoadHTML(R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname placeholder='First Name' value='First Name'>
           <input id=lastname placeholder='Last Name' value='Last Name'>
           <input id=phone placeholder=Phone value=Phone>
           <input id=cc placeholder='Credit Card Number' value='Credit Card'>
           <input id=city placeholder=City value=City>
           <select id=state name=state placeholder=State>
             <option selected>?</option>
             <option>AA</option>
             <option>AE</option>
             <option>AK</option>
           </select>
           <input type=submit value=Send>
         </form>)");

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebInputElement input_element = GetInputElementById("firstname");
  WebFormElement form_element = input_element.Form();
  std::vector<WebFormControlElement> control_elements =
      GetOwnedAutofillableFormControls(input_element.GetDocument(),
                                       form_element);

  ASSERT_EQ(6U, control_elements.size());
  // We now modify the values.
  // This will be ignored, the string will be sanitized into an empty string.
  control_elements[0].SetValue(WebString::FromUtf16(
      std::u16string(1, base::i18n::kLeftToRightMark) + u"     "));

  // This will be considered as a value entered by the user.
  control_elements[1].SetValue(WebString::FromUtf16(u"Earp"));
  control_elements[1].SetUserHasEditedTheField(true);

  // This will be ignored, the string will be sanitized into an empty string.
  control_elements[2].SetValue(WebString::FromUtf16(u"(___)-___-____"));

  // This will be ignored, the string will be sanitized into an empty string.
  control_elements[3].SetValue(WebString::FromUtf16(u"____-____-____-____"));

  // This will be ignored, because it's injected by the website and not the
  // user.
  control_elements[4].SetValue(WebString::FromUtf16(u"Enter your city.."));

  control_elements[5].SetValue(WebString::FromUtf16(u"AK"));

  // Find the form that contains the input element.
  FormData form = FindForm(input_element);
  EXPECT_EQ(u"TestForm", form.name());
  EXPECT_EQ(GURL("http://abc.com"), form.action());

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(6U, fields.size());

  // Preview the form and verify that the cursor position has been updated.
  test_api(form).field(0).set_value(u"Wyatt");
  test_api(form).field(1).set_value(u"Earpagus");
  test_api(form).field(2).set_value(u"888-123-4567");
  test_api(form).field(3).set_value(u"1111-2222-3333-4444");
  test_api(form).field(4).set_value(u"Montreal");
  test_api(form).field(5).set_value(u"AA");
  test_api(form).field(0).set_is_autofilled_according_to_renderer(true);
  test_api(form).field(1).set_is_autofilled_according_to_renderer(true);
  test_api(form).field(2).set_is_autofilled_according_to_renderer(true);
  test_api(form).field(3).set_is_autofilled_according_to_renderer(true);
  test_api(form).field(4).set_is_autofilled_according_to_renderer(true);
  test_api(form).field(5).set_is_autofilled_according_to_renderer(true);
  ExecuteJavaScriptForTests("document.getElementById('firstname').focus();");
  ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                    mojom::ActionPersistence::kPreview);

  // Fill the form.
  ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                    mojom::ActionPersistence::kFill);

  // Find the newly-filled form that contains the input element.
  FormData form2 = FindForm(input_element);
  EXPECT_EQ(u"TestForm", form2.name());
  EXPECT_EQ(GURL("http://abc.com"), form2.action());

  EXPECT_THAT(form2.fields(),
              ElementsAre(test::FormFieldDescriptionEq(
                              {.label = u"First Name",
                               .name = u"firstname",
                               .id_attribute = u"firstname",
                               .value = u"Wyatt",
                               .placeholder = u"First Name",
                               .is_autofilled_according_to_renderer = true}),
                          test::FormFieldDescriptionEq(
                              {.label = u"Last Name",
                               .name = u"lastname",
                               .id_attribute = u"lastname",
                               .value = u"Earp",
                               .placeholder = u"Last Name",
                               .is_autofilled_according_to_renderer = false}),
                          test::FormFieldDescriptionEq(
                              {.label = u"Phone",
                               .name = u"phone",
                               .id_attribute = u"phone",
                               .value = u"888-123-4567",
                               .placeholder = u"Phone",
                               .is_autofilled_according_to_renderer = true}),
                          test::FormFieldDescriptionEq(
                              {.label = u"Credit Card Number",
                               .name = u"cc",
                               .id_attribute = u"cc",
                               .value = u"1111-2222-3333-4444",
                               .placeholder = u"Credit Card Number",
                               .is_autofilled_according_to_renderer = true}),
                          test::FormFieldDescriptionEq(
                              {.label = u"City",
                               .name = u"city",
                               .id_attribute = u"city",
                               .value = u"Montreal",
                               .placeholder = u"City",
                               .is_autofilled_according_to_renderer = true}),
                          test::FormFieldDescriptionEq(
                              {.label = u"State",
                               .name = u"state",
                               .name_attribute = u"state",
                               .id_attribute = u"state",
                               .value = u"AA",
                               .placeholder = u"State",
                               .is_autofilled_according_to_renderer = true})));
}

// Similar to test case `FillFormModifyValues`.
TEST_F(FormAutofillTest, FillFormModifyInitiatingValue) {
  LoadHTML(R"(<form name=TestForm action='http://abc.com'>
           <input id=cc placeholder='Credit Card Number' value='Credit Card'>
           <input id=expiration_date placeholder='Expiration Date'
                  value='Expiration Date'>
           <input id=name placeholder='Full Name' value='Full Name'>
           <input type=submit value=Send>
         </form>)");

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebInputElement input_element = GetInputElementById("cc");
  WebFormElement form_element = input_element.Form();
  std::vector<WebFormControlElement> control_elements =
      GetOwnedAutofillableFormControls(input_element.GetDocument(),
                                       form_element);

  ASSERT_EQ(3U, control_elements.size());
  // We now modify the values.
  // This will be ignored.
  control_elements[0].SetValue(WebString::FromUtf16(u"____-____-____-____"));
  // This will be ignored.
  control_elements[1].SetValue(WebString::FromUtf16(u"____/__"));
  control_elements[2].SetValue(WebString::FromUtf16(u"John Smith"));
  control_elements[2].SetUserHasEditedTheField(true);

  // Find the form that contains the input element.
  FormData form = FindForm(input_element);
  EXPECT_EQ(u"TestForm", form.name());
  EXPECT_EQ(GURL("http://abc.com"), form.action());

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(3U, fields.size());

  // Preview the form and verify that the cursor position has been updated.
  test_api(form).field(0).set_value(u"1111-2222-3333-4444");
  test_api(form).field(1).set_value(u"03/2030");
  test_api(form).field(2).set_value(u"Susan Smith");
  test_api(form).field(0).set_is_autofilled_according_to_renderer(true);
  test_api(form).field(1).set_is_autofilled_according_to_renderer(true);
  test_api(form).field(2).set_is_autofilled_according_to_renderer(true);
  ExecuteJavaScriptForTests("document.getElementById('cc').focus();");
  ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                    mojom::ActionPersistence::kPreview);
  // The selection should be set after the 19th character.
  EXPECT_EQ(19u, input_element.SelectionStart());
  EXPECT_EQ(19u, input_element.SelectionEnd());

  // Fill the form.
  ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                    mojom::ActionPersistence::kFill);

  // Find the newly-filled form that contains the input element.
  FormData form2 = FindForm(input_element);
  EXPECT_EQ(u"TestForm", form2.name());
  EXPECT_EQ(GURL("http://abc.com"), form2.action());

  EXPECT_THAT(form2.fields(),
              ElementsAreArray(kCreditCardAutofilledFieldsMatchers));

  // Verify that the cursor position has been updated.
  EXPECT_EQ(19u, input_element.SelectionStart());
  EXPECT_EQ(19u, input_element.SelectionEnd());
}

// Similar to test case `FillFormModifyValues`.
TEST_F(FormAutofillTest, FillFormJSModifiesUserInputValue) {
  LoadHTML(R"(<form name=TestForm action='http://abc.com'>
           <input id=cc placeholder='Credit Card Number' value='Credit Card'>
           <input id=expiration_date placeholder='Expiration Date'
                  value='Expiration Date'>
           <input id=name placeholder='Full Name' value='Full Name'>
           <input type=submit value=Send>
         </form>)");

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebInputElement input_element = GetInputElementById("cc");
  WebFormElement form_element = input_element.Form();
  std::vector<WebFormControlElement> control_elements =
      GetOwnedAutofillableFormControls(input_element.GetDocument(),
                                       form_element);

  ASSERT_EQ(3U, control_elements.size());
  // We now modify the values.
  // This will be ignored.
  control_elements[0].SetValue(WebString::FromUtf16(u"____-____-____-____"));
  // This will be ignored.
  control_elements[1].SetValue(WebString::FromUtf16(u"____/__"));
  control_elements[2].SetValue(WebString::FromUtf16(u"john smith"));
  control_elements[2].SetUserHasEditedTheField(true);

  // Sometimes the JS modifies the value entered by the user.
  ExecuteJavaScriptForTests(
      "document.getElementById('name').value = 'John Smith';");

  // Find the form that contains the input element.
  FormData form = FindForm(input_element);
  EXPECT_EQ(u"TestForm", form.name());
  EXPECT_EQ(GURL("http://abc.com"), form.action());

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(3U, fields.size());

  // Preview the form and verify that the cursor position has been updated.
  test_api(form).field(0).set_value(u"1111-2222-3333-4444");
  test_api(form).field(1).set_value(u"03/2030");
  test_api(form).field(2).set_value(u"Susan Smith");
  test_api(form).field(0).set_is_autofilled_according_to_renderer(true);
  test_api(form).field(1).set_is_autofilled_according_to_renderer(true);
  test_api(form).field(2).set_is_autofilled_according_to_renderer(true);
  ExecuteJavaScriptForTests("document.getElementById('cc').focus();");
  ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                    mojom::ActionPersistence::kPreview);
  // The selection should be set after the 19th character.
  EXPECT_EQ(19u, input_element.SelectionStart());
  EXPECT_EQ(19u, input_element.SelectionEnd());

  // Fill the form.
  ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                    mojom::ActionPersistence::kFill);

  // Find the newly-filled form that contains the input element.
  FormData form2 = FindForm(input_element);
  EXPECT_EQ(u"TestForm", form2.name());
  EXPECT_EQ(GURL("http://abc.com"), form2.action());

  EXPECT_THAT(form2.fields(),
              ElementsAreArray(kCreditCardAutofilledFieldsMatchers));

  // Verify that the cursor position has been updated.
  EXPECT_EQ(19u, input_element.SelectionStart());
  EXPECT_EQ(19u, input_element.SelectionEnd());
}

TEST_F(FormAutofillTest, FillFormNonEmptyFieldsWithPlaceholderValues) {
  TestFillFormNonEmptyField(
      R"(<form name=TestForm action='http://abc.com' method=POST>
           <input id=firstname placeholder='First Name' value='First Name'>
           <input id=lastname placeholder='Last Name' value='Last Name'>
           <input id=email placeholder=Email value=Email>
           <input type=submit value=Send>
         </form>)",
      false, nullptr, nullptr, "First Name", "Last Name", "Email");
}

TEST_F(FormAutofillTest, FillFormNonEmptyFieldForUnownedForm) {
  TestFillFormNonEmptyField(
      R"(<head><title>delivery recipient info</title></head>
         <input id=firstname>
         <input id=lastname>
         <input id=email>
         <input type=submit value=Send>)",
      true, nullptr, nullptr, nullptr, nullptr, nullptr);
}

TEST_F(FormAutofillTest, UndoAutofill) {
  LoadHTML(R"(
    <form id=form_id>
        <input id=text_id_1>
        <input id=text_id_2>
        <select id=select_id_1>
          <option value=undo_select_option_1>Foo</option>
          <option value=autofill_select_option_1>Bar</option>
        </select>
        <select id=select_id_2>
          <option value=undo_select_option_2>Foo</option>
          <option value=autofill_select_option_2>Bar</option>
        </select>
      </form>
  )");
  WebFormControlElement text_element_1 = GetFormControlElementById("text_id_1");
  WebFormControlElement text_element_2 = GetFormControlElementById("text_id_2");
  text_element_1.SetAutofillValue("autofill_text_1",
                                  WebAutofillState::kAutofilled);
  text_element_2.SetAutofillValue("autofill_text_2",
                                  WebAutofillState::kAutofilled);

  WebFormControlElement select_element_1 =
      GetFormControlElementById("select_id_1");
  WebFormControlElement select_element_2 =
      GetFormControlElementById("select_id_2");
  select_element_1.SetAutofillValue("autofill_select_option_1",
                                    WebAutofillState::kAutofilled);
  select_element_2.SetAutofillValue("autofill_select_option_2",
                                    WebAutofillState::kAutofilled);

  auto HasAutofillValue = [](const WebString& value,
                             WebAutofillState autofill_state) {
    return ::testing::AllOf(
        ::testing::Property(&WebFormControlElement::Value, value),
        ::testing::Property(&WebFormControlElement::GetAutofillState,
                            autofill_state));
  };
  ASSERT_THAT(text_element_1, HasAutofillValue("autofill_text_1",
                                               WebAutofillState::kAutofilled));
  ASSERT_THAT(text_element_2, HasAutofillValue("autofill_text_2",
                                               WebAutofillState::kAutofilled));
  ASSERT_THAT(select_element_1,
              HasAutofillValue("autofill_select_option_1",
                               WebAutofillState::kAutofilled));
  ASSERT_THAT(select_element_2,
              HasAutofillValue("autofill_select_option_2",
                               WebAutofillState::kAutofilled));

  std::vector<WebFormElement> forms =
      GetMainFrame()->GetDocument().GetTopLevelForms();
  EXPECT_EQ(1U, forms.size());

  std::optional<FormData> form = ExtractFormData(forms.front());
  ASSERT_TRUE(form);

  ASSERT_EQ(form->fields().size(), 4u);
  std::vector<FormFieldData> undo_fields;
  for (size_t i = 0; i < 4; i += 2) {
    std::u16string type = i == 0 ? u"text" : u"select_option";
    test_api(*form).field(i).set_value(u"undo_" + type + u"_1");
    test_api(*form).field(i).set_is_autofilled_according_to_renderer(false);
    undo_fields.push_back(form->fields()[i]);
  }

  form->set_fields(undo_fields);
  ExecuteJavaScriptForTests("document.getElementById('text_id_1').focus();");
  ApplyFieldsAction(text_element_1.GetDocument(), form->fields(),
                    mojom::ActionPersistence::kFill,
                    mojom::FormActionType::kUndo);
  EXPECT_THAT(text_element_1,
              HasAutofillValue("undo_text_1", WebAutofillState::kNotFilled));
  EXPECT_THAT(text_element_2, HasAutofillValue("autofill_text_2",
                                               WebAutofillState::kAutofilled));
  EXPECT_THAT(select_element_1, HasAutofillValue("undo_select_option_1",
                                                 WebAutofillState::kNotFilled));
  EXPECT_THAT(select_element_2,
              HasAutofillValue("autofill_select_option_2",
                               WebAutofillState::kAutofilled));
}

}  // namespace
}  // namespace autofill::form_util
