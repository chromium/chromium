// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/ranges/algorithm.h"
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
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
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

using base::ASCIIToUTF16;
using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;
using testing::_;
using testing::ElementsAre;
using testing::Field;
using testing::Optional;
using testing::Pair;
using testing::Property;

namespace autofill::form_util {
namespace {

struct AutofillFieldCase {
  FormControlType form_control_type;
  const char* const id_attribute;
  const char* const initial_value;
  bool should_be_autofilled;   // Whether the filed should be autofilled.
  const char* const autofill_value;  // The value being used to fill the field.
  const char* const expected_value;  // The expected value after Autofill
                                     // or Preview.
};

struct WebElementDescriptor {
  enum RetrievalMethod {
    CSS_SELECTOR,
    ID,
    NONE,
  };

  // Information to retrieve element with.
  std::string descriptor;

  // Which retrieval method to use.
  RetrievalMethod retrieval_method = NONE;
};

const char kFormHtml[] =
    R"(<form name=TestForm action='http://abc.com'>
         <input id=firstname>
         <input id=lastname>
         <input type=hidden id=imhidden>
         <input id=notempty value=Hi>
         <input autocomplete=off id=noautocomplete>
         <input disabled=disabled id=notenabled>
         <input readonly id=readonly>
         <input style='visibility: hidden' id=invisible>
         <input style='display: none' id=displaynone>
         <input type=month id=month>
         <input type=month id='month-nonempty' value='2011-12'>
         <select id=select>
           <option></option>
           <option value=CA>California</option>
           <option value=TX>Texas</option>
         </select>
         <select id='select-nonempty'>
           <option value=CA selected>California</option>
           <option value=TX>Texas</option>
         </select>
         <select id='select-unchanged'>
           <option value=CA selected>California</option>
           <option value=TX>Texas</option>
         </select>
         <select id='select-displaynone' style='display:none'>
           <option value=CA selected>California</option>
           <option value=TX>Texas</option>
         </select>
         <textarea id=textarea></textarea>
         <textarea id='textarea-nonempty'>Go&#10;away!</textarea>
         <input type=submit name='reply-send' value=Send>
       </form>)";

// This constant uses a mixed-case title tag to be sure that the title match is
// not case-sensitive. Other tests in this file use an all-lower title tag.
const char kUnownedFormHtml[] =
    R"(<head><title>Enter Shipping Info</title></head>
       <input id=firstname>
       <input id=lastname>
       <input type=hidden id=imhidden>
       <input id=notempty value=Hi>
       <input autocomplete=off id=noautocomplete>
       <input disabled=disabled id=notenabled>
       <input readonly id=readonly>
       <input style='visibility: hidden' id=invisible>
       <input style='display: none' id=displaynone>
       <input type=month id=month>
       <input type=month id='month-nonempty' value='2011-12'>
       <select id=select>
         <option></option>
         <option value=CA>California</option>
         <option value=TX>Texas</option>
       </select>
       <select id='select-nonempty'>
         <option value=CA selected>California</option>
         <option value=TX>Texas</option>
       </select>
       <select id='select-unchanged'>
         <option value=CA selected>California</option>
         <option value=TX>Texas</option>
       </select>
       <select id='select-displaynone' style='display:none'>
         <option value=CA selected>California</option>
         <option value=TX>Texas</option>
       </select>
       <textarea id=textarea></textarea>
       <textarea id='textarea-nonempty'>Go&#10;away!</textarea>
       <input type=submit name='reply-send' value=Send>)";

// This constant has no title tag, and should be passed to
// LoadHTMLWithURLOverride to test the detection of unowned forms by URL.
const char kUnownedUntitledFormHtml[] =
    R"(<input id=firstname>
       <input id=lastname>
       <input type=hidden id=imhidden>
       <input id=notempty value=Hi>
       <input autocomplete=off id=noautocomplete>
       <input disabled=disabled id=notenabled>
       <input readonly id=readonly>
       <input style='visibility: hidden' id=invisible>
       <input style='display: none' id=displaynone>
       <input type=month id=month>
       <input type=month id='month-nonempty' value='2011-12'>
       <select id=select>
         <option></option>
         <option value=CA>California</option>
         <option value=TX>Texas</option>
       </select>
       <select id='select-nonempty'>
         <option value=CA selected>California</option>
         <option value=TX>Texas</option>
       </select>
       <select id='select-unchanged'>
         <option value=CA selected>California</option>
         <option value=TX>Texas</option>
       </select>
       <select id='select-displaynone' style='display:none'>
         <option value=CA selected>California</option>
         <option value=TX>Texas</option>
       </select>
       <textarea id=textarea></textarea>
       <textarea id='textarea-nonempty'>Go&#10;away!</textarea>
       <input type=submit name='reply-send' value=Send>)";

// This constant does not have a title tag, but should match an unowned form
// anyway because it is not English.
const char kUnownedNonEnglishFormHtml[] =
    R"(<html lang=fr>
         <input id=firstname>
         <input id=lastname>
         <input type=hidden id=imhidden>
         <input id=notempty value=Hi>
         <input autocomplete=off id=noautocomplete>
         <input disabled=disabled id=notenabled>
         <input readonly id=readonly>
         <input style='visibility: hidden' id=invisible>
         <input style='display: none' id=displaynone>
         <input type=month id=month>
         <input type=month id='month-nonempty' value='2011-12'>
         <select id=select>
           <option></option>
           <option value=CA>California</option>
           <option value=TX>Texas</option>
         </select>
         <select id='select-nonempty'>
           <option value=CA selected>California</option>
           <option value=TX>Texas</option>
         </select>
         <select id='select-unchanged'>
           <option value=CA selected>California</option>
           <option value=TX>Texas</option>
         </select>
         <select id='select-displaynone' style='display:none'>
           <option value=CA selected>California</option>
           <option value=TX>Texas</option>
         </select>
         <textarea id=textarea></textarea>
         <textarea id='textarea-nonempty'>Go&#10;away!</textarea>
         <input type=submit name='reply-send' value=Send>
       </html>)";

std::string RetrievalMethodToString(
    const WebElementDescriptor::RetrievalMethod& method) {
  switch (method) {
    case WebElementDescriptor::CSS_SELECTOR:
      return "CSS_SELECTOR";
    case WebElementDescriptor::ID:
      return "ID";
    case WebElementDescriptor::NONE:
      return "NONE";
  }
  NOTREACHED_IN_MIGRATION();
  return "UNKNOWN";
}

bool ClickElement(const WebDocument& document,
                  const WebElementDescriptor& element_descriptor) {
  WebString web_descriptor = WebString::FromUTF8(element_descriptor.descriptor);
  blink::WebElement element;

  switch (element_descriptor.retrieval_method) {
    case WebElementDescriptor::CSS_SELECTOR: {
      element = document.QuerySelector(web_descriptor);
      break;
    }
    case WebElementDescriptor::ID:
      element = document.GetElementById(web_descriptor);
      break;
    case WebElementDescriptor::NONE:
      return true;
  }

  if (!element) {
    DVLOG(1) << "Could not find "
             << element_descriptor.descriptor
             << " by "
             << RetrievalMethodToString(element_descriptor.retrieval_method)
             << ".";
    return false;
  }

  element.SimulateClick();
  return true;
}

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

constexpr CallTimerState kCallTimerStateDummy = {
    .call_site = CallTimerState::CallSite::kUpdateFormCache,
    .last_autofill_agent_reset = {},
    .last_dom_content_loaded = {},
};

FormData FindForm(const blink::WebFormControlElement& element) {
  if (auto p = FindFormAndFieldForFormControlElement(
          element, *base::MakeRefCounted<FieldDataManager>(),
          kCallTimerStateDummy, {})) {
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
        blink::WebString::FromASCII("Arial"), 12);
#endif
  }

  void TearDown() override {
    form_cache_.reset();
    test::AutofillRendererTest::TearDown();
  }

  std::optional<FormData> ExtractFormData(
      WebFormElement form,
      DenseSet<ExtractOption> extract_options = {}) {
    return form_util::ExtractFormData(GetDocument(), form,
                                      *base::MakeRefCounted<FieldDataManager>(),
                                      kCallTimerStateDummy, extract_options);
  }

  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
  FindFormAndFieldForFormControlElement(
      WebFormControlElement control,
      DenseSet<ExtractOption> extract_options = {}) {
    return form_util::FindFormAndFieldForFormControlElement(
        control, *base::MakeRefCounted<FieldDataManager>(),
        kCallTimerStateDummy, extract_options);
  }

  FormCache::UpdateFormCacheResult UpdateFormCache() {
    return form_cache_->UpdateFormCache(
        *base::MakeRefCounted<FieldDataManager>());
  }

  void ExpectLabels(const char* html,
                    const std::vector<std::u16string>& id_attributes,
                    const std::vector<std::u16string>& name_attributes,
                    const std::vector<std::u16string>& labels,
                    const std::vector<std::u16string>& names,
                    const std::vector<std::u16string>& values) {
    ASSERT_EQ(labels.size(), id_attributes.size());
    ASSERT_EQ(labels.size(), name_attributes.size());
    ASSERT_EQ(labels.size(), names.size());
    ASSERT_EQ(labels.size(), values.size());

    std::vector<FormFieldData> fields;
    for (size_t i = 0; i < labels.size(); ++i) {
      FormFieldData expected;
      expected.set_id_attribute(id_attributes[i]);
      expected.set_name_attribute(name_attributes[i]);
      expected.set_label(labels[i]);
      expected.set_name(names[i]);
      expected.set_value(values[i]);
      expected.set_form_control_type(FormControlType::kInputText);
      expected.set_max_length(FormFieldData::kDefaultMaxLength);
      fields.push_back(expected);
    }
    ExpectLabelsAndTypes(html, fields);
  }

  void ExpectLabelsAndTypes(const char* html,
                            const std::vector<FormFieldData>& fields) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    const FormData& form = forms[0];
    EXPECT_EQ(u"TestForm", form.name());
    EXPECT_EQ(GURL("http://cnn.com"), form.action());
    ASSERT_EQ(fields.size(), form.fields().size());

    for (size_t i = 0; i < fields.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));
      EXPECT_FORM_FIELD_DATA_EQUALS(fields[i], form.fields()[i]);
    }
  }

  // Use this validator when the test HTML uses the id attribute instead of
  // the name attribute to identify the input fields. Otherwise, this is the
  // same text structure as ExpectJohnSmithLabelsAndNameAttributes().
  void ExpectJohnSmithLabelsAndIdAttributes(const char* html) {
    std::vector<std::u16string> id_attributes, name_attributes, labels, names,
        values;

    id_attributes.push_back(u"firstname");
    name_attributes.emplace_back();
    labels.push_back(u"First name:");
    names.push_back(id_attributes.back());
    values.push_back(u"John");

    id_attributes.push_back(u"lastname");
    name_attributes.emplace_back();
    labels.push_back(u"Last name:");
    names.push_back(id_attributes.back());
    values.push_back(u"Smith");

    id_attributes.push_back(u"email");
    name_attributes.emplace_back();
    labels.push_back(u"Email:");
    names.push_back(id_attributes.back());
    values.push_back(u"john@example.com");

    ExpectLabels(html, id_attributes, name_attributes, labels, names, values);
  }

  // Use this validator when the test HTML uses the name attribute instead of
  // the id attribute to identify the input fields. Otherwise, this is the same
  // text structure as ExpectJohnSmithLabelsAndIdAttributes().
  void ExpectJohnSmithLabelsAndNameAttributes(const char* html) {
    std::vector<std::u16string> id_attributes, name_attributes, labels, names,
        values;
    id_attributes.emplace_back();
    name_attributes.push_back(u"firstname");
    labels.push_back(u"First name:");
    names.push_back(name_attributes.back());
    values.push_back(u"John");

    id_attributes.emplace_back();
    name_attributes.push_back(u"lastname");
    labels.push_back(u"Last name:");
    names.push_back(name_attributes.back());
    values.push_back(u"Smith");

    id_attributes.emplace_back();
    name_attributes.push_back(u"email");
    labels.push_back(u"Email:");
    names.push_back(name_attributes.back());
    values.push_back(u"john@example.com");
    ExpectLabels(html, id_attributes, name_attributes, labels, names, values);
  }

  typedef WebString (*GetValueFunction)(WebFormControlElement element);

  // Test FormFill* functions.
  void TestFormFillFunctions(const char* html,
                             bool unowned,
                             const char* url_override,
                             const AutofillFieldCase* field_cases,
                             size_t number_of_field_cases,
                             mojom::ActionPersistence action_persistence,
                             GetValueFunction get_value_function) {
    if (url_override) {
      LoadHTMLWithUrlOverride(html, url_override);
    } else {
      LoadHTML(html);
    }

    // Find the form to fill.
    WebInputElement input_element = GetInputElementById("firstname");
    FormData form = FindForm(input_element);
    const std::vector<FormFieldData>& fields = form.fields();
    ASSERT_EQ(number_of_field_cases, fields.size());

    // Verify the initial state of the form and setup filling data.
    for (size_t i = 0; i < number_of_field_cases; ++i) {
      SCOPED_TRACE(base::StringPrintf("Verify initial value for field %s",
                                      field_cases[i].id_attribute));
      EXPECT_EQ(field_cases[i].form_control_type,
                fields[i].form_control_type());
      EXPECT_EQ(base::UTF8ToUTF16(field_cases[i].id_attribute),
                fields[i].id_attribute());
      EXPECT_EQ(base::UTF8ToUTF16(field_cases[i].initial_value),
                fields[i].value());
      test_api(form).field(i).set_value(
          ASCIIToUTF16(field_cases[i].autofill_value));
      test_api(form).field(i).set_is_autofilled(true);
    }

    // Fill and validate.
    ExecuteJavaScriptForTests("document.getElementById('firstname').focus();");
    ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                      action_persistence);
    for (size_t i = 0; i < number_of_field_cases; ++i) {
      ValidateFilledField(field_cases[i], get_value_function,
                          action_persistence);
    }
  }

  // Validate an Autofilled field.
  void ValidateFilledField(const AutofillFieldCase& field_case,
                           GetValueFunction get_value_function,
                           mojom::ActionPersistence action_persistence) {
    SCOPED_TRACE(base::StringPrintf("Verify autofilled value for field %s",
                                    field_case.id_attribute));
    WebFormControlElement element =
        GetFormControlElementById(field_case.id_attribute);
    EXPECT_EQ(field_case.expected_value, get_value_function(element).Utf8());
    EXPECT_EQ(field_case.should_be_autofilled,
              action_persistence == mojom::ActionPersistence::kFill
                  ? element.IsAutofilled()
                  : element.IsPreviewed());
  }

  void TestFillForm(const char* html, bool unowned, const char* url_override) {
    static const AutofillFieldCase field_cases[] = {
        // Fields: form_control_type, name, initial_value,
        // autocomplete_attribute, should_be_autofilled, autofill_value,
        // expected_value.
        // Regular empty fields (firstname & lastname) should be autofilled.
        {FormControlType::kInputText, "firstname", "", true, "filled firstname",
         "filled firstname"},
        {FormControlType::kInputText, "lastname", "", true, "filled lastname",
         "filled lastname"},
        {FormControlType::kInputText, "notempty", "Hi", true, "filled notempty",
         "filled notempty"},
        {FormControlType::kInputText, "noautocomplete", "", true,
         "filled noautocomplete", "filled noautocomplete"},
        // Disabled fields should not be autofilled.
        {FormControlType::kInputText, "notenabled", "", false,
         "filled notenabled", ""},
        // Readonly fields should not be autofilled.
        {FormControlType::kInputText, "readonly", "", false, "filled readonly",
         ""},
        // Fields with "visibility: hidden" should not be autofilled.
        {FormControlType::kInputText, "invisible", "", false,
         "filled invisible", ""},
        // Fields with "display:none" should not be autofilled.
        {FormControlType::kInputText, "displaynone", "", false,
         "filled displaynone", ""},
        // Regular <input type=month> should be autofilled.
        {FormControlType::kInputMonth, "month", "", true, "2017-11", "2017-11"},
        {FormControlType::kInputMonth, "month-nonempty", "2011-12", true,
         "2017-11", "2017-11"},
        // Regular select fields should be autofilled.
        {FormControlType::kSelectOne, "select", "", true, "TX", "TX"},
        // Select fields should be autofilled even if they already have a
        // non-empty value.
        {FormControlType::kSelectOne, "select-nonempty", "CA", true, "TX",
         "TX"},
        {FormControlType::kSelectOne, "select-unchanged", "CA", true, "CA",
         "CA"},
        // Select fields that are not focusable should be filled.
        {FormControlType::kSelectOne, "select-displaynone", "CA", true, "TX",
         "TX"},
        // Regular textarea elements should be autofilled.
        {FormControlType::kTextArea, "textarea", "", true,
         "some multi-\nline value", "some multi-\nline value"},
        {FormControlType::kTextArea, "textarea-nonempty", "Go\naway!", true,
         "some multi-\nline value", "some multi-\nline value"},
    };
    TestFormFillFunctions(html, unowned, url_override, field_cases,
                          std::size(field_cases),
                          mojom::ActionPersistence::kFill, &GetValueWrapper);
    WebInputElement firstname = GetInputElementById("firstname");
    EXPECT_EQ(16u, firstname.SelectionStart());
    EXPECT_EQ(16u, firstname.SelectionEnd());
  }

  void TestPreviewForm(const char* html, bool unowned,
                       const char* url_override) {
    static const AutofillFieldCase field_cases[] = {
        // Normal empty fields should be previewed.
        {FormControlType::kInputText, "firstname", "", true,
         "suggested firstname", "suggested firstname"},
        {FormControlType::kInputText, "lastname", "", true,
         "suggested lastname", "suggested lastname"},
        {FormControlType::kInputText, "notempty", "Hi", true,
         "suggested notempty", "suggested notempty"},
        {FormControlType::kInputText, "noautocomplete", "", true,
         "filled noautocomplete", "filled noautocomplete"},
        // Disabled fields should not be previewed.
        {FormControlType::kInputText, "notenabled", "", false,
         "suggested notenabled", ""},
        // Readonly fields should not be previewed.
        {FormControlType::kInputText, "readonly", "", false,
         "suggested readonly", ""},
        // Fields with "visibility: hidden" should not be previewed.
        {FormControlType::kInputText, "invisible", "", false,
         "suggested invisible", ""},
        // Fields with "display:none" should not previewed.
        {FormControlType::kInputText, "displaynone", "", false,
         "suggested displaynone", ""},
        // Regular <input type=month> should be previewed.
        {FormControlType::kInputMonth, "month", "", true, "2017-11", "2017-11"},
        {FormControlType::kInputMonth, "month-nonempty", "2011-12", true,
         "2017-11", "2017-11"},
        // Regular select fields should be previewed.
        {FormControlType::kSelectOne, "select", "", true, "TX", "TX"},
        // Select fields should be previewed even if they already have a
        // non-empty value.
        {FormControlType::kSelectOne, "select-nonempty", "CA", true, "TX",
         "TX"},
        // Select fields should be previewed even if no suggestion is passed.
        {FormControlType::kSelectOne, "select-unchanged", "CA", true, "", ""},
        // Select fields that are not focusable should always be filled.
        {FormControlType::kSelectOne, "select-displaynone", "CA", true, "CA",
         "CA"},
        // Normal textarea elements should be previewed.
        {FormControlType::kTextArea, "textarea", "", true,
         "suggested multi-\nline value", "suggested multi-\nline value"},
        // Nonempty textarea elements should not be previewed.
        {FormControlType::kTextArea, "textarea-nonempty", "Go\naway!", true,
         "suggested multi-\nline value", "suggested multi-\nline value"},
    };
    TestFormFillFunctions(
        html, unowned, url_override, field_cases, std::size(field_cases),
        mojom::ActionPersistence::kPreview, &GetSuggestedValueWrapper);

    // Verify preview selection.
    WebInputElement firstname = GetInputElementById("firstname");
    // Since the suggestion is previewed as a placeholder, there should be no
    // selected text.
    EXPECT_EQ(0u, firstname.SelectionStart());
    EXPECT_EQ(0u, firstname.SelectionEnd());
  }

  void TestFindFormForInputElement(const char* html, bool unowned) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Find the form and verify it's the correct form.
    FormData form = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name());
      EXPECT_EQ(GURL("http://abc.com"), form.action());
    }

    const std::vector<FormFieldData>& fields = form.fields();
    ASSERT_EQ(4U, fields.size());

    FormFieldData expected;
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"John");
    expected.set_label(u"John");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Smith");
    expected.set_label(u"Smith");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"john@example.com");
    expected.set_label(u"john@example.com");
    expected.set_autocomplete_attribute("off");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
    expected.set_autocomplete_attribute({});

    expected.set_id_attribute(u"phone");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"1.800.555.1234");
    expected.set_label(u"1.800.555.1234");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);
  }

  void TestFindFormForTextAreaElement(const char* html, bool unowned) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the textarea element we want to find.
    WebElement element = GetDocument().GetElementById("street-address");
    WebFormControlElement textarea_element =
        element.To<WebFormControlElement>();

    // Find the form and verify it's the correct form.
    FormData form = FindForm(textarea_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name());
      EXPECT_EQ(GURL("http://abc.com"), form.action());
    }

    const std::vector<FormFieldData>& fields = form.fields();
    ASSERT_EQ(4U, fields.size());

    FormFieldData expected;

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"John");
    expected.set_label(u"John");
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Smith");
    expected.set_label(u"Smith");
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"john@example.com");
    expected.set_label(u"john@example.com");
    expected.set_autocomplete_attribute("off");
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
    expected.set_autocomplete_attribute({});

    expected.set_id_attribute(u"street-address");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"123 Fantasy Ln.\nApt. 42");
    expected.set_label({});
    expected.set_form_control_type(FormControlType::kTextArea);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);
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

    const std::vector<FormFieldData>& fields = form.fields();
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;
    expected.set_form_control_type(FormControlType::kInputText);

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    expected.set_max_length(5);
    expected.set_is_autofilled(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    expected.set_max_length(7);
    expected.set_is_autofilled(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    expected.set_max_length(9);
    expected.set_is_autofilled(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

    // Fill the form.
    test_api(form).field(0).set_value(u"Brother");
    test_api(form).field(1).set_value(u"Jonathan");
    test_api(form).field(2).set_value(u"brotherj@example.com");
    test_api(form).field(0).set_is_autofilled(true);
    test_api(form).field(1).set_is_autofilled(true);
    test_api(form).field(2).set_is_autofilled(true);
    ExecuteJavaScriptForTests("document.getElementById('firstname').focus();");
    ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                      mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2 = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form2.name());
      EXPECT_EQ(GURL("http://abc.com"), form2.action());
    }

    const std::vector<FormFieldData>& fields2 = form2.fields();
    ASSERT_EQ(3U, fields2.size());

    expected.set_form_control_type(FormControlType::kInputText);

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Broth");
    expected.set_max_length(5);
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Jonatha");
    expected.set_max_length(7);
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"brotherj@");
    expected.set_max_length(9);
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
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

    const std::vector<FormFieldData>& fields = form.fields();
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

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

    const std::vector<FormFieldData>& fields2 = form2.fields();
    ASSERT_EQ(3U, fields2.size());

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Brother");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Jonathan");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"brotherj@example.com");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
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

    const std::vector<FormFieldData>& fields = form.fields();
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

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

    const std::vector<FormFieldData>& fields2 = form2.fields();
    ASSERT_EQ(3U, fields2.size());

    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Wyatt");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Earp");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"wyatt@example.com");
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
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

    const std::vector<FormFieldData>& fields = form.fields();
    const size_t unowned_offset = unowned ? 3 : 0;
    ASSERT_EQ(unowned_offset + 3, fields.size());

    FormFieldData expected;
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);

    expected.set_id_attribute(u"apple");
    expected.set_name(expected.id_attribute());
    expected.set_is_autofilled(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[unowned_offset]);

    expected.set_id_attribute(u"banana");
    expected.set_name(expected.id_attribute());
    expected.set_is_autofilled(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[unowned_offset + 1]);

    expected.set_id_attribute(u"cantelope");
    expected.set_name(expected.id_attribute());
    expected.set_is_autofilled(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[unowned_offset + 2]);

    // Fill the form.
    test_api(form).field(unowned_offset + 0).set_value(u"Red");
    test_api(form).field(unowned_offset + 1).set_value(u"Yellow");
    test_api(form).field(unowned_offset + 2).set_value(u"Also Yellow");
    test_api(form).field(unowned_offset + 0).set_is_autofilled(true);
    test_api(form).field(unowned_offset + 1).set_is_autofilled(true);
    test_api(form).field(unowned_offset + 2).set_is_autofilled(true);
    ExecuteJavaScriptForTests("document.getElementById('apple').focus();");
    ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                      mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2 = FindForm(input_element);
    if (!unowned) {
      EXPECT_TRUE(form2.name().empty());
      EXPECT_EQ(GURL("http://abc.com"), form2.action());
    }

    const std::vector<FormFieldData>& fields2 = form2.fields();
    ASSERT_EQ(unowned_offset + 3, fields2.size());

    expected.set_id_attribute(u"apple");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Red");
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[unowned_offset + 0]);

    expected.set_id_attribute(u"banana");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Yellow");
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[unowned_offset + 1]);

    expected.set_id_attribute(u"cantelope");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Also Yellow");
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[unowned_offset + 2]);
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
    input_element.SetValue(WebString::FromASCII("Wy"));

    // Find the form that contains the input element.
    FormData form = FindForm(input_element);
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name());
      EXPECT_EQ(GURL("http://abc.com"), form.action());
    }

    const std::vector<FormFieldData>& fields = form.fields();
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Wy");
    if (placeholder_firstname) {
      expected.set_label(ASCIIToUTF16(placeholder_firstname));
      expected.set_placeholder(ASCIIToUTF16(placeholder_firstname));
    }
    expected.set_is_autofilled(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    if (initial_lastname) {
      expected.set_label(ASCIIToUTF16(initial_lastname));
      expected.set_value(ASCIIToUTF16(initial_lastname));
    } else if (placeholder_lastname) {
      expected.set_label(ASCIIToUTF16(placeholder_lastname));
      expected.set_placeholder(ASCIIToUTF16(placeholder_lastname));
      expected.set_value(ASCIIToUTF16(placeholder_lastname));
    } else {
      expected.set_label({});
      expected.set_value({});
    }
    expected.set_is_autofilled(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    if (initial_email) {
      expected.set_label(ASCIIToUTF16(initial_email));
      expected.set_value(ASCIIToUTF16(initial_email));
    } else if (placeholder_email) {
      expected.set_label(ASCIIToUTF16(placeholder_email));
      expected.set_placeholder(ASCIIToUTF16(placeholder_email));
      expected.set_value(ASCIIToUTF16(placeholder_email));
    } else {
      expected.set_label({});
      expected.set_value({});
    }
    expected.set_is_autofilled(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

    // Preview the form and verify that the cursor position has been updated.
    test_api(form).field(0).set_value(u"Wyatt");
    test_api(form).field(1).set_value(u"Earp");
    test_api(form).field(2).set_value(u"wyatt@example.com");
    test_api(form).field(0).set_is_autofilled(true);
    test_api(form).field(1).set_is_autofilled(true);
    test_api(form).field(2).set_is_autofilled(true);
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

    const std::vector<FormFieldData>& fields2 = form2.fields();
    ASSERT_EQ(3U, fields2.size());

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Wyatt");
    if (placeholder_firstname) {
      expected.set_label(ASCIIToUTF16(placeholder_firstname));
      expected.set_placeholder(ASCIIToUTF16(placeholder_firstname));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Earp");
    if (placeholder_lastname) {
      expected.set_label(ASCIIToUTF16(placeholder_lastname));
      expected.set_placeholder(ASCIIToUTF16(placeholder_lastname));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.set_id_attribute(u"email");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"wyatt@example.com");
    if (placeholder_email) {
      expected.set_label(ASCIIToUTF16(placeholder_email));
      expected.set_placeholder(ASCIIToUTF16(placeholder_email));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(5u, input_element.SelectionStart());
    EXPECT_EQ(5u, input_element.SelectionEnd());
  }

  // Tests that loading, dynamically editing, and then autofilling the form in
  // `html` yields a specific result.
  //
  // The form is expected to have a very specific structure. In particular, its
  // fields are supposed to be first name, last name, phone, credit card number,
  // city, and state, whose placeholder attributes are supposed to match the
  // `placeholder_*` arguments.
  //
  // Each field's value is modified dynamically. The second one is explicitly
  // marked as user-edited; the other ones are not. The third and fourth field's
  // values are typical placeholder values are expected to be ignored.
  //
  // TODO(crbug.com/41483772): Remove implicit assumptions about `html` from
  // this function.
  void TestFillFormAndModifyValues(const char* html,
                                   const char* placeholder_firstname,
                                   const char* placeholder_lastname,
                                   const char* placeholder_phone,
                                   const char* placeholder_creditcard,
                                   const char* placeholder_city,
                                   const char* placeholder_state) {
    LoadHTML(html);

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
    control_elements[0].SetValue(WebString::FromUTF16(
        std::u16string(1, base::i18n::kLeftToRightMark) + u"     "));

    // This will be considered as a value entered by the user.
    control_elements[1].SetValue(WebString::FromUTF16(u"Earp"));
    control_elements[1].SetUserHasEditedTheField(true);

    // This will be ignored, the string will be sanitized into an empty string.
    control_elements[2].SetValue(WebString::FromUTF16(u"(___)-___-____"));

    // This will be ignored, the string will be sanitized into an empty string.
    control_elements[3].SetValue(WebString::FromUTF16(u"____-____-____-____"));

    // This will be ignored, because it's injected by the website and not the
    // user.
    control_elements[4].SetValue(WebString::FromUTF16(u"Enter your city.."));

    control_elements[5].SetValue(WebString::FromUTF16(u"AK"));

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
    test_api(form).field(0).set_is_autofilled(true);
    test_api(form).field(1).set_is_autofilled(true);
    test_api(form).field(2).set_is_autofilled(true);
    test_api(form).field(3).set_is_autofilled(true);
    test_api(form).field(4).set_is_autofilled(true);
    test_api(form).field(5).set_is_autofilled(true);
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

    const std::vector<FormFieldData>& fields2 = form2.fields();
    ASSERT_EQ(6U, fields2.size());

    FormFieldData expected;
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);

    expected.set_id_attribute(u"firstname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Wyatt");
    if (placeholder_firstname) {
      expected.set_label(ASCIIToUTF16(placeholder_firstname));
      expected.set_placeholder(ASCIIToUTF16(placeholder_firstname));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    expected.set_is_user_edited(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    // The last name field is not filled, because there is a value in it.
    expected.set_id_attribute(u"lastname");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Earp");
    if (placeholder_lastname) {
      expected.set_label(ASCIIToUTF16(placeholder_lastname));
      expected.set_placeholder(ASCIIToUTF16(placeholder_lastname));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(false);
    expected.set_is_user_edited(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.set_id_attribute(u"phone");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"888-123-4567");
    if (placeholder_phone) {
      expected.set_label(ASCIIToUTF16(placeholder_phone));
      expected.set_placeholder(ASCIIToUTF16(placeholder_phone));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    expected.set_is_user_edited(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

    expected.set_id_attribute(u"cc");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"1111-2222-3333-4444");
    if (placeholder_creditcard) {
      expected.set_label(ASCIIToUTF16(placeholder_creditcard));
      expected.set_placeholder(ASCIIToUTF16(placeholder_creditcard));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    expected.set_is_user_edited(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[3]);

    expected.set_id_attribute(u"city");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"Montreal");
    if (placeholder_city) {
      expected.set_label(ASCIIToUTF16(placeholder_city));
      expected.set_placeholder(ASCIIToUTF16(placeholder_city));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    expected.set_is_user_edited(false);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[4]);

    expected.set_form_control_type(FormControlType::kSelectOne);
    expected.set_id_attribute(u"state");
    expected.set_name_attribute(u"state");
    expected.set_name(expected.name_attribute());
    expected.set_value(u"AA");
    if (placeholder_state) {
      expected.set_label(ASCIIToUTF16(placeholder_state));
      expected.set_placeholder(ASCIIToUTF16(placeholder_state));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    expected.set_is_user_edited(false);
    expected.set_max_length(0);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[5]);
  }

  // Similar to TestFillFormAndModifyValues().
  // TODO(crbug.com/41483772): Remove implicit assumptions about `html` from
  // this function.
  void TestFillFormAndModifyInitiatingValue(const char* html,
                                            const char* placeholder_creditcard,
                                            const char* placeholder_expiration,
                                            const char* placeholder_name) {
    LoadHTML(html);

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
    control_elements[0].SetValue(WebString::FromUTF16(u"____-____-____-____"));
    // This will be ignored.
    control_elements[1].SetValue(WebString::FromUTF16(u"____/__"));
    control_elements[2].SetValue(WebString::FromUTF16(u"John Smith"));
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
    test_api(form).field(0).set_is_autofilled(true);
    test_api(form).field(1).set_is_autofilled(true);
    test_api(form).field(2).set_is_autofilled(true);
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

    const std::vector<FormFieldData>& fields2 = form2.fields();
    ASSERT_EQ(3U, fields2.size());

    FormFieldData expected;
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);

    expected.set_id_attribute(u"cc");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"1111-2222-3333-4444");
    if (placeholder_creditcard) {
      expected.set_label(ASCIIToUTF16(placeholder_creditcard));
      expected.set_placeholder(ASCIIToUTF16(placeholder_creditcard));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    expected.set_id_attribute(u"expiration_date");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"03/2030");
    if (placeholder_expiration) {
      expected.set_label(ASCIIToUTF16(placeholder_expiration));
      expected.set_placeholder(ASCIIToUTF16(placeholder_expiration));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.set_id_attribute(u"name");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"John Smith");
    if (placeholder_name) {
      expected.set_label(ASCIIToUTF16(placeholder_name));
      expected.set_placeholder(ASCIIToUTF16(placeholder_name));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(false);
    expected.set_is_user_edited(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(19u, input_element.SelectionStart());
    EXPECT_EQ(19u, input_element.SelectionEnd());
  }

  // Similar to TestFillFormAndModifyValues().
  // TODO(crbug.com/41483772): Remove implicit assumptions about `html` from
  // this function.
  void TestFillFormJSModifiesUserInputValue(const char* html,
                                            const char* placeholder_creditcard,
                                            const char* placeholder_expiration,
                                            const char* placeholder_name) {
    LoadHTML(html);

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
    control_elements[0].SetValue(WebString::FromUTF16(u"____-____-____-____"));
    // This will be ignored.
    control_elements[1].SetValue(WebString::FromUTF16(u"____/__"));
    control_elements[2].SetValue(WebString::FromUTF16(u"john smith"));
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
    test_api(form).field(0).set_is_autofilled(true);
    test_api(form).field(1).set_is_autofilled(true);
    test_api(form).field(2).set_is_autofilled(true);
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

    const std::vector<FormFieldData>& fields2 = form2.fields();
    ASSERT_EQ(3U, fields2.size());

    FormFieldData expected;
    expected.set_form_control_type(FormControlType::kInputText);
    expected.set_max_length(FormFieldData::kDefaultMaxLength);

    expected.set_id_attribute(u"cc");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"1111-2222-3333-4444");
    if (placeholder_creditcard) {
      expected.set_label(ASCIIToUTF16(placeholder_creditcard));
      expected.set_placeholder(ASCIIToUTF16(placeholder_creditcard));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    expected.set_id_attribute(u"expiration_date");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"03/2030");
    if (placeholder_expiration) {
      expected.set_label(ASCIIToUTF16(placeholder_expiration));
      expected.set_placeholder(ASCIIToUTF16(placeholder_expiration));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.set_id_attribute(u"name");
    expected.set_name(expected.id_attribute());
    expected.set_value(u"John Smith");
    if (placeholder_name) {
      expected.set_label(ASCIIToUTF16(placeholder_name));
      expected.set_placeholder(ASCIIToUTF16(placeholder_name));
    } else {
      expected.set_label({});
      expected.set_placeholder({});
    }
    expected.set_is_autofilled(false);
    expected.set_is_user_edited(true);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(19u, input_element.SelectionStart());
    EXPECT_EQ(19u, input_element.SelectionEnd());
  }

  void TestClearPreviewedElements(const char* html) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    std::vector<std::pair<WebFormControlElement, WebAutofillState>> elements;
    elements.emplace_back(GetInputElementById("firstname"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("lastname"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("email"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("email2"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("phone"),
                          WebAutofillState::kNotFilled);
    WebInputElement firstname = elements[0].first.To<WebInputElement>();
    WebInputElement lastname = elements[1].first.To<WebInputElement>();

    // Set the auto-filled attribute.
    for (auto& [element, state] : elements) {
      element.SetAutofillState(WebAutofillState::kPreviewed);
    }

    // Set the suggested values on two of the elements.
    firstname.SetSuggestedValue(WebString::FromASCII("Wyatt"));
    lastname.SetSuggestedValue(WebString::FromASCII("Earp"));
    elements[2].first.SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[3].first.SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[4].first.SetSuggestedValue(WebString::FromASCII("650-777-9999"));

    std::vector<bool> is_value_empty(elements.size());
    for (size_t i = 0; i < elements.size(); ++i) {
      is_value_empty[i] = elements[i].first.Value().IsEmpty();
    }

    // Clear the previewed fields.
    ClearPreviewedElements(elements);

    // Verify the previewed fields are cleared.
    for (size_t i = 0; i < elements.size(); ++i) {
      WebFormControlElement& element = elements[i].first;
      SCOPED_TRACE(testing::Message() << "Element " << i);
      EXPECT_EQ(element.Value().IsEmpty(), is_value_empty[i]);
      EXPECT_TRUE(element.SuggestedValue().IsEmpty());
      EXPECT_FALSE(element.IsAutofilled());
    }
  }

  void TestClearPreviewedFormWithNonEmptyInitiatingNode(const char* html) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    std::vector<std::pair<WebFormControlElement, WebAutofillState>> elements;
    elements.emplace_back(GetInputElementById("firstname"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("lastname"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("email"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("email2"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("phone"),
                          WebAutofillState::kNotFilled);
    WebInputElement firstname = elements[0].first.To<WebInputElement>();
    WebInputElement lastname = elements[1].first.To<WebInputElement>();

    // Set the auto-filled attribute.
    for (auto& [element, state] : elements) {
      element.SetAutofillState(WebAutofillState::kPreviewed);
    }

    // Set the suggested values on all of the elements.
    firstname.SetSuggestedValue(WebString::FromASCII("Wyatt"));
    lastname.SetSuggestedValue(WebString::FromASCII("Earp"));
    elements[2].first.SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[3].first.SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[4].first.SetSuggestedValue(WebString::FromASCII("650-777-9999"));

    // Clear the previewed fields.
    ClearPreviewedElements(elements);

    // Fields with non-empty values are restored.
    EXPECT_EQ(u"W", firstname.Value().Utf16());
    EXPECT_TRUE(firstname.SuggestedValue().IsEmpty());
    EXPECT_FALSE(firstname.IsAutofilled());

    // Verify the previewed fields are cleared.
    for (size_t i = 1; i < elements.size(); ++i) {
      WebFormControlElement& element = elements[i].first;
      SCOPED_TRACE(testing::Message() << "Element " << i);
      EXPECT_TRUE(element.Value().IsEmpty());
      EXPECT_TRUE(element.SuggestedValue().IsEmpty());
      EXPECT_FALSE(element.IsAutofilled());
    }
  }

  void TestClearPreviewedFormWithAutofilledInitiatingNode(const char* html) {
    LoadHTML(html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    ASSERT_EQ(1U, forms.size());

    std::vector<std::pair<WebFormControlElement, WebAutofillState>> elements;
    elements.emplace_back(GetInputElementById("firstname"),
                          WebAutofillState::kAutofilled);
    elements.emplace_back(GetInputElementById("lastname"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("email"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("email2"),
                          WebAutofillState::kNotFilled);
    elements.emplace_back(GetInputElementById("phone"),
                          WebAutofillState::kNotFilled);
    WebInputElement firstname = elements[0].first.To<WebInputElement>();
    WebInputElement lastname = elements[1].first.To<WebInputElement>();

    // Set the auto-filled attribute.
    for (auto& [element, state] : elements) {
      element.SetAutofillState(WebAutofillState::kPreviewed);
    }

    // Set the suggested values on all of the elements.
    firstname.SetSuggestedValue(WebString::FromASCII("Wyatt"));
    lastname.SetSuggestedValue(WebString::FromASCII("Earp"));
    elements[2].first.SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[3].first.SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[4].first.SetSuggestedValue(WebString::FromASCII("650-777-9999"));

    // Clear the previewed fields.
    ClearPreviewedElements(elements);

    // Fields with non-empty values are restored.
    EXPECT_EQ(u"W", firstname.Value().Utf16());
    EXPECT_TRUE(firstname.SuggestedValue().IsEmpty());
    EXPECT_TRUE(firstname.IsAutofilled());

    // Verify the previewed fields are cleared.
    for (size_t i = 1; i < elements.size(); ++i) {
      WebFormControlElement& element = elements[i].first;
      SCOPED_TRACE(testing::Message() << "Element " << i);
      EXPECT_TRUE(element.Value().IsEmpty());
      EXPECT_TRUE(element.SuggestedValue().IsEmpty());
      EXPECT_FALSE(element.IsAutofilled());
    }
  }

  static WebString GetValueWrapper(WebFormControlElement element) {
    if (element.FormControlType() == blink::mojom::FormControlType::kTextArea) {
      return element.To<WebFormControlElement>().Value();
    }

    if (element.FormControlType() ==
        blink::mojom::FormControlType::kSelectOne) {
      return element.To<WebSelectElement>().Value();
    }

    return element.To<WebInputElement>().Value();
  }

  static WebString GetSuggestedValueWrapper(WebFormControlElement element) {
    if (element.FormControlType() == blink::mojom::FormControlType::kTextArea) {
      return element.To<WebFormControlElement>().SuggestedValue();
    }

    if (element.FormControlType() ==
        blink::mojom::FormControlType::kSelectOne) {
      return element.To<WebSelectElement>().SuggestedValue();
    }

    return element.To<WebInputElement>().SuggestedValue();
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
                                             /*extract_options=*/{}, &result);

  FormFieldData expected;
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  expected.set_id_attribute(u"element");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"value");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a text field with autocomplete="off".
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutocompleteOff) {
  LoadHTML(R"(<input id=element value=value autocomplete=off>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             {}, &result);

  FormFieldData expected;
  expected.set_id_attribute(u"element");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"value");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_autocomplete_attribute("off");
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a text field with maxlength specified.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldMaxLength) {
  LoadHTML(R"(<input id=element value=value maxlength=5>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             {}, &result);

  FormFieldData expected;
  expected.set_id_attribute(u"element");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"value");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(5);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
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
                                             {}, &result);

  FormFieldData expected;
  expected.set_id_attribute(u"element");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"value");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  expected.set_is_autofilled(true);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a radio or a checkbox field that has been
// autofilled.
TEST_F(FormAutofillTest, WebFormControlElementToClickableFormField) {
  LoadHTML(R"(<input type=checkbox id=checkbox value=mail checked>
              <input type=radio id=radio value=male>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebInputElement element = GetInputElementById("checkbox");
  element.SetAutofillState(WebAutofillState::kAutofilled);
  FormFieldData result;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             {}, &result);

  FormFieldData expected;
  expected.set_id_attribute(u"checkbox");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"mail");
  expected.set_form_control_type(FormControlType::kInputCheckbox);
  expected.set_max_length(0);
  expected.set_is_autofilled(true);
  expected.set_check_status(FormFieldData::CheckStatus::kChecked);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);

  element = GetInputElementById("radio");
  element.SetAutofillState(WebAutofillState::kAutofilled);
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             {}, &result);
  expected.set_id_attribute(u"radio");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"male");
  expected.set_form_control_type(FormControlType::kInputRadio);
  expected.set_max_length(0);
  expected.set_is_autofilled(true);
  expected.set_check_status(FormFieldData::CheckStatus::kCheckableButUnchecked);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
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
                                             {}, &result);

  FormFieldData expected;
  expected.set_id_attribute(u"element");
  expected.set_name(expected.id_attribute());
  expected.set_max_length(0);
  expected.set_form_control_type(FormControlType::kSelectOne);

  expected.set_value(u"CA");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
  EXPECT_THAT(result.selected_option().CopyAsOptional(),
              Optional(Field(&SelectOption::text, u"California")));
  ASSERT_EQ(2U, result.options().size());
  EXPECT_EQ(u"CA", result.options()[0].value);
  EXPECT_EQ(u"California", result.options()[0].text);
  EXPECT_EQ(u"TX", result.options()[1].value);
  EXPECT_EQ(u"Texas", result.options()[1].text);
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

  FormFieldData result1;
  WebFormControlElementToFormFieldForTesting(WebFormElement(), element, nullptr,
                                             {}, &result1);

  FormFieldData expected;
  expected.set_id_attribute(u"element");
  expected.set_name(expected.id_attribute());
  expected.set_max_length(0);
  expected.set_form_control_type(FormControlType::kSelectOne);
  // We check that the extra attributes have been copied to `result1`.
  expected.set_is_autofilled(true);
  expected.set_autocomplete_attribute("off");
  expected.set_should_autocomplete(false);
  expected.set_is_focusable(true);
  expected.set_is_visible(true);
  expected.set_text_direction(base::i18n::LEFT_TO_RIGHT);

  expected.set_value(u"CA");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result1);
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
                                             {}, &result);

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
                                             {}, &result);
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
                                             {}, &result);
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
                                             {}, &result);

  FormFieldData expected;
  expected.set_id_attribute(u"element");
  expected.set_name(expected.id_attribute());
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  expected.set_form_control_type(FormControlType::kTextArea);
  expected.set_value(
      u"This element's value\n"
      u"spans multiple lines.");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
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
                                             {}, &result);

  FormFieldData expected;
  expected.set_id_attribute(u"element");
  expected.set_name(expected.id_attribute());
  expected.set_max_length(0);
  expected.set_form_control_type(FormControlType::kInputMonth);
  expected.set_value(u"2011-12");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
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
                                             {}, &result);

  FormFieldData expected;
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  expected.set_id_attribute(u"password");
  expected.set_name(expected.id_attribute());
  expected.set_form_control_type(FormControlType::kInputPassword);
  expected.set_value(u"secret");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
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
                                               nullptr,
                                               /*extract_options=*/{}, &result);

    FormFieldData expected;
    expected.set_id_attribute(ASCIIToUTF16(test_case.element_id));
    expected.set_name(expected.id_attribute());
    expected.set_form_control_type(test_case.form_control_type);
    expected.set_max_length(
        (test_case.form_control_type == FormControlType::kInputText ||
         test_case.form_control_type == FormControlType::kTextArea)
            ? FormFieldData::kDefaultMaxLength
            : 0);
    expected.set_autocomplete_attribute(test_case.autocomplete_attribute);
    expected.set_parsed_autocomplete(
        ParseAutocompleteAttribute(test_case.autocomplete_attribute));
    expected.set_value(ASCIIToUTF16(test_case.value));

    SCOPED_TRACE(test_case.element_id);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
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
                                             {}, &result);
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
                                             {}, &result);
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
                                             {}, &result);
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
                                             {}, &result);
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
                                             {}, &result);
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
                                             {}, &result);
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
                                             {}, &result);
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
                                             {}, &result);
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
                                             {}, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormAutofillTest, WebFormElementToFormData) {
  LoadHTML(
      R"(<form name=TestForm action='http://cnn.com/submit/?a=1'>
           <label for=firstname>First name:</label>
             <input id=firstname value=John>
           <label for=lastname>Last name:</label>
             <input id=lastname value=Smith>
           <label for=street-address>Address:</label>
             <textarea id=street-address>123 Fantasy Ln.&#10;Apt. 42</textarea>
           <label for=state>State:</label>
             <select id=state>
               <option value=CA>California</option>
               <option value=TX>Texas</option>
             </select>
           <label for=password>Password:</label>
             <input type=password id=password value=secret>
           <label for=month>Card expiration:</label>
             <input type=month id=month value='2011-12'>
             <input type=submit name='reply-send' value=Send>
           <!-- The below inputs should be ignored -->
           <label for=notvisible>Hidden:</label>
             <input type=hidden id=notvisible value=apple>
         </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebVector<WebFormElement> forms = frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());

  WebInputElement input_element = GetInputElementById("firstname");

  FormData form = FindForm(input_element);

  EXPECT_EQ(u"TestForm", form.name());
  EXPECT_EQ(GetFormRendererId(forms[0]), form.renderer_id());
  EXPECT_EQ(GURL("http://cnn.com/submit/"), form.action());

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(6U, fields.size());

  FormFieldData expected;
  expected.set_id_attribute(u"firstname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"John");
  expected.set_label(u"First name:");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.set_id_attribute(u"lastname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Smith");
  expected.set_label(u"Last name:");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.set_id_attribute(u"street-address");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"123 Fantasy Ln.\nApt. 42");
  expected.set_label(u"Address:");
  expected.set_form_control_type(FormControlType::kTextArea);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.set_id_attribute(u"state");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"CA");
  expected.set_label(u"State:");
  expected.set_form_control_type(FormControlType::kSelectOne);
  expected.set_max_length(0);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  expected.set_id_attribute(u"password");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"secret");
  expected.set_label(u"Password:");
  expected.set_form_control_type(FormControlType::kInputPassword);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[4]);

  expected.set_id_attribute(u"month");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"2011-12");
  expected.set_label(u"Card expiration:");
  expected.set_form_control_type(FormControlType::kInputMonth);
  expected.set_max_length(0);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[5]);

  // Check renderer_id.
  WebVector<WebFormControlElement> form_control_elements =
      forms[0].GetFormControlElements();
  for (size_t i = 0; i < fields.size(); ++i)
    EXPECT_EQ(GetFieldRendererId(form_control_elements[i]),
              fields[i].renderer_id());
}

TEST_F(FormAutofillTest, WebFormElementConsiderNonControlLabelableElements) {
  LoadHTML(R"(<form id=form>
                <label for=progress>Progress:</label>
                <progress id=progress></progress>
                <label for=firstname>First name:</label>
                <input id=firstname value=John>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  FormData form = *ExtractFormData(web_form);

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(1U, fields.size());
  EXPECT_EQ(u"firstname", fields[0].name());
}

// We should not be able to serialize a form with too many fillable fields.
TEST_F(FormAutofillTest, WebFormElementToFormData_TooManyFields) {
  std::string html = "<form name=TestForm action='http://cnn.com'>";
  for (size_t i = 0; i < (kMaxExtractableFields + 1); ++i) {
    html += "<input>";
  }
  html += "</form>";
  LoadHTML(html.c_str());

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebVector<WebFormElement> forms = frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());
  ASSERT_FALSE(forms.front().GetFormControlElements().empty());

  WebInputElement input_element = forms.front()
                                      .GetFormControlElements()
                                      .front()
                                      .DynamicTo<WebInputElement>();
  EXPECT_THAT(
      FindFormAndFieldForFormControlElement(input_element),
      Optional(Pair(
          Property(&FormData::fields,
                   ElementsAre(Property(&FormFieldData::renderer_id,
                                        GetFieldRendererId(input_element)))),
          _)));
}

// Tests that the `should_autocomplete` is set to false for all the fields when
// an autocomplete='off' attribute is set for the form in HTML.
TEST_F(FormAutofillTest, WebFormElementToFormData_AutocompleteOff_OnForm) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com' autocomplete=off>
           <label for=firstname>First name:</label>
             <input id=firstname value=John>
           <label for=lastname>Last name:</label>
             <input id=lastname value=Smith>
           <label for='street-address'>Address:</label>
             <input id=addressline1 value='123 Test st.'>
         </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  FormData form = *ExtractFormData(web_form);
  for (const FormFieldData& field : form.fields()) {
    EXPECT_FALSE(field.should_autocomplete());
  }
}

// Tests that the `should_autocomplete` is set to false only for the field
// which has an autocomplete='off' attribute set for it in HTML.
TEST_F(FormAutofillTest, WebFormElementToFormData_AutocompleteOff_OnField) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com'>
           <label for=firstname>First name:</label>
             <input id=firstname value=John autocomplete=off>
           <label for=lastname>Last name:</label>
             <input id=lastname value=Smith>
           <label for='street-address'>Address:</label>
             <input id=addressline1 value='123 Test st.'>
         </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  FormData form = *ExtractFormData(web_form);
  ASSERT_EQ(3U, form.fields().size());
  EXPECT_FALSE(form.fields()[0].should_autocomplete());
  EXPECT_TRUE(form.fields()[1].should_autocomplete());
  EXPECT_TRUE(form.fields()[2].should_autocomplete());
}

// `should_autocomplete` must be set to false for the field with
// autocomplete='one-time-code' attribute set in HTML.
TEST_F(FormAutofillTest, WebFormElementToFormData_AutocompleteOff_OneTimeCode) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com'>
           <input value=123 autocomplete='one-time-code'>
         </form>)");
  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  FormData form = *ExtractFormData(web_form);
  ASSERT_EQ(1U, form.fields().size());
  EXPECT_FALSE(form.fields()[0].should_autocomplete());
}

// Tests CSS classes are set.
TEST_F(FormAutofillTest, WebFormElementToFormData_CssClasses) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com' autocomplete=off>
           <input id=firstname class='firstname_field'>
           <input id=lastname class='lastname_field'>
           <input id=addressline1>
         </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  FormData form = *ExtractFormData(web_form);
  ASSERT_EQ(3U, form.fields().size());
  EXPECT_EQ(u"firstname_field", form.fields()[0].css_classes());
  EXPECT_EQ(u"lastname_field", form.fields()[1].css_classes());
  EXPECT_EQ(std::u16string(), form.fields()[2].css_classes());
}

// Tests id attributes are set.
TEST_F(FormAutofillTest, WebFormElementToFormData_IdAttributes) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com' autocomplete=off>
           <input name=name1 id=firstname>
           <input name=name2 id=lastname>
           <input name=same id=same>
           <input id=addressline1>
         </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  FormData form = *ExtractFormData(web_form);
  EXPECT_EQ(4U, form.fields().size());

  // id attributes.
  EXPECT_EQ(u"firstname", form.fields()[0].id_attribute());
  EXPECT_EQ(u"lastname", form.fields()[1].id_attribute());
  EXPECT_EQ(u"same", form.fields()[2].id_attribute());
  EXPECT_EQ(u"addressline1", form.fields()[3].id_attribute());

  // name attributes.
  EXPECT_EQ(u"name1", form.fields()[0].name_attribute());
  EXPECT_EQ(u"name2", form.fields()[1].name_attribute());
  EXPECT_EQ(u"same", form.fields()[2].name_attribute());
  EXPECT_EQ(u"", form.fields()[3].name_attribute());

  // name for autofill
  EXPECT_EQ(u"name1", form.fields()[0].name());
  EXPECT_EQ(u"name2", form.fields()[1].name());
  EXPECT_EQ(u"same", form.fields()[2].name());
  EXPECT_EQ(u"addressline1", form.fields()[3].name());
}

TEST_F(FormAutofillTest, ExtractForms) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
           First name: <input id=firstname value=John>
           Last name: <input id=lastname value=Smith>
           Email: <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, ExtractMultipleForms) {
  LoadHTML(R"(<form name=TestForm action='http://cnn.com'>
                <input id=firstname value=John>
                <input id=lastname value=Smith>
                <input id=email value='john@example.com'>
                <input type=submit name='reply-send' value=Send>
              </form>
              <form name=TestForm2 action='http://zoo.com'>
                <input id=firstname value=Jack>
                <input id=lastname value=Adams>
                <input id=email value='jack@example.com'>
                <input type=submit name='reply-send' value=Send>
              </form>)");

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(2U, forms.size());

  // First form.
  const FormData& form = forms[0];
  EXPECT_EQ(u"TestForm", form.name());
  EXPECT_EQ(GURL("http://cnn.com"), form.action());

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);

  expected.set_id_attribute(u"firstname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"John");
  expected.set_label(u"John");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.set_id_attribute(u"lastname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Smith");
  expected.set_label(u"Smith");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.set_id_attribute(u"email");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"john@example.com");
  expected.set_label(u"john@example.com");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  // Second form.
  const FormData& form2 = forms[1];
  EXPECT_EQ(u"TestForm2", form2.name());
  EXPECT_EQ(GURL("http://zoo.com"), form2.action());

  const std::vector<FormFieldData>& fields2 = form2.fields();
  ASSERT_EQ(3U, fields2.size());

  expected.set_id_attribute(u"firstname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Jack");
  expected.set_label(u"Jack");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.set_id_attribute(u"lastname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Adams");
  expected.set_label(u"Adams");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.set_id_attribute(u"email");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"jack@example.com");
  expected.set_label(u"jack@example.com");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
}

TEST_F(FormAutofillTest, OnlyExtractNewForms) {
  LoadHTML(
      R"(<form id=testform action='http://cnn.com'>
           <input id=firstname value=John>
           <input id=lastname value=Smith>
           <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Second call should give nothing as there are no new forms.
  forms = UpdateFormCache().updated_forms;
  ASSERT_TRUE(forms.empty());

  // Append to the current form will re-extract.
  ExecuteJavaScriptForTests(
      R"(var newInput = document.createElement('input');
         newInput.setAttribute('type', 'text');
         newInput.setAttribute('id', 'telephone');
         newInput.value = '12345';
         document.getElementById('testform').appendChild(newInput);)");
  base::RunLoop().RunUntilIdle();

  forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  const std::vector<FormFieldData>& fields = forms[0].fields();
  ASSERT_EQ(4U, fields.size());

  FormFieldData expected;
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);

  expected.set_id_attribute(u"firstname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"John");
  expected.set_label(u"John");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.set_id_attribute(u"lastname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Smith");
  expected.set_label(u"Smith");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.set_id_attribute(u"email");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"john@example.com");
  expected.set_label(u"john@example.com");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.set_id_attribute(u"telephone");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"12345");
  expected.set_label({});
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  forms.clear();

  // Completely new form will also be extracted.
  ExecuteJavaScriptForTests(
      R"(var newForm=document.createElement('form');
        newForm.id='new_testform';
        newForm.action='http://google.com';
        newForm.method='post';
        var newFirstname=document.createElement('input');
        newFirstname.setAttribute('type', 'text');
        newFirstname.setAttribute('id', 'second_firstname');
        newFirstname.value = 'Bob';
        var newLastname=document.createElement('input');
        newLastname.setAttribute('type', 'text');
        newLastname.setAttribute('id', 'second_lastname');
        newLastname.value = 'Hope';
        var newEmail=document.createElement('input');
        newEmail.setAttribute('type', 'text');
        newEmail.setAttribute('id', 'second_email');
        newEmail.value = 'bobhope@example.com';
        newForm.appendChild(newFirstname);
        newForm.appendChild(newLastname);
        newForm.appendChild(newEmail);
        document.body.appendChild(newForm);)");
  base::RunLoop().RunUntilIdle();

  forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  const std::vector<FormFieldData>& fields2 = forms[0].fields();
  ASSERT_EQ(3U, fields2.size());

  expected.set_id_attribute(u"second_firstname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Bob");
  expected.set_label({});
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.set_id_attribute(u"second_lastname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Hope");
  expected.set_label({});
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.set_id_attribute(u"second_email");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"bobhope@example.com");
  expected.set_label({});
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
}

// We should not report additional forms for empty forms.
TEST_F(FormAutofillTest, ExtractFormsNoFields) {
  LoadHTML(R"(<form name=TestForm action='http://cnn.com'>
              </form>)");

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_TRUE(forms.empty());
}

TEST_F(FormAutofillTest, WebFormElementToFormData_Autocomplete) {
  {
    // Form is still Autofill-able despite autocomplete=off.
    LoadHTML(
        R"(<form name=TestForm action='http://cnn.com' autocomplete=off>
             <input id=firstname value=John>
             <input id=lastname value=Smith>
             <input id=email value='john@example.com'>
             <input type=submit name='reply-send' value=Send>
           </form>)");

    WebVector<WebFormElement> web_forms = GetDocument().GetTopLevelForms();
    ASSERT_EQ(1U, web_forms.size());
    WebFormElement web_form = web_forms[0];

    EXPECT_TRUE(ExtractFormData(web_form));
  }
}

TEST_F(FormAutofillTest, FindFormForInputElement) {
  TestFindFormForInputElement(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname value=John>
           <input id=lastname value=Smith>
           <input id=email value='john@example.com' autocomplete=off>
           <input id=phone value='1.800.555.1234'>
           <input type=submit name='reply-send' value=Send>
         </form>)",
      false);
}

TEST_F(FormAutofillTest, FindFormForInputElementForUnownedForm) {
  TestFindFormForInputElement(
      R"(<head><title>delivery recipient</title></head>
         <input id=firstname value=John>
         <input id=lastname value=Smith>
         <input id=email value='john@example.com' autocomplete=off>
         <input id=phone value='1.800.555.1234'>
         <input type=submit name='reply-send' value=Send>)",
      true);
}

TEST_F(FormAutofillTest, FindFormForTextAreaElement) {
  TestFindFormForTextAreaElement(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname value=John>
           <input id=lastname value=Smith>
           <input id=email value='john@example.com' autocomplete=off>
           <textarea id='street-address'>123 Fantasy Ln.&#10;Apt. 42</textarea>
           <input type=submit name='reply-send' value=Send>
         </form>)",
      false);
}

TEST_F(FormAutofillTest, FindFormForTextAreaElementForUnownedForm) {
  TestFindFormForTextAreaElement(
      R"(<head><title>delivery address</title></head>
         <input id=firstname value=John>
         <input id=lastname value=Smith>
         <input id=email value='john@example.com' autocomplete=off>
         <textarea id='street-address'>123 Fantasy Ln.&#10;Apt. 42</textarea>
         <input type=submit name='reply-send' value=Send>)",
      true);
}

// Test regular FillForm function.
TEST_F(FormAutofillTest, FillForm) {
  TestFillForm(kFormHtml, false, nullptr);
}

TEST_F(FormAutofillTest, FillFormForUnownedForm) {
  TestFillForm(kUnownedFormHtml, true, nullptr);
}

TEST_F(FormAutofillTest, FillFormForUnownedUntitledForm) {
  TestFillForm(kUnownedUntitledFormHtml, true,
               "http://example.test/checkout_flow");
}

TEST_F(FormAutofillTest, FillFormForUnownedNonEnglishForm) {
  TestFillForm(kUnownedNonEnglishFormHtml, true, nullptr);
}

TEST_F(FormAutofillTest, FillFormForUnownedNonASCIIForm) {
  std::string html = R"(<head><title>accented latin: \xC3\xA0, )"
                     R"(thai: \xE0\xB8\x81, control: \x04, )"
                     R"(nbsp: \xEF\xBB\xBF, non-BMP: \xF0\x9F\x8C\x80; )"
                     R"(This should match a CHECKOUT flow )"
                     R"(despite the non-ASCII chars</title></head>)";
  html.append(kUnownedUntitledFormHtml);
  TestFillForm(html.c_str(), true, nullptr);
}

TEST_F(FormAutofillTest, PreviewFormX) {
  TestPreviewForm(kFormHtml, false, nullptr);
}

TEST_F(FormAutofillTest, PreviewFormForUnownedForm) {
  TestPreviewForm(kUnownedFormHtml, true, nullptr);
}

TEST_F(FormAutofillTest, PreviewFormForUnownedUntitledForm) {
  TestPreviewForm(kUnownedUntitledFormHtml, true,
                  "http://example.test/Enter_Shipping_Address/");
}

TEST_F(FormAutofillTest, PreviewFormForUnownedNonEnglishForm) {
  TestPreviewForm(kUnownedNonEnglishFormHtml, true, nullptr);
}


TEST_F(FormAutofillTest, Labels) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
           <label for=firstname> First name: </label>
             <input id=firstname value=John>
           <label for=lastname> Last name: </label>
             <input id=lastname value=Smith>
           <label for=email> Email: </label>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
}

// <label for=fieldId> elements are correctly assigned to their inputs. Multiple
// labels are separated with a space.
// TODO(crbug.com/40229922): Simplify the test using `ExpectLabels()`. This
// requires some refactoring of the fixture, as only owned forms are supported
// at the moment.
TEST_F(FormAutofillTest, LabelForAttribute) {
  LoadHTML(R"(<label for=fieldId>foo</label>
              <label for=fieldId>bar</label>
              <input id=fieldId>)");
  ASSERT_NE(GetMainFrame(), nullptr);

  base::HistogramTester histogram_tester;
  // Simulate seeing an unowned form containing just the input "fieldID".
  FormData form = *ExtractFormData(WebFormElement());
  ASSERT_EQ(form.fields().size(), 1u);
  FormFieldData& form_field_data = test_api(form).field(0);

  EXPECT_EQ(form_field_data.label(), u"foo bar");
  EXPECT_EQ(form_field_data.label_source(), FormFieldData::LabelSource::kForId);
}

// Tests that when a label is assigned to an input, text behind it is considered
// as a fallback.
// The label is assigned to the input without the for-attribute, by declaring it
// it inside the label.
TEST_F(FormAutofillTest, LabelTextBehindInput) {
  ExpectLabels(R"(
    <form name=TestForm action=http://cnn.com>
      <label>
        <input>
        label
      </label>
    </form>
  )",
               /*id_attributes=*/{u""}, /*name_attributes=*/{u""},
               /*labels=*/{u"label"}, /*names=*/{u""}, /*values=*/{u""});
}

TEST_F(FormAutofillTest, LabelsWithSpans) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
           <label for=firstname><span>First name: </span></label>
             <input id=firstname value=John>
           <label for=lastname><span>Last name: </span></label>
             <input id=lastname value=Smith>
           <label for=email><span>Email: </span></label>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
}

// This test is different from FormAutofillTest.Labels in that the label
// elements for= attribute is set to the name of the form control element it is
// a label for instead of the id of the form control element.  This is invalid
// because the for= attribute must be set to the id of the form control element;
// however, current label parsing code will extract the text from the previous
// label element and apply it to the following input field.
TEST_F(FormAutofillTest, InvalidLabels) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"");
  name_attributes.push_back(u"firstname");
  labels.push_back(u"First name:");
  names.push_back(name_attributes.back());
  values.push_back(u"John");

  id_attributes.push_back(u"");
  name_attributes.push_back(u"lastname");
  labels.push_back(u"Last name:");
  names.push_back(name_attributes.back());
  values.push_back(u"Smith");

  id_attributes.push_back(u"");
  name_attributes.push_back(u"email");
  labels.push_back(u"Email:");
  names.push_back(name_attributes.back());
  values.push_back(u"john@example.com");

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
           <label for=firstname> First name: </label>
             <input name=firstname value=John>
           <label for=lastname> Last name: </label>
             <input name=lastname value=Smith>
           <label for=email> Email: </label>
             <input name=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

// This test has three form control elements, only one of which has a label
// element associated with it.
TEST_F(FormAutofillTest, OneLabelElement) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
           First name:
             <input id=firstname value=John>
           <label for=lastname>Last name: </label>
             <input id=lastname value=Smith>
           Email:
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromText) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
           First name:
             <input id=firstname value=John>
           Last name:
             <input id=lastname value=Smith>
           Email:
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromParagraph) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
           <p>First name:</p><input id=firstname value=John>
           <p>Last name:</p>
             <input id=lastname value=Smith>
           <p>Email:</p>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromBold) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
           <b>First name:</b><input id=firstname value=John>
           <b>Last name:</b>
             <input id=lastname value=Smith>
           <b>Email:</b>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredPriorToImgOrBr) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
           First name:<img><input id=firstname value=John>
           Last name:<img>
             <input id=lastname value=Smith>
           Email:<br>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableCell) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
           <table>
             <tr>
               <td>First name:</td>
               <td><input id=firstname value=John></td>
             </tr>
             <tr>
               <td>Last name:</td>
               <td><input id=lastname value=Smith></td>
             </tr>
             <tr>
               <td>Email:</td>
               <td><input id=email value='john@example.com'></td>
             </tr>
             <tr>
               <td></td>
               <td><input type=submit name='reply-send' value=Send></td>
             </tr>
           </table>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableCellTH) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
         <table>
           <tr>
             <th>First name:</th>
             <td><input id=firstname value=John></td>
           </tr>
           <tr>
             <th>Last name:</th>
             <td><input id=lastname value=Smith></td>
           </tr>
           <tr>
             <th>Email:</th>
             <td><input id=email value='john@example.com'></td>
           </tr>
           <tr>
             <td></td>
             <td><input type=submit name='reply-send' value=Send></td>
           </tr>
         </table>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableCellNested) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"firstname");
  name_attributes.push_back(u"");
  labels.push_back(u"First name: Bogus");
  names.push_back(id_attributes.back());
  values.push_back(u"John");

  id_attributes.push_back(u"lastname");
  name_attributes.push_back(u"");
  labels.push_back(u"Last name:");
  names.push_back(id_attributes.back());
  values.push_back(u"Smith");

  id_attributes.push_back(u"email");
  name_attributes.push_back(u"");
  labels.push_back(u"Email:");
  names.push_back(id_attributes.back());
  values.push_back(u"john@example.com");

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
         <table>
           <tr>
             <td>
               <font>
                 First name:
               </font>
               <font>
                 Bogus
               </font>
             </td>
             <td>
               <font>
                 <input id=firstname value=John>
               </font>
             </td>
           </tr>
           <tr>
             <td>
               <font>
                 Last name:
               </font>
             </td>
             <td>
               <font>
                 <input id=lastname value=Smith>
               </font>
             </td>
           </tr>
           <tr>
             <td>
               <font>
                 Email:
               </font>
             </td>
             <td>
               <font>
                 <input id=email value='john@example.com'>
               </font>
             </td>
           </tr>
           <tr>
             <td></td>
             <td>
               <input type=submit name='reply-send' value=Send>
             </td>
           </tr>
         </table>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredFromTableEmptyTDs) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"firstname");
  name_attributes.push_back(u"");
  labels.push_back(u"* First Name");
  names.push_back(id_attributes.back());
  values.push_back(u"John");

  id_attributes.push_back(u"lastname");
  name_attributes.push_back(u"");
  labels.push_back(u"* Last Name");
  names.push_back(id_attributes.back());
  values.push_back(u"Smith");

  id_attributes.push_back(u"email");
  name_attributes.push_back(u"");
  labels.push_back(u"* Email");
  names.push_back(id_attributes.back());
  values.push_back(u"john@example.com");

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
         <table>
           <tr>
             <td>
               <span>*</span>
               <b>First Name</b>
             </td>
             <td></td>
             <td>
               <input id=firstname value=John>
             </td>
           </tr>
           <tr>
             <td>
               <span>*</span>
               <b>Last Name</b>
             </td>
             <td></td>
             <td>
               <input id=lastname value=Smith>
             </td>
           </tr>
           <tr>
             <td>
               <span>*</span>
               <b>Email</b>
             </td>
             <td></td>
             <td>
               <input id=email value='john@example.com'>
             </td>
           </tr>
           <tr>
             <td></td>
             <td>
               <input type=submit name='reply-send' value=Send>
             </td>
           </tr>
         </table>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredFromPreviousTD) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"firstname");
  name_attributes.push_back(u"");
  labels.push_back(u"* First Name");
  names.push_back(id_attributes.back());
  values.push_back(u"John");

  id_attributes.push_back(u"lastname");
  name_attributes.push_back(u"");
  labels.push_back(u"* Last Name");
  names.push_back(id_attributes.back());
  values.push_back(u"Smith");

  id_attributes.push_back(u"email");
  name_attributes.push_back(u"");
  labels.push_back(u"* Email");
  names.push_back(id_attributes.back());
  values.push_back(u"john@example.com");

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
         <table>
           <tr>
             <td>* First Name</td>
             <td>
               Bogus
               <input type=hidden>
               <input id=firstname value=John>
             </td>
           </tr>
           <tr>
             <td>* Last Name</td>
             <td>
               <input id=lastname value=Smith>
             </td>
           </tr>
           <tr>
             <td>* Email</td>
             <td>
               <input id=email value='john@example.com'>
             </td>
           </tr>
           <tr>
             <td></td>
             <td>
               <input type=submit name='reply-send' value=Send>
             </td>
           </tr>
         </table>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

// <script>, <noscript> and <option> tags are excluded when the labels are
// inferred.
// Also <!-- comment --> is excluded.
TEST_F(FormAutofillTest, LabelsInferredFromTableWithSpecialElements) {
  FormFieldData expected;
  std::vector<FormFieldData> fields;

  expected.set_id_attribute(u"firstname");
  expected.set_name_attribute(u"");
  expected.set_label(u"* First Name");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"John");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  fields.push_back(expected);

  expected.set_id_attribute(u"middlename");
  expected.set_name_attribute(u"");
  expected.set_label(u"* Middle Name");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Joe");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  fields.push_back(expected);

  expected.set_id_attribute(u"lastname");
  expected.set_name_attribute(u"");
  expected.set_label(u"* Last Name");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Smith");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  fields.push_back(expected);

  expected.set_id_attribute(u"country");
  expected.set_name_attribute(u"");
  expected.set_label(u"* Country");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"US");
  expected.set_form_control_type(FormControlType::kSelectOne);
  expected.set_max_length(0);
  fields.push_back(expected);

  expected.set_id_attribute(u"email");
  expected.set_name_attribute(u"");
  expected.set_label(u"* Email");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"john@example.com");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  fields.push_back(expected);

  ExpectLabelsAndTypes(
      R"(<form name=TestForm action='http://cnn.com'>
         <table>
           <tr>
             <td>
               <span>*</span>
               <b>First Name</b>
             </td>
             <td>
               <script> <!-- function test() { alert('ignored as label'); } -->
               </script>
               <input id=firstname value=John>
             </td>
           </tr>
           <tr>
             <td>
               <span>*</span>
               <b>Middle Name</b>
             </td>
             <td>
               <noscript>
                 <p>Bad</p>
               </noscript>
               <input id=middlename value=Joe>
             </td>
           </tr>
           <tr>
             <td>
               <span>*</span>
               <b>Last Name</b>
             </td>
             <td>
               <input id=lastname value=Smith>
             </td>
           </tr>
           <tr>
             <td>
               <span>*</span>
               <b>Country</b>
             </td>
             <td>
               <select id=country>
                 <option value=US>The value should be ignored as label.
                 </option>
                 <option value=JP>JAPAN</option>
               </select>
             </td>
           </tr>
           <tr>
             <td>
               <span>*</span>
               <b>Email</b>
             </td>
             <td>
               <!-- This comment should be ignored as inferred label.-->
               <input id=email value='john@example.com'>
             </td>
           </tr>
           <tr>
             <td></td>
             <td>
               <input type=submit name='reply-send' value=Send>
             </td>
           </tr>
         </table>
         </form>)",
      fields);
}

TEST_F(FormAutofillTest, LabelsInferredFromTableLabels) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
         <table>
           <tr>
             <td>
               <label>First name:</label>
               <input id=firstname value=John>
             </td>
           </tr>
           <tr>
             <td>
               <label>Last name:</label>
               <input id=lastname value=Smith>
             </td>
           </tr>
           <tr>
             <td>
               <label>Email:</label>
               <input id=email value='john@example.com'>
             </td>
           </tr>
         </table>
         <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableTDInterveningElements) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
         <table>
           <tr>
             <td>
               First name:
               <br>
               <input id=firstname value=John>
             </td>
           </tr>
           <tr>
             <td>
               Last name:
               <br>
               <input id=lastname value=Smith>
             </td>
           </tr>
           <tr>
             <td>
               Email:
               <br>
               <input id=email value='john@example.com'>
             </td>
           </tr>
         </table>
         <input type=submit name='reply-send' value=Send>
         </form>)");
}

// Verify that we correctly infer labels when the label text spans multiple
// adjacent HTML elements, not separated by whitespace.
TEST_F(FormAutofillTest, LabelsInferredFromTableAdjacentElements) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"firstname");
  name_attributes.push_back(u"");
  labels.push_back(u"*First Name");
  names.push_back(id_attributes.back());
  values.push_back(u"John");

  id_attributes.push_back(u"lastname");
  name_attributes.push_back(u"");
  labels.push_back(u"*Last Name");
  names.push_back(id_attributes.back());
  values.push_back(u"Smith");

  id_attributes.push_back(u"email");
  name_attributes.push_back(u"");
  labels.push_back(u"*Email");
  names.push_back(id_attributes.back());
  values.push_back(u"john@example.com");

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
         <table>
           <tr>
             <td>
               <span>*</span><b>First Name</b>
             </td>
             <td>
               <input id=firstname value=John>
             </td>
           </tr>
           <tr>
             <td>
               <span>*</span><b>Last Name</b>
             </td>
             <td>
               <input id=lastname value=Smith>
             </td>
           </tr>
           <tr>
             <td>
               <span>*</span><b>Email</b>
             </td>
             <td>
               <input id=email value='john@example.com'>
             </td>
           </tr>
           <tr>
             <td>
               <input type=submit name='reply-send' value=Send>
             </td>
           </tr>
         </table>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

// Verify that we correctly infer labels when the label text resides in the
// previous row.
TEST_F(FormAutofillTest, LabelsInferredFromTableRow) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"firstname");
  name_attributes.push_back(u"");
  labels.push_back(u"*First Name");
  names.push_back(id_attributes.back());
  values.push_back(u"John");

  id_attributes.push_back(u"lastname");
  name_attributes.push_back(u"");
  labels.push_back(u"*Last Name");
  names.push_back(id_attributes.back());
  values.push_back(u"Smith");

  id_attributes.push_back(u"email");
  name_attributes.push_back(u"");
  labels.push_back(u"*Email");
  names.push_back(id_attributes.back());
  values.push_back(u"john@example.com");

  id_attributes.push_back(u"name2");
  name_attributes.push_back(u"");
  labels.push_back(u"NAME");
  names.push_back(id_attributes.back());
  values.push_back(u"John Smith");

  id_attributes.push_back(u"email2");
  name_attributes.push_back(u"");
  labels.push_back(u"EMAIL");
  names.push_back(id_attributes.back());
  values.push_back(u"john@example2.com");

  id_attributes.push_back(u"phone1");
  name_attributes.push_back(u"");
  labels.push_back(u"Phone");
  names.push_back(id_attributes.back());
  values.push_back(u"123");

  id_attributes.push_back(u"phone2");
  name_attributes.push_back(u"");
  labels.push_back(u"Phone");
  names.push_back(id_attributes.back());
  values.push_back(u"456");

  id_attributes.push_back(u"phone3");
  name_attributes.push_back(u"");
  labels.push_back(u"Phone");
  names.push_back(id_attributes.back());
  values.push_back(u"7890");

  // Note that ccnumber uses the name attribute instead of the id attribute.
  id_attributes.push_back(u"");
  name_attributes.push_back(u"ccnumber");
  labels.push_back(u"Credit Card Number");
  names.push_back(name_attributes.back());
  values.push_back(u"4444555544445555");

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
         <table>
           <tr>
             <td>*First Name</td>
             <td>*Last Name</td>
             <td>*Email</td>
           </tr>
           <tr>
             <td>
               <input id=firstname value=John>
             </td>
             <td>
               <input id=lastname value=Smith>
             </td>
             <td>
               <input id=email value='john@example.com'>
             </td>
           </tr>
           <tr>
             <td colspan=2>NAME</td>
             <td>EMAIL</td>
           </tr>
           <tr>
             <td colspan=2>
               <input id=name2 value='John Smith'>
             </td>
             <td>
               <input id=email2 value='john@example2.com'>
             </td>
           </tr>
           <tr>
             <td>Phone</td>
           </tr>
           <tr>
             <td>
               <input id=phone1 value=123>
             </td>
             <td>
               <input id=phone2 value=456>
             </td>
             <td>
               <input id=phone3 value=7890>
             </td>
           </tr>
           <tr>
             <th>
               Credit Card Number
             </th>
           </tr>
           <tr>
             <td>
               <input name=ccnumber value=4444555544445555>
             </td>
           </tr>
           <tr>
             <td>
               <input type=submit name='reply-send' value=Send>
             </td>
           </tr>
         </table>)",
      id_attributes, name_attributes, labels, names, values);
}

// Verify that we correctly infer labels when enclosed within a list item.
TEST_F(FormAutofillTest, LabelsInferredFromListItem) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"areacode");
  name_attributes.push_back(u"");
  labels.push_back(u"* Home Phone");
  names.push_back(id_attributes.back());
  values.push_back(u"415");

  id_attributes.push_back(u"prefix");
  name_attributes.push_back(u"");
  labels.push_back(u"* Home Phone");
  names.push_back(id_attributes.back());
  values.push_back(u"555");

  id_attributes.push_back(u"suffix");
  name_attributes.push_back(u"");
  labels.push_back(u"* Home Phone");
  names.push_back(id_attributes.back());
  values.push_back(u"1212");

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
         <div>
           <li>
             <span>Bogus</span>
           </li>
           <li>
             <label><em>*</em> Home Phone</label>
             <input id=areacode value=415>
             <input id=prefix value=555>
             <input id=suffix value=1212>
           </li>
           <li>
             <input type=submit name='reply-send' value=Send>
           </li>
         </div>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredFromDefinitionList) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"firstname");
  name_attributes.push_back(u"");
  labels.push_back(u"* First name: Bogus");
  names.push_back(id_attributes.back());
  values.push_back(u"John");

  id_attributes.push_back(u"lastname");
  name_attributes.push_back(u"");
  labels.push_back(u"Last name:");
  names.push_back(id_attributes.back());
  values.push_back(u"Smith");

  id_attributes.push_back(u"email");
  name_attributes.push_back(u"");
  labels.push_back(u"Email:");
  names.push_back(id_attributes.back());
  values.push_back(u"john@example.com");

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
         <dl>
           <dt>
             <span>
               *
             </span>
             <span>
               First name:
             </span>
             <span>
               Bogus
             </span>
           </dt>
           <dd>
             <font>
               <input id=firstname value=John>
             </font>
           </dd>
           <dt>
             <span>
               Last name:
             </span>
           </dt>
           <dd>
             <font>
               <input id=lastname value=Smith>
             </font>
           </dd>
           <dt>
             <span>
               Email:
             </span>
           </dt>
           <dd>
             <font>
               <input id=email value='john@example.com'>
             </font>
           </dd>
           <dt></dt>
           <dd>
             <input type=submit name='reply-send' value=Send>
           </dd>
         </dl>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredWithSameName) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"");
  name_attributes.push_back(u"Address");
  labels.push_back(u"Address Line 1:");
  names.push_back(name_attributes.back());
  values.emplace_back();

  id_attributes.push_back(u"");
  name_attributes.push_back(u"Address");
  labels.push_back(u"Address Line 2:");
  names.push_back(name_attributes.back());
  values.emplace_back();

  id_attributes.push_back(u"");
  name_attributes.push_back(u"Address");
  labels.push_back(u"Address Line 3:");
  names.push_back(name_attributes.back());
  values.emplace_back();

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
           Address Line 1:
             <input name=Address>
           Address Line 2:
             <input name=Address>
           Address Line 3:
             <input name=Address>
           <input type=submit name='reply-send' value=Send>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredWithImageTags) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"");
  name_attributes.push_back(u"dayphone1");
  labels.push_back(u"Phone:");
  names.push_back(name_attributes.back());
  values.emplace_back();

  id_attributes.push_back(u"");
  name_attributes.push_back(u"dayphone2");
  labels.push_back(
      base::FeatureList::IsEnabled(
          features::kAutofillConsiderPhoneNumberSeparatorsValidLabels)
          ? u"-"
          : u"");
  names.push_back(name_attributes.back());
  values.emplace_back();

  id_attributes.push_back(u"");
  name_attributes.push_back(u"dayphone3");
  labels.push_back(
      base::FeatureList::IsEnabled(
          features::kAutofillConsiderPhoneNumberSeparatorsValidLabels)
          ? u"-"
          : u"");
  names.push_back(name_attributes.back());
  values.emplace_back();

  id_attributes.push_back(u"");
  name_attributes.push_back(u"dayphone4");
  labels.push_back(u"ext.:");
  names.push_back(name_attributes.back());
  values.emplace_back();

  id_attributes.push_back(u"");
  name_attributes.push_back(u"dummy");
  labels.emplace_back();
  names.push_back(name_attributes.back());
  values.emplace_back();

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
           Phone:
           <input name=dayphone1>
           <img>
           -
           <img>
           <input name=dayphone2>
           <img>
           -
           <img>
           <input name=dayphone3>
           ext.:
           <input name=dayphone4>
           <input name=dummy>
           <input type=submit name='reply-send' value=Send>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredFromDivTable) {
  ExpectJohnSmithLabelsAndNameAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
         <div>First name:<br>
           <span>
             <input name=firstname value=John>
           </span>
         </div>
         <div>Last name:<br>
           <span>
             <input name=lastname value=Smith>
           </span>
         </div>
         <div>Email:<br>
           <span>
             <input name=email value='john@example.com'>
           </span>
         </div>
         <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromDivSiblingTable) {
  ExpectJohnSmithLabelsAndNameAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
         <div>First name:</div>
         <div>
           <span>
             <input name=firstname value=John>
           </span>
         </div>
         <div>Last name:</div>
         <div>
           <span>
             <input name=lastname value=Smith>
           </span>
         </div>
         <div>Email:</div>
         <div>
           <span>
             <input name=email value='john@example.com'>
           </span>
         </div>
         <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromLabelInDivTable) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
         <label>First name:</label>
         <label for=lastname>Last name:</label>
         <div>
           <input id=firstname value=John>
         </div>
         <div>
           <input id=lastname value=Smith>
         </div>
         <label>Email:</label>
         <div>
           <span>
             <input id=email value='john@example.com'>
           </span>
         </div>
         <input type=submit name='reply-send' value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, LabelsInferredFromDefinitionListRatherThanDivTable) {
  ExpectJohnSmithLabelsAndIdAttributes(
      R"(<form name=TestForm action='http://cnn.com'>
         <div>This is not a label.<br>
         <dl>
           <dt>
             <span>
               First name:
             </span>
           </dt>
           <dd>
             <font>
               <input id=firstname value=John>
             </font>
           </dd>
           <dt>
             <span>
               Last name:
             </span>
           </dt>
           <dd>
             <font>
               <input id=lastname value=Smith>
             </font>
           </dd>
           <dt>
             <span>
               Email:
             </span>
           </dt>
           <dd>
             <font>
               <input id=email value='john@example.com'>
             </font>
           </dd>
           <dt></dt>
           <dd>
             <input type=submit name='reply-send' value=Send>
           </dd>
         </dl>
         </div>
         </form>)");
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

  WebVector<WebFormElement> forms = frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());

  FormData form = *ExtractFormData(forms[0]);
  EXPECT_EQ(u"TestForm", form.name());
  EXPECT_EQ(GURL("http://cnn.com"), form.action());

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(4U, fields.size());

  FormFieldData expected;
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);

  expected.set_label(u"Phone:");
  expected.set_name_attribute(u"dayphone1");
  expected.set_name(expected.name_attribute());
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.set_label(
      base::FeatureList::IsEnabled(
          features::kAutofillConsiderPhoneNumberSeparatorsValidLabels)
          ? u"-"
          : u"");
  expected.set_name_attribute(u"dayphone2");
  expected.set_name(expected.name_attribute());
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.set_label(
      base::FeatureList::IsEnabled(
          features::kAutofillConsiderPhoneNumberSeparatorsValidLabels)
          ? u"-"
          : u"");
  expected.set_name_attribute(u"dayphone3");
  expected.set_name(expected.name_attribute());
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.set_label(u"ext.:");
  expected.set_name_attribute(u"dayphone4");
  expected.set_name(expected.name_attribute());
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);
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

  WebVector<WebFormElement> forms = frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());

  FormData form = *ExtractFormData(forms[0]);
  EXPECT_EQ(u"TestForm", form.name());
  EXPECT_EQ(GURL("http://cnn.com"), form.action());

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(6U, fields.size());

  FormFieldData expected;
  expected.set_form_control_type(FormControlType::kInputText);

  expected.set_name_attribute(u"dayphone1");
  expected.set_label(u"Phone:");
  expected.set_name(expected.name_attribute());
  expected.set_max_length(3);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.set_name_attribute(u"dayphone2");
  expected.set_label(
      base::FeatureList::IsEnabled(
          features::kAutofillConsiderPhoneNumberSeparatorsValidLabels)
          ? u"-"
          : u"");
  expected.set_name(expected.name_attribute());
  expected.set_max_length(3);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.set_name_attribute(u"dayphone3");
  expected.set_label(
      base::FeatureList::IsEnabled(
          features::kAutofillConsiderPhoneNumberSeparatorsValidLabels)
          ? u"-"
          : u"");
  expected.set_name(expected.name_attribute());
  expected.set_max_length(4);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.set_name_attribute(u"dayphone4");
  expected.set_label(u"ext.:");
  expected.set_name(expected.name_attribute());
  expected.set_max_length(5);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  // When unspecified `size`, default is returned.
  expected.set_name_attribute(u"default1");
  expected.set_label({});
  expected.set_name(expected.name_attribute());
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[4]);

  // When invalid `size`, default is returned.
  expected.set_name_attribute(u"invalid1");
  expected.set_label({});
  expected.set_name(expected.name_attribute());
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[5]);
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

TEST_F(FormAutofillTest, FillFormModifyValues) {
  TestFillFormAndModifyValues(
      R"(<form name=TestForm action='http://abc.com'>
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
         </form>)",
      "First Name", "Last Name", "Phone", "Credit Card Number", "City",
      "State");
}

TEST_F(FormAutofillTest, FillFormModifyInitiatingValue) {
  TestFillFormAndModifyInitiatingValue(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=cc placeholder='Credit Card Number' value='Credit Card'>
           <input id=expiration_date placeholder='Expiration Date'
                  value='Expiration Date'>
           <input id=name placeholder='Full Name' value='Full Name'>
           <input type=submit value=Send>
         </form>)",
      "Credit Card Number", "Expiration Date", "Full Name");
}

TEST_F(FormAutofillTest, FillFormJSModifiesUserInputValue) {
  TestFillFormJSModifiesUserInputValue(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=cc placeholder='Credit Card Number' value='Credit Card'>
           <input id=expiration_date placeholder='Expiration Date'
                  value='Expiration Date'>
           <input id=name placeholder='Full Name' value='Full Name'>
           <input type=submit value=Send>
         </form>)",
      "Credit Card Number", "Expiration Date", "Full Name");
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

  WebVector<WebFormElement> forms =
      GetMainFrame()->GetDocument().GetTopLevelForms();
  EXPECT_EQ(1U, forms.size());

  FormData form = *ExtractFormData(forms[0]);

  EXPECT_EQ(form.fields().size(), 4u);
  std::vector<FormFieldData> undo_fields;
  for (size_t i = 0; i < 4; i += 2) {
    std::u16string type = i == 0 ? u"text" : u"select_option";
    test_api(form).field(i).set_value(u"undo_" + type + u"_1");
    test_api(form).field(i).set_is_autofilled(false);
    undo_fields.push_back(form.fields()[i]);
  }

  form.set_fields(undo_fields);
  ExecuteJavaScriptForTests("document.getElementById('text_id_1').focus();");
  ApplyFieldsAction(text_element_1.GetDocument(), form.fields(),
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

TEST_F(FormAutofillTest, ClearPreviewedElements) {
  TestClearPreviewedElements(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname value=Wyatt>
           <input id=lastname>
           <input id=email>
           <input type=email id=email2>
           <input type=tel id=phone>
           <input type=submit value=Send>
         </form>)");
}

TEST_F(FormAutofillTest, ClearPreviewedFormWithElementForUnownedForm) {
  TestClearPreviewedElements(
      R"(<head><title>store checkout</title></head>
         <input id=firstname value=Wyatt>
         <input id=lastname>
         <input id=email>
         <input type=email id=email2>
         <input type=tel id=phone>
         <input type=submit value=Send>)");
}

TEST_F(FormAutofillTest, ClearPreviewedFormWithNonEmptyInitiatingNode) {
  TestClearPreviewedFormWithNonEmptyInitiatingNode(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname value=W>
           <input id=lastname>
           <input id=email>
           <input type=email id=email2>
           <input type=tel id=phone>
           <input type=submit value=Send>
         </form>)");
}

TEST_F(FormAutofillTest,
       ClearPreviewedFormWithNonEmptyInitiatingNodeForUnownedForm) {
  TestClearPreviewedFormWithNonEmptyInitiatingNode(
      R"(<head><title>shipping details</title></head>
         <input id=firstname value=W>
         <input id=lastname>
         <input id=email>
         <input type=email id=email2>
         <input type=tel id=phone>
         <input type=submit value=Send>)");
}

TEST_F(FormAutofillTest, ClearPreviewedFormWithAutofilledInitiatingNode) {
  TestClearPreviewedFormWithAutofilledInitiatingNode(
      R"(<form name=TestForm action='http://abc.com'>
           <input id=firstname value=W>
           <input id=lastname>
           <input id=email>
           <input type=email id=email2>
           <input type=tel id=phone>
           <input type=submit value=Send>
         </form>)");
}

TEST_F(FormAutofillTest,
       ClearPreviewedFormWithAutofilledInitiatingNodeForUnownedForm) {
  TestClearPreviewedFormWithAutofilledInitiatingNode(
      R"(<head><title>shipping details</title></head>
         <input id=firstname value=W>
         <input id=lastname>
         <input id=email>
         <input type=email id=email2>
         <input type=tel id=phone>
         <input type=submit value=Send>)");
}

// If we have multiple labels per id, the labels concatenated into label string.
TEST_F(FormAutofillTest, MultipleLabelsPerElement) {
  std::vector<std::u16string> id_attributes, name_attributes, labels, names,
      values;

  id_attributes.push_back(u"firstname");
  name_attributes.emplace_back();
  labels.push_back(u"First Name:");
  names.push_back(id_attributes.back());
  values.push_back(u"John");

  id_attributes.push_back(u"lastname");
  name_attributes.emplace_back();
  labels.push_back(u"Last Name:");
  names.push_back(id_attributes.back());
  values.push_back(u"Smith");

  id_attributes.push_back(u"email");
  name_attributes.emplace_back();
  labels.push_back(u"Email: xxx@yyy.com");
  names.push_back(id_attributes.back());
  values.push_back(u"john@example.com");

  ExpectLabels(
      R"(<form name=TestForm action='http://cnn.com'>
           <label for=firstname> First Name: </label>
           <label for=firstname></label>
             <input id=firstname value=John>
           <label for=lastname></label>
           <label for=lastname> Last Name: </label>
             <input id=lastname value=Smith>
           <label for=email> Email: </label>
           <label for=email> xxx@yyy.com </label>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)",
      id_attributes, name_attributes, labels, names, values);
}

TEST_F(FormAutofillTest, ClickElement) {
  LoadHTML(R"(<button id=link>Button</button>
              <button name=button>Button</button>)");
  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  // Successful retrieval by id.
  WebElementDescriptor clicker;
  clicker.retrieval_method = WebElementDescriptor::ID;
  clicker.descriptor = "link";
  EXPECT_TRUE(ClickElement(frame->GetDocument(), clicker));

  // Successful retrieval by css selector.
  clicker.retrieval_method = WebElementDescriptor::CSS_SELECTOR;
  clicker.descriptor = "button[name='button']";
  EXPECT_TRUE(ClickElement(frame->GetDocument(), clicker));

  // Unsuccessful retrieval due to invalid CSS selector.
  clicker.descriptor = "^*&";
  EXPECT_FALSE(ClickElement(frame->GetDocument(), clicker));

  // Unsuccessful retrieval because element does not exist.
  clicker.descriptor = "#junk";
  EXPECT_FALSE(ClickElement(frame->GetDocument(), clicker));
}

TEST_F(FormAutofillTest, SelectOneAsText) {
  LoadHTML(R"(<form name=TestForm action='http://cnn.com'>
                <input id=firstname value=John>
                <input id=lastname value=Smith>
                <select id=country>
                  <option value=AF>Afghanistan</option>
                  <option value=AL>Albania</option>
                  <option value=DZ>Algeria</option>
                </select>
                <input type=submit name='reply-send' value=Send>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  // Set the value of the select-one.
  WebSelectElement select_element =
      frame->GetDocument().GetElementById("country").To<WebSelectElement>();
  select_element.SetValue(WebString::FromUTF8("AL"));

  WebVector<WebFormElement> forms = frame->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());

  FormData form = *ExtractFormData(forms[0]);
  EXPECT_EQ(u"TestForm", form.name());
  EXPECT_EQ(GURL("http://cnn.com"), form.action());

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;

  expected.set_id_attribute(u"firstname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"John");
  expected.set_label(u"John");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.set_id_attribute(u"lastname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Smith");
  expected.set_label(u"Smith");
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.set_id_attribute(u"country");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"AL");
  expected.set_label({});
  expected.set_form_control_type(FormControlType::kSelectOne);
  expected.set_max_length(0);
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
  EXPECT_THAT(fields[2].selected_option().CopyAsOptional(),
              Optional(Field(&SelectOption::text, u"Albania")));
}

TEST_F(FormAutofillTest, UnownedFormElementsToFormDataWithoutForm) {
  LoadHTML(R"(<head><title>delivery info</title></head>
              <div>
                <label for=firstname>First name:</label>
                <label for=lastname>Last name:</label>
                <input id=firstname value=John>
                <input id=lastname value=Smith>
                <label for=email>Email:</label>
                <input id=email value='john@example.com'>
              </div>)");
  FormData form = *ExtractFormData(WebFormElement());

  EXPECT_TRUE(form.name().empty());
  EXPECT_FALSE(form.action().is_valid());

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.set_form_control_type(FormControlType::kInputText);
  expected.set_max_length(FormFieldData::kDefaultMaxLength);

  expected.set_id_attribute(u"firstname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"John");
  expected.set_label(u"First name:");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.set_id_attribute(u"lastname");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"Smith");
  expected.set_label(u"Last name:");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.set_id_attribute(u"email");
  expected.set_name(expected.id_attribute());
  expected.set_value(u"john@example.com");
  expected.set_label(u"Email:");
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
}

TEST_F(FormAutofillTest, UnownedFormElementsToFormDataWithForm) {
  LoadHTML(kFormHtml);
  EXPECT_FALSE(ExtractFormData(WebFormElement()));
}

TEST_F(FormAutofillTest, FormlessForms) {
  LoadHTML(kUnownedUntitledFormHtml);
  EXPECT_TRUE(ExtractFormData(WebFormElement()));
}

TEST_F(FormAutofillTest, FormCache_ExtractNewForms) {
  struct {
    const char* description;
    const char* html;
    const size_t number_of_extracted_forms;
    const bool is_form_tag;
  } test_cases[] = {
      // An empty form should not be extracted
      {"Empty Form",
       R"(<form name=TestForm action='http://abc.com'>
          </form>)",
       0u, true},
      // A form with less than three fields with no autocomplete type(s) should
      // be extracted because no minimum is being enforced for upload.
      {"Small Form no autocomplete",
       R"(<form name=TestForm action='http://abc.com'>
            <input id=firstname>
          </form>)",
       1u, true},
      // A form with less than three fields with at least one autocomplete type
      // should be extracted.
      {"Small Form w/ autocomplete",
       R"(<form name=TestForm action='http://abc.com'>
            <input id=firstname autocomplete='given-name'>
          </form>)",
       1u, true},
      // A form with three or more fields should be extracted.
      {"3 Field Form",
       R"(<form name=TestForm action='http://abc.com'>
            <input id=firstname>
            <input id=lastname>
            <input id=email>
            <input type=submit value=Send>
          </form>)",
       1u, true},
      // An input field with an autocomplete attribute outside of a form should
      // be extracted.
      {"Small, formless, with autocomplete",
       R"(<input id=firstname autocomplete='given-name'>
          <input type=submit value=Send>)",
       1u, false},
      // An input field without an autocomplete attribute outside of a form,
      // with no checkout hints, should not be extracted.
      {"Small, formless, no autocomplete",
       R"(<input id=firstname>
          <input type=submit value=Send>)",
       1u, false},
      // A form with one field which is password gets extracted.
      {"Password-Only",
       R"(<form name=TestForm action='http://abc.com'>
            <input type=password id=pw>
          </form>)",
       1u, true},
      // A form with two fields which are passwords should be extracted.
      {"two passwords",
       R"(<form name=TestForm action='http://abc.com'>
            <input type=password id=pw>
            <input type=password id=new_pw>
          </form>)",
       1u, true},
  };

  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);

    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    EXPECT_EQ(test_case.number_of_extracted_forms, forms.size());
    if (!forms.empty())
      EXPECT_EQ(test_case.is_form_tag, !forms.back().renderer_id().is_null());
  }
}

TEST_F(FormAutofillTest, AriaLabelAndDescription) {
  LoadHTML(
      R"(<form id=form>
           <div id=label>aria label</div>
           <div id=description>aria description</div>
           <input id=field0 aria-label='inline aria label'>
           <input id=field1 aria-labelledby='label'>
           <input id=field2 aria-describedby='description'>
         </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  WebFormControlElement control_element =
      frame->GetDocument().GetElementById("field0").To<WebFormControlElement>();
  ASSERT_TRUE(control_element);
  FormData form = FindForm(control_element);

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(3U, fields.size());

  // Field 0
  EXPECT_EQ(u"inline aria label", fields[0].aria_label());
  EXPECT_EQ(u"", fields[0].aria_description());

  // Field 1
  EXPECT_EQ(u"aria label", fields[1].aria_label());
  EXPECT_EQ(u"", fields[1].aria_description());

  // Field 2
  EXPECT_EQ(u"", fields[2].aria_label());
  EXPECT_EQ(u"aria description", fields[2].aria_description());
}

TEST_F(FormAutofillTest, AriaLabelAndDescription2) {
  LoadHTML(
      R"(<form id=form>
           <input id=field0 aria-label='inline aria label'>
           <input id=field1 aria-labelledby='label'>
           <input id=field2 aria-describedby='description'>
         </form>
         <div id=label>aria label</div>
         <div id=description>aria description</div>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  WebFormControlElement control_element =
      frame->GetDocument().GetElementById("field0").To<WebFormControlElement>();
  ASSERT_TRUE(control_element);
  FormData form = FindForm(control_element);

  const std::vector<FormFieldData>& fields = form.fields();
  ASSERT_EQ(3U, fields.size());

  // Field 0
  EXPECT_EQ(u"inline aria label", fields[0].aria_label());
  EXPECT_EQ(u"", fields[0].aria_description());

  // Field 1
  EXPECT_EQ(u"aria label", fields[1].aria_label());
  EXPECT_EQ(u"", fields[1].aria_description());

  // Field 2
  EXPECT_EQ(u"", fields[2].aria_label());
  EXPECT_EQ(u"aria description", fields[2].aria_description());
}

}  // namespace
}  // namespace autofill::form_util
