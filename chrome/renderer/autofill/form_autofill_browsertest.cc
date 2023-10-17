// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
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
#include "third_party/blink/public/web/web_select_list_element.h"

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

namespace autofill::form_util {

namespace {

struct AutofillFieldCase {
  FormControlType form_control_type;
  const char* const id_attribute;
  const char* const initial_value;
  const char* const autocomplete_attribute;  // The autocomplete attribute of
                                             // the element.
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
    "<FORM name='TestForm' action='http://abc.com' method='post'>"
    "  <INPUT type='text' id='firstname'/>"
    "  <INPUT type='text' id='lastname'/>"
    "  <INPUT type='hidden' id='imhidden'/>"
    "  <INPUT type='text' id='notempty' value='Hi'/>"
    "  <INPUT type='text' autocomplete='off' id='noautocomplete'/>"
    "  <INPUT type='text' disabled='disabled' id='notenabled'/>"
    "  <INPUT type='text' readonly id='readonly'/>"
    "  <INPUT type='text' style='visibility: hidden'"
    "         id='invisible'/>"
    "  <INPUT type='text' style='display: none' id='displaynone'/>"
    "  <INPUT type='month' id='month'/>"
    "  <INPUT type='month' id='month-nonempty' value='2011-12'/>"
    "  <SELECT id='select'>"
    "    <OPTION></OPTION>"
    "    <OPTION value='CA'>California</OPTION>"
    "    <OPTION value='TX'>Texas</OPTION>"
    "  </SELECT>"
    "  <SELECT id='select-nonempty'>"
    "    <OPTION value='CA' selected>California</OPTION>"
    "    <OPTION value='TX'>Texas</OPTION>"
    "  </SELECT>"
    "  <SELECT id='select-unchanged'>"
    "    <OPTION value='CA' selected>California</OPTION>"
    "    <OPTION value='TX'>Texas</OPTION>"
    "  </SELECT>"
    "  <SELECT id='select-displaynone' style='display:none'>"
    "    <OPTION value='CA' selected>California</OPTION>"
    "    <OPTION value='TX'>Texas</OPTION>"
    "  </SELECT>"
    "  <TEXTAREA id='textarea'></TEXTAREA>"
    "  <TEXTAREA id='textarea-nonempty'>Go&#10;away!</TEXTAREA>"
    "  <INPUT type='submit' name='reply-send' value='Send'/>"
    "</FORM>";

// This constant uses a mixed-case title tag to be sure that the title match is
// not case-sensitive. Other tests in this file use an all-lower title tag.
const char kUnownedFormHtml[] =
    "<HEAD><TITLE>Enter Shipping Info</TITLE></HEAD>"
    "<INPUT type='text' id='firstname'/>"
    "<INPUT type='text' id='lastname'/>"
    "<INPUT type='hidden' id='imhidden'/>"
    "<INPUT type='text' id='notempty' value='Hi'/>"
    "<INPUT type='text' autocomplete='off' id='noautocomplete'/>"
    "<INPUT type='text' disabled='disabled' id='notenabled'/>"
    "<INPUT type='text' readonly id='readonly'/>"
    "<INPUT type='text' style='visibility: hidden'"
    "       id='invisible'/>"
    "<INPUT type='text' style='display: none' id='displaynone'/>"
    "<INPUT type='month' id='month'/>"
    "<INPUT type='month' id='month-nonempty' value='2011-12'/>"
    "<SELECT id='select'>"
    "  <OPTION></OPTION>"
    "  <OPTION value='CA'>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<SELECT id='select-nonempty'>"
    "  <OPTION value='CA' selected>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<SELECT id='select-unchanged'>"
    "  <OPTION value='CA' selected>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<SELECT id='select-displaynone' style='display:none'>"
    "  <OPTION value='CA' selected>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<TEXTAREA id='textarea'></TEXTAREA>"
    "<TEXTAREA id='textarea-nonempty'>Go&#10;away!</TEXTAREA>"
    "<INPUT type='submit' name='reply-send' value='Send'/>";

// This constant has no title tag, and should be passed to
// LoadHTMLWithURLOverride to test the detection of unowned forms by URL.
const char kUnownedUntitledFormHtml[] =
    "<INPUT type='text' id='firstname'/>"
    "<INPUT type='text' id='lastname'/>"
    "<INPUT type='hidden' id='imhidden'/>"
    "<INPUT type='text' id='notempty' value='Hi'/>"
    "<INPUT type='text' autocomplete='off' id='noautocomplete'/>"
    "<INPUT type='text' disabled='disabled' id='notenabled'/>"
    "<INPUT type='text' readonly id='readonly'/>"
    "<INPUT type='text' style='visibility: hidden'"
    "       id='invisible'/>"
    "<INPUT type='text' style='display: none' id='displaynone'/>"
    "<INPUT type='month' id='month'/>"
    "<INPUT type='month' id='month-nonempty' value='2011-12'/>"
    "<SELECT id='select'>"
    "  <OPTION></OPTION>"
    "  <OPTION value='CA'>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<SELECT id='select-nonempty'>"
    "  <OPTION value='CA' selected>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<SELECT id='select-unchanged'>"
    "  <OPTION value='CA' selected>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<SELECT id='select-displaynone' style='display:none'>"
    "  <OPTION value='CA' selected>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<TEXTAREA id='textarea'></TEXTAREA>"
    "<TEXTAREA id='textarea-nonempty'>Go&#10;away!</TEXTAREA>"
    "<INPUT type='submit' name='reply-send' value='Send'/>";

// This constant does not have a title tag, but should match an unowned form
// anyway because it is not English.
const char kUnownedNonEnglishFormHtml[] =
    "<HTML LANG='fr'>"
    "<INPUT type='text' id='firstname'/>"
    "<INPUT type='text' id='lastname'/>"
    "<INPUT type='hidden' id='imhidden'/>"
    "<INPUT type='text' id='notempty' value='Hi'/>"
    "<INPUT type='text' autocomplete='off' id='noautocomplete'/>"
    "<INPUT type='text' disabled='disabled' id='notenabled'/>"
    "<INPUT type='text' readonly id='readonly'/>"
    "<INPUT type='text' style='visibility: hidden'"
    "       id='invisible'/>"
    "<INPUT type='text' style='display: none' id='displaynone'/>"
    "<INPUT type='month' id='month'/>"
    "<INPUT type='month' id='month-nonempty' value='2011-12'/>"
    "<SELECT id='select'>"
    "  <OPTION></OPTION>"
    "  <OPTION value='CA'>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<SELECT id='select-nonempty'>"
    "  <OPTION value='CA' selected>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<SELECT id='select-unchanged'>"
    "  <OPTION value='CA' selected>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<SELECT id='select-displaynone' style='display:none'>"
    "  <OPTION value='CA' selected>California</OPTION>"
    "  <OPTION value='TX'>Texas</OPTION>"
    "</SELECT>"
    "<TEXTAREA id='textarea'></TEXTAREA>"
    "<TEXTAREA id='textarea-nonempty'>Go&#10;away!</TEXTAREA>"
    "<INPUT type='submit' name='reply-send' value='Send'/>"
    "</HTML>";

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
  NOTREACHED();
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

  if (element.IsNull()) {
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

}  // namespace

class FormAutofillTest : public ChromeRenderViewTest {
 public:
  FormAutofillTest() = default;

  FormAutofillTest(const FormAutofillTest&) = delete;
  FormAutofillTest& operator=(const FormAutofillTest&) = delete;

  ~FormAutofillTest() override = default;

#if BUILDFLAG(IS_WIN)
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Autofill uses the system font to render suggestion previews. On Windows
    // an extra step is required to ensure that the system font is configured.
    blink::WebFontRendering::SetMenuFontMetrics(
        blink::WebString::FromASCII("Arial"), 12);
  }
#endif

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
      expected.id_attribute = id_attributes[i];
      expected.name_attribute = name_attributes[i];
      expected.label = labels[i];
      expected.name = names[i];
      expected.value = values[i];
      expected.form_control_type = FormControlType::kInputText;
      expected.max_length = FormFieldData::kDefaultMaxLength;
      fields.push_back(expected);
    }
    ExpectLabelsAndTypes(html, fields);
  }

  void ExpectLabelsAndTypes(const char* html,
                            const std::vector<FormFieldData>& fields) {
    LoadHTML(html);

    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    const FormData& form = forms[0];
    EXPECT_EQ(u"TestForm", form.name);
    EXPECT_EQ(GURL("http://cnn.com"), form.action);
    ASSERT_EQ(fields.size(), form.fields.size());

    for (size_t i = 0; i < fields.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));
      EXPECT_FORM_FIELD_DATA_EQUALS(fields[i], form.fields[i]);
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
    if (url_override)
      LoadHTMLWithUrlOverride(html, url_override);
    else
      LoadHTML(html);

    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Find the form that contains the input element.
    FormData form_data;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form_data, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form_data.name);
      EXPECT_EQ(GURL("http://abc.com"), form_data.action);
    }

    const std::vector<FormFieldData>& fields = form_data.fields;
    ASSERT_EQ(number_of_field_cases, fields.size());

    FormFieldData expected;
    // Verify field's initial value.
    for (size_t i = 0; i < number_of_field_cases; ++i) {
      SCOPED_TRACE(base::StringPrintf("Verify initial value for field %s",
                                      field_cases[i].id_attribute));
      expected.form_control_type = field_cases[i].form_control_type;
      expected.max_length =
          (expected.form_control_type == FormControlType::kInputText ||
           expected.form_control_type == FormControlType::kTextArea)
              ? FormFieldData::kDefaultMaxLength
              : 0;
      expected.id_attribute = ASCIIToUTF16(field_cases[i].id_attribute);
      expected.name = expected.id_attribute;
      expected.value = ASCIIToUTF16(field_cases[i].initial_value);
      if (expected.form_control_type == FormControlType::kInputText ||
          expected.form_control_type == FormControlType::kInputMonth) {
        expected.label = ASCIIToUTF16(field_cases[i].initial_value);
      } else {
        expected.label.clear();
      }
      expected.autocomplete_attribute = field_cases[i].autocomplete_attribute;
      EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[i]);
      // Fill the form_data for the field.
      form_data.fields[i].value = ASCIIToUTF16(field_cases[i].autofill_value);
      // Set the is_autofilled property for the field.
      form_data.fields[i].is_autofilled = field_cases[i].should_be_autofilled;
    }

    // Autofill the form using the given fill form function.
    ApplyFormAction(form_data, input_element, mojom::ActionType::kFill,
                    action_persistence);

    // Validate Autofill or Preview results.
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
    WebString value;
    WebFormControlElement element = GetFormControlElementById(
        WebString::FromASCII(field_case.id_attribute));
    if ((element.FormControlType() ==
         blink::mojom::FormControlType::kSelectOne) ||
        (element.FormControlType() ==
         blink::mojom::FormControlType::kTextArea)) {
      value = get_value_function(element);
    } else {
      ASSERT_TRUE(element.FormControlType() ==
                      blink::mojom::FormControlType::kInputText ||
                  element.FormControlType() ==
                      blink::mojom::FormControlType::kInputMonth);
      value = get_value_function(element);
    }

    const WebString expected_value =
        WebString::FromASCII(field_case.expected_value);
    if (expected_value.IsEmpty())
      EXPECT_TRUE(value.IsEmpty());
    else
      EXPECT_EQ(expected_value.Utf8(), value.Utf8());

    EXPECT_EQ(field_case.should_be_autofilled,
              action_persistence == mojom::ActionPersistence::kFill
                  ? element.IsAutofilled()
                  : element.IsPreviewed());
  }

  WebFormControlElement GetFormControlElementById(const WebString& id) {
    return GetMainFrame()
        ->GetDocument()
        .GetElementById(id)
        .To<WebFormControlElement>();
  }

  WebInputElement GetInputElementById(const WebString& id) {
    return GetMainFrame()
        ->GetDocument()
        .GetElementById(id)
        .To<WebInputElement>();
  }

  void TestFillForm(const char* html, bool unowned, const char* url_override) {
    static const AutofillFieldCase field_cases[] = {
        // Fields: form_control_type, name, initial_value,
        // autocomplete_attribute, should_be_autofilled, autofill_value,
        // expected_value.
        // Regular empty fields (firstname & lastname) should be autofilled.
        {FormControlType::kInputText, "firstname", "", "", true,
         "filled firstname", "filled firstname"},
        {FormControlType::kInputText, "lastname", "", "", true,
         "filled lastname", "filled lastname"},
        // hidden fields should not be extracted to form_data.
        // Non empty fields should not be autofilled.
        {FormControlType::kInputText, "notempty", "Hi", "", false,
         "filled notempty", "Hi"},
        {FormControlType::kInputText, "noautocomplete", "", "off", true,
         "filled noautocomplete", "filled noautocomplete"},
        // Disabled fields should not be autofilled.
        {FormControlType::kInputText, "notenabled", "", "", false,
         "filled notenabled", ""},
        // Readonly fields should not be autofilled.
        {FormControlType::kInputText, "readonly", "", "", false,
         "filled readonly", ""},
        // Fields with "visibility: hidden" should not be autofilled.
        {FormControlType::kInputText, "invisible", "", "", false,
         "filled invisible", ""},
        // Fields with "display:none" should not be autofilled.
        {FormControlType::kInputText, "displaynone", "", "", false,
         "filled displaynone", ""},
        // Regular <input type="month"> should be autofilled.
        {FormControlType::kInputMonth, "month", "", "", true, "2017-11",
         "2017-11"},
        // Non-empty <input type="month"> should not be autofilled.
        {FormControlType::kInputMonth, "month-nonempty", "2011-12", "", false,
         "2017-11", "2011-12"},
        // Regular select fields should be autofilled.
        {FormControlType::kSelectOne, "select", "", "", true, "TX", "TX"},
        // Select fields should be autofilled even if they already have a
        // non-empty value.
        {FormControlType::kSelectOne, "select-nonempty", "CA", "", true, "TX",
         "TX"},
        // Select fields should not be autofilled if no new value is passed from
        // autofill profile. The existing value should not be overriden.
        {FormControlType::kSelectOne, "select-unchanged", "CA", "", false, "CA",
         "CA"},
        // Select fields that are not focusable should always be filled.
        {FormControlType::kSelectOne, "select-displaynone", "CA", "", true,
         "CA", "CA"},
        // Regular textarea elements should be autofilled.
        {FormControlType::kTextArea, "textarea", "", "", true,
         "some multi-\nline value", "some multi-\nline value"},
        // Non-empty textarea elements should not be autofilled.
        {FormControlType::kTextArea, "textarea-nonempty", "Go\naway!", "",
         false, "some multi-\nline value", "Go\naway!"},
    };
    TestFormFillFunctions(html, unowned, url_override, field_cases,
                          std::size(field_cases),
                          mojom::ActionPersistence::kFill, &GetValueWrapper);
    // Verify preview selection.
    WebInputElement firstname = GetInputElementById("firstname");
    EXPECT_EQ(16u, firstname.SelectionStart());
    EXPECT_EQ(16u, firstname.SelectionEnd());
  }

  void TestPreviewForm(const char* html, bool unowned,
                       const char* url_override) {
    static const AutofillFieldCase field_cases[] = {
        // Normal empty fields should be previewed.
        {FormControlType::kInputText, "firstname", "", "", true,
         "suggested firstname", "suggested firstname"},
        {FormControlType::kInputText, "lastname", "", "", true,
         "suggested lastname", "suggested lastname"},
        // Hidden fields should not be extracted to form_data.
        // Non empty fields should not be previewed.
        {FormControlType::kInputText, "notempty", "Hi", "", false,
         "suggested notempty", ""},
        {FormControlType::kInputText, "noautocomplete", "", "off", true,
         "filled noautocomplete", "filled noautocomplete"},
        // Disabled fields should not be previewed.
        {FormControlType::kInputText, "notenabled", "", "", false,
         "suggested notenabled", ""},
        // Readonly fields should not be previewed.
        {FormControlType::kInputText, "readonly", "", "", false,
         "suggested readonly", ""},
        // Fields with "visibility: hidden" should not be previewed.
        {FormControlType::kInputText, "invisible", "", "", false,
         "suggested invisible", ""},
        // Fields with "display:none" should not previewed.
        {FormControlType::kInputText, "displaynone", "", "", false,
         "suggested displaynone", ""},
        // Regular <input type="month"> should be previewed.
        {FormControlType::kInputMonth, "month", "", "", true, "2017-11",
         "2017-11"},
        // Non-empty <input type="month"> should not be previewed.
        {FormControlType::kInputMonth, "month-nonempty", "2011-12", "", false,
         "2017-11", ""},
        // Regular select fields should be previewed.
        {FormControlType::kSelectOne, "select", "", "", true, "TX", "TX"},
        // Select fields should be previewed even if they already have a
        // non-empty value.
        {FormControlType::kSelectOne, "select-nonempty", "CA", "", true, "TX",
         "TX"},
        // Select fields should not be previewed if no suggestion is passed from
        // autofill profile.
        {FormControlType::kSelectOne, "select-unchanged", "CA", "", false, "",
         ""},
        // Select fields that are not focusable should always be filled.
        {FormControlType::kSelectOne, "select-displaynone", "CA", "", true,
         "CA", "CA"},
        // Normal textarea elements should be previewed.
        {FormControlType::kTextArea, "textarea", "", "", true,
         "suggested multi-\nline value", "suggested multi-\nline value"},
        // Nonempty textarea elements should not be previewed.
        {FormControlType::kTextArea, "textarea-nonempty", "Go\naway!", "",
         false, "suggested multi-\nline value", ""},
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
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Find the form and verify it's the correct form.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name);
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(4U, fields.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value = u"John";
    expected.label = u"John";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, field);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value = u"Smith";
    expected.label = u"Smith";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    expected.value = u"john@example.com";
    expected.label = u"john@example.com";
    expected.autocomplete_attribute = "off";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
    expected.autocomplete_attribute.clear();

    expected.id_attribute = u"phone";
    expected.name = expected.id_attribute;
    expected.value = u"1.800.555.1234";
    expected.label = u"1.800.555.1234";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);
  }

  void TestFindFormForTextAreaElement(const char* html, bool unowned) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the textarea element we want to find.
    WebElement element =
        web_frame->GetDocument().GetElementById("street-address");
    WebFormControlElement textarea_element =
        element.To<WebFormControlElement>();

    // Find the form and verify it's the correct form.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(textarea_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name);
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(4U, fields.size());

    FormFieldData expected;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value = u"John";
    expected.label = u"John";
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value = u"Smith";
    expected.label = u"Smith";
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    expected.value = u"john@example.com";
    expected.label = u"john@example.com";
    expected.autocomplete_attribute = "off";
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
    expected.autocomplete_attribute.clear();

    expected.id_attribute = u"street-address";
    expected.name = expected.id_attribute;
    expected.value = u"123 Fantasy Ln.\nApt. 42";
    expected.label.clear();
    expected.form_control_type = FormControlType::kTextArea;
    expected.max_length = FormFieldData::kDefaultMaxLength;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, field);
  }

  void TestFillFormMaxLength(const char* html, bool unowned) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Find the form that contains the input element.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name);
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.max_length = 5;
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.max_length = 7;
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    expected.max_length = 9;
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

    // Fill the form.
    form.fields[0].value = u"Brother";
    form.fields[1].value = u"Jonathan";
    form.fields[2].value = u"brotherj@example.com";
    form.fields[0].is_autofilled = true;
    form.fields[1].is_autofilled = true;
    form.fields[2].is_autofilled = true;
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2;
    FormFieldData field2;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form2, &field2));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form2.name);
      EXPECT_EQ(GURL("http://abc.com"), form2.action);
    }

    const std::vector<FormFieldData>& fields2 = form2.fields;
    ASSERT_EQ(3U, fields2.size());

    expected.form_control_type = FormControlType::kInputText;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value = u"Broth";
    expected.max_length = 5;
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value = u"Jonatha";
    expected.max_length = 7;
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    expected.value = u"brotherj@";
    expected.max_length = 9;
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
  }

  void TestFillFormNegativeMaxLength(const char* html, bool unowned) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Find the form that contains the input element.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name);
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

    // Fill the form.
    form.fields[0].value = u"Brother";
    form.fields[1].value = u"Jonathan";
    form.fields[2].value = u"brotherj@example.com";
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2;
    FormFieldData field2;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form2, &field2));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form2.name);
      EXPECT_EQ(GURL("http://abc.com"), form2.action);
    }

    const std::vector<FormFieldData>& fields2 = form2.fields;
    ASSERT_EQ(3U, fields2.size());

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value = u"Brother";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value = u"Jonathan";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    expected.value = u"brotherj@example.com";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
  }

  void TestFillFormEmptyName(const char* html, bool unowned) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Find the form that contains the input element.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name);
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

    // Fill the form.
    form.fields[0].value = u"Wyatt";
    form.fields[1].value = u"Earp";
    form.fields[2].value = u"wyatt@example.com";
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2;
    FormFieldData field2;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form2, &field2));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form2.name);
      EXPECT_EQ(GURL("http://abc.com"), form2.action);
    }

    const std::vector<FormFieldData>& fields2 = form2.fields;
    ASSERT_EQ(3U, fields2.size());

    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value = u"Wyatt";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value = u"Earp";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    expected.value = u"wyatt@example.com";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
  }

  void TestFillFormEmptyFormNames(const char* html, bool unowned) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    const size_t expected_size = unowned ? 1 : 2;
    ASSERT_EQ(expected_size, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("apple");

    // Find the form that contains the input element.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    if (!unowned) {
      EXPECT_TRUE(form.name.empty());
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    const size_t unowned_offset = unowned ? 3 : 0;
    ASSERT_EQ(unowned_offset + 3, fields.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"apple";
    expected.name = expected.id_attribute;
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[unowned_offset]);

    expected.id_attribute = u"banana";
    expected.name = expected.id_attribute;
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[unowned_offset + 1]);

    expected.id_attribute = u"cantelope";
    expected.name = expected.id_attribute;
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[unowned_offset + 2]);

    // Fill the form.
    form.fields[unowned_offset + 0].value = u"Red";
    form.fields[unowned_offset + 1].value = u"Yellow";
    form.fields[unowned_offset + 2].value = u"Also Yellow";
    form.fields[unowned_offset + 0].is_autofilled = true;
    form.fields[unowned_offset + 1].is_autofilled = true;
    form.fields[unowned_offset + 2].is_autofilled = true;
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2;
    FormFieldData field2;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form2, &field2));
    if (!unowned) {
      EXPECT_TRUE(form2.name.empty());
      EXPECT_EQ(GURL("http://abc.com"), form2.action);
    }

    const std::vector<FormFieldData>& fields2 = form2.fields;
    ASSERT_EQ(unowned_offset + 3, fields2.size());

    expected.id_attribute = u"apple";
    expected.name = expected.id_attribute;
    expected.value = u"Red";
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[unowned_offset + 0]);

    expected.id_attribute = u"banana";
    expected.name = expected.id_attribute;
    expected.value = u"Yellow";
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[unowned_offset + 1]);

    expected.id_attribute = u"cantelope";
    expected.name = expected.id_attribute;
    expected.value = u"Also Yellow";
    expected.is_autofilled = true;
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
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");

    // Simulate typing by modifying the field value.
    input_element.SetValue(WebString::FromASCII("Wy"));

    // Find the form that contains the input element.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name);
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value = u"Wy";
    if (placeholder_firstname) {
      expected.label = ASCIIToUTF16(placeholder_firstname);
      expected.placeholder = ASCIIToUTF16(placeholder_firstname);
    }
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    if (initial_lastname) {
      expected.label = ASCIIToUTF16(initial_lastname);
      expected.value = ASCIIToUTF16(initial_lastname);
    } else if (placeholder_lastname) {
      expected.label = ASCIIToUTF16(placeholder_lastname);
      expected.placeholder = ASCIIToUTF16(placeholder_lastname);
      expected.value = ASCIIToUTF16(placeholder_lastname);
    } else {
      expected.label.clear();
      expected.value.clear();
    }
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    if (initial_email) {
      expected.label = ASCIIToUTF16(initial_email);
      expected.value = ASCIIToUTF16(initial_email);
    } else if (placeholder_email) {
      expected.label = ASCIIToUTF16(placeholder_email);
      expected.placeholder = ASCIIToUTF16(placeholder_email);
      expected.value = ASCIIToUTF16(placeholder_email);
    } else {
      expected.label.clear();
      expected.value.clear();
    }
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

    // Preview the form and verify that the cursor position has been updated.
    form.fields[0].value = u"Wyatt";
    form.fields[1].value = u"Earp";
    form.fields[2].value = u"wyatt@example.com";
    form.fields[0].is_autofilled = true;
    form.fields[1].is_autofilled = true;
    form.fields[2].is_autofilled = true;
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kPreview);
    // The selection should be set after the second character.
    EXPECT_EQ(2u, input_element.SelectionStart());
    EXPECT_EQ(2u, input_element.SelectionEnd());

    // Fill the form.
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2;
    FormFieldData field2;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form2, &field2));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form2.name);
      EXPECT_EQ(GURL("http://abc.com"), form2.action);
    }

    const std::vector<FormFieldData>& fields2 = form2.fields;
    ASSERT_EQ(3U, fields2.size());

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value = u"Wyatt";
    if (placeholder_firstname) {
      expected.label = ASCIIToUTF16(placeholder_firstname);
      expected.placeholder = ASCIIToUTF16(placeholder_firstname);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value = u"Earp";
    if (placeholder_lastname) {
      expected.label = ASCIIToUTF16(placeholder_lastname);
      expected.placeholder = ASCIIToUTF16(placeholder_lastname);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    expected.value = u"wyatt@example.com";
    if (placeholder_email) {
      expected.label = ASCIIToUTF16(placeholder_email);
      expected.placeholder = ASCIIToUTF16(placeholder_email);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(5u, input_element.SelectionStart());
    EXPECT_EQ(5u, input_element.SelectionEnd());
  }

  void TestFillFormAndModifyValues(const char* html,
                                   const char* placeholder_firstname,
                                   const char* placeholder_lastname,
                                   const char* placeholder_phone,
                                   const char* placeholder_creditcard,
                                   const char* placeholder_city,
                                   const char* placeholder_state) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");
    WebFormElement form_element = input_element.Form();
    std::vector<WebFormControlElement> control_elements =
        ExtractAutofillableElementsInForm(form_element);

    ASSERT_EQ(6U, control_elements.size());
    // We now modify the values.
    // This will be ignored, the string will be sanitized into an empty string.
    control_elements[0].SetValue(WebString::FromUTF16(
        std::u16string(1, base::i18n::kLeftToRightMark) + u"     "));

    // This will be considered as a value entered by the user.
    control_elements[1].SetValue(WebString::FromUTF16(u"Earp"));
    control_elements[1].SetUserHasEditedTheFieldForTest();

    // This will be ignored, the string will be sanitized into an empty string.
    control_elements[2].SetValue(WebString::FromUTF16(u"(___)-___-____"));

    // This will be ignored, the string will be sanitized into an empty string.
    control_elements[3].SetValue(WebString::FromUTF16(u"____-____-____-____"));

    // This will be ignored, because it's injected by the website and not the
    // user.
    control_elements[4].SetValue(WebString::FromUTF16(u"Enter your city.."));

    control_elements[5].SetValue(WebString::FromUTF16(u"AK"));
    control_elements[5].SetUserHasEditedTheFieldForTest();

    // Find the form that contains the input element.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    EXPECT_EQ(u"TestForm", form.name);
    EXPECT_EQ(GURL("http://abc.com"), form.action);

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(6U, fields.size());

    // Preview the form and verify that the cursor position has been updated.
    form.fields[0].value = u"Wyatt";
    form.fields[1].value = u"Earpagus";
    form.fields[2].value = u"888-123-4567";
    form.fields[3].value = u"1111-2222-3333-4444";
    form.fields[4].value = u"Montreal";
    form.fields[5].value = u"AA";
    form.fields[0].is_autofilled = true;
    form.fields[1].is_autofilled = true;
    form.fields[2].is_autofilled = true;
    form.fields[3].is_autofilled = true;
    form.fields[4].is_autofilled = true;
    form.fields[5].is_autofilled = true;
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kPreview);
    // The selection should be set after the fifth character.
    EXPECT_EQ(5u, input_element.SelectionStart());
    EXPECT_EQ(5u, input_element.SelectionEnd());

    // Fill the form.
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2;
    FormFieldData field2;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form2, &field2));
    EXPECT_EQ(u"TestForm", form2.name);
    EXPECT_EQ(GURL("http://abc.com"), form2.action);

    const std::vector<FormFieldData>& fields2 = form2.fields;
    ASSERT_EQ(6U, fields2.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value = u"Wyatt";
    if (placeholder_firstname) {
      expected.label = ASCIIToUTF16(placeholder_firstname);
      expected.placeholder = ASCIIToUTF16(placeholder_firstname);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    // The last name field is not filled, because there is a value in it.
    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value = u"Earp";
    if (placeholder_lastname) {
      expected.label = ASCIIToUTF16(placeholder_lastname);
      expected.placeholder = ASCIIToUTF16(placeholder_lastname);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.id_attribute = u"phone";
    expected.name = expected.id_attribute;
    expected.value = u"888-123-4567";
    if (placeholder_phone) {
      expected.label = ASCIIToUTF16(placeholder_phone);
      expected.placeholder = ASCIIToUTF16(placeholder_phone);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

    expected.id_attribute = u"cc";
    expected.name = expected.id_attribute;
    expected.value = u"1111-2222-3333-4444";
    if (placeholder_creditcard) {
      expected.label = ASCIIToUTF16(placeholder_creditcard);
      expected.placeholder = ASCIIToUTF16(placeholder_creditcard);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[3]);

    expected.id_attribute = u"city";
    expected.name = expected.id_attribute;
    expected.value = u"Montreal";
    if (placeholder_city) {
      expected.label = ASCIIToUTF16(placeholder_city);
      expected.placeholder = ASCIIToUTF16(placeholder_city);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[4]);

    expected.form_control_type = FormControlType::kSelectOne;
    expected.id_attribute = u"state";
    expected.name_attribute = u"state";
    expected.name = expected.name_attribute;
    expected.value = u"AA";
    if (placeholder_state) {
      expected.label = ASCIIToUTF16(placeholder_state);
      expected.placeholder = ASCIIToUTF16(placeholder_state);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    expected.max_length = 0;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[5]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(5u, input_element.SelectionStart());
    EXPECT_EQ(5u, input_element.SelectionEnd());
  }

  void TestFillFormWithPlaceholderValues(const char* html,
                                         const char* placeholder_firstname,
                                         const char* placeholder_lastname,
                                         const char* placeholder_email) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("firstname");
    WebFormElement form_element = input_element.Form();
    std::vector<WebFormControlElement> control_elements =
        ExtractAutofillableElementsInForm(form_element);

    ASSERT_EQ(3U, control_elements.size());
    // We now modify the values.
    // These will be ignored, because it's (case insensitively) equal to the
    // placeholder.
    control_elements[0].SetValue(WebString::FromUTF16(
        std::u16string(1, base::i18n::kLeftToRightMark) + u"first name"));
    control_elements[1].SetValue(WebString::FromUTF16(u"LAST NAME"));
    // This will be considered.
    control_elements[2].SetValue(WebString::FromUTF16(u"john@smith.com"));
    // Find the form that contains the input element.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    EXPECT_EQ(u"TestForm", form.name);
    EXPECT_EQ(GURL("http://abc.com"), form.action);

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(3U, fields.size());

    // Preview the form and verify that the cursor position has been updated.
    form.fields[0].value = u"Wyatt";
    form.fields[1].value = u"Earpagus";
    form.fields[2].value = u"susan@smith.com";
    form.fields[0].is_autofilled = true;
    form.fields[1].is_autofilled = true;
    form.fields[2].is_autofilled = false;
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kPreview);
    // The selection should be set after the fifth character.
    EXPECT_EQ(5u, input_element.SelectionStart());
    EXPECT_EQ(5u, input_element.SelectionEnd());

    // Fill the form.
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2;
    FormFieldData field2;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form2, &field2));
    EXPECT_EQ(u"TestForm", form2.name);
    EXPECT_EQ(GURL("http://abc.com"), form2.action);

    const std::vector<FormFieldData>& fields2 = form2.fields;
    ASSERT_EQ(3U, fields2.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value = u"Wyatt";
    if (placeholder_firstname) {
      expected.label = ASCIIToUTF16(placeholder_firstname);
      expected.placeholder = ASCIIToUTF16(placeholder_firstname);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value = u"Earpagus";
    if (placeholder_lastname) {
      expected.label = ASCIIToUTF16(placeholder_lastname);
      expected.placeholder = ASCIIToUTF16(placeholder_lastname);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    // The email field is not filled, because there is a value in it.
    expected.id_attribute = u"email";
    expected.name = expected.id_attribute;
    expected.value = u"john@smith.com";
    if (placeholder_email) {
      expected.label = ASCIIToUTF16(placeholder_email);
      expected.placeholder = ASCIIToUTF16(placeholder_email);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(5u, input_element.SelectionStart());
    EXPECT_EQ(5u, input_element.SelectionEnd());
  }

  void TestFillFormAndModifyInitiatingValue(const char* html,
                                            const char* placeholder_creditcard,
                                            const char* placeholder_expiration,
                                            const char* placeholder_name) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("cc");
    WebFormElement form_element = input_element.Form();
    std::vector<WebFormControlElement> control_elements =
        ExtractAutofillableElementsInForm(form_element);

    ASSERT_EQ(3U, control_elements.size());
    // We now modify the values.
    // This will be ignored.
    control_elements[0].SetValue(WebString::FromUTF16(u"____-____-____-____"));
    // This will be ignored.
    control_elements[1].SetValue(WebString::FromUTF16(u"____/__"));
    control_elements[2].SetValue(WebString::FromUTF16(u"John Smith"));
    control_elements[2].SetUserHasEditedTheFieldForTest();

    // Find the form that contains the input element.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    EXPECT_EQ(u"TestForm", form.name);
    EXPECT_EQ(GURL("http://abc.com"), form.action);

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(3U, fields.size());

    // Preview the form and verify that the cursor position has been updated.
    form.fields[0].value = u"1111-2222-3333-4444";
    form.fields[1].value = u"03/2030";
    form.fields[2].value = u"Susan Smith";
    form.fields[0].is_autofilled = true;
    form.fields[1].is_autofilled = true;
    form.fields[2].is_autofilled = true;
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kPreview);
    // The selection should be set after the 19th character.
    EXPECT_EQ(19u, input_element.SelectionStart());
    EXPECT_EQ(19u, input_element.SelectionEnd());

    // Fill the form.
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2;
    FormFieldData field2;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form2, &field2));
    EXPECT_EQ(u"TestForm", form2.name);
    EXPECT_EQ(GURL("http://abc.com"), form2.action);

    const std::vector<FormFieldData>& fields2 = form2.fields;
    ASSERT_EQ(3U, fields2.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"cc";
    expected.name = expected.id_attribute;
    expected.value = u"1111-2222-3333-4444";
    if (placeholder_creditcard) {
      expected.label = ASCIIToUTF16(placeholder_creditcard);
      expected.placeholder = ASCIIToUTF16(placeholder_creditcard);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    expected.id_attribute = u"expiration_date";
    expected.name = expected.id_attribute;
    expected.value = u"03/2030";
    if (placeholder_expiration) {
      expected.label = ASCIIToUTF16(placeholder_expiration);
      expected.placeholder = ASCIIToUTF16(placeholder_expiration);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.id_attribute = u"name";
    expected.name = expected.id_attribute;
    expected.value = u"John Smith";
    if (placeholder_name) {
      expected.label = ASCIIToUTF16(placeholder_name);
      expected.placeholder = ASCIIToUTF16(placeholder_name);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(19u, input_element.SelectionStart());
    EXPECT_EQ(19u, input_element.SelectionEnd());
  }

  void TestFillFormJSModifiesUserInputValue(const char* html,
                                            const char* placeholder_creditcard,
                                            const char* placeholder_expiration,
                                            const char* placeholder_name) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Get the input element we want to find.
    WebInputElement input_element = GetInputElementById("cc");
    WebFormElement form_element = input_element.Form();
    std::vector<WebFormControlElement> control_elements =
        ExtractAutofillableElementsInForm(form_element);

    ASSERT_EQ(3U, control_elements.size());
    // We now modify the values.
    // This will be ignored.
    control_elements[0].SetValue(WebString::FromUTF16(u"____-____-____-____"));
    // This will be ignored.
    control_elements[1].SetValue(WebString::FromUTF16(u"____/__"));
    control_elements[2].SetValue(WebString::FromUTF16(u"john smith"));
    control_elements[2].SetUserHasEditedTheFieldForTest();

    // Sometimes the JS modifies the value entered by the user.
    ExecuteJavaScriptForTests(
        "document.getElementById('name').value = 'John Smith';");

    // Find the form that contains the input element.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form, &field));
    EXPECT_EQ(u"TestForm", form.name);
    EXPECT_EQ(GURL("http://abc.com"), form.action);

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(3U, fields.size());

    // Preview the form and verify that the cursor position has been updated.
    form.fields[0].value = u"1111-2222-3333-4444";
    form.fields[1].value = u"03/2030";
    form.fields[2].value = u"Susan Smith";
    form.fields[0].is_autofilled = true;
    form.fields[1].is_autofilled = true;
    form.fields[2].is_autofilled = true;
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kPreview);
    // The selection should be set after the 19th character.
    EXPECT_EQ(19u, input_element.SelectionStart());
    EXPECT_EQ(19u, input_element.SelectionEnd());

    // Fill the form.
    ApplyFormAction(form, input_element, mojom::ActionType::kFill,
                    mojom::ActionPersistence::kFill);

    // Find the newly-filled form that contains the input element.
    FormData form2;
    FormFieldData field2;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(input_element, nullptr,
                                                      /*extract_options=*/{},
                                                      &form2, &field2));
    EXPECT_EQ(u"TestForm", form2.name);
    EXPECT_EQ(GURL("http://abc.com"), form2.action);

    const std::vector<FormFieldData>& fields2 = form2.fields;
    ASSERT_EQ(3U, fields2.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"cc";
    expected.name = expected.id_attribute;
    expected.value = u"1111-2222-3333-4444";
    if (placeholder_creditcard) {
      expected.label = ASCIIToUTF16(placeholder_creditcard);
      expected.placeholder = ASCIIToUTF16(placeholder_creditcard);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

    expected.id_attribute = u"expiration_date";
    expected.name = expected.id_attribute;
    expected.value = u"03/2030";
    if (placeholder_expiration) {
      expected.label = ASCIIToUTF16(placeholder_expiration);
      expected.placeholder = ASCIIToUTF16(placeholder_expiration);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = true;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

    expected.id_attribute = u"name";
    expected.name = expected.id_attribute;
    expected.value = u"John Smith";
    if (placeholder_name) {
      expected.label = ASCIIToUTF16(placeholder_name);
      expected.placeholder = ASCIIToUTF16(placeholder_name);
    } else {
      expected.label.clear();
      expected.placeholder.clear();
    }
    expected.is_autofilled = false;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(19u, input_element.SelectionStart());
    EXPECT_EQ(19u, input_element.SelectionEnd());
  }

  void TestClearSectionWithNode(const char* html, bool unowned) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Set the auto-filled attribute.
    WebInputElement firstname = GetInputElementById("firstname");
    firstname.SetAutofillState(WebAutofillState::kAutofilled);
    WebInputElement lastname = GetInputElementById("lastname");
    lastname.SetAutofillState(WebAutofillState::kAutofilled);
    WebInputElement month = GetInputElementById("month");
    month.SetAutofillState(WebAutofillState::kAutofilled);
    WebFormControlElement textarea = GetFormControlElementById("textarea");
    textarea.SetAutofillState(WebAutofillState::kAutofilled);

    // Set the value of the disabled text input element.
    WebInputElement notenabled = GetInputElementById("notenabled");
    notenabled.SetValue(WebString::FromUTF8("no clear"));

    // Clear the form.
    EXPECT_TRUE(form_cache.ClearSectionWithElement(firstname));

    // Verify that the auto-filled attribute has been turned off.
    EXPECT_FALSE(firstname.IsAutofilled());

    // Verify the form is cleared.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(
        firstname, nullptr, /*extract_options=*/{}, &form, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name);
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(9U, fields.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value.clear();
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value.clear();
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"noAC";
    expected.name = expected.id_attribute;
    expected.value = u"one";
    expected.label = u"one";
    expected.autocomplete_attribute = "off";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
    expected.autocomplete_attribute.clear();

    expected.id_attribute = u"notenabled";
    expected.name = expected.id_attribute;
    expected.value = u"no clear";
    expected.label.clear();
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

    expected.form_control_type = FormControlType::kInputMonth;
    expected.max_length = 0;
    expected.id_attribute = u"month";
    expected.name = expected.id_attribute;
    expected.value.clear();
    expected.label.clear();
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[4]);

    expected.id_attribute = u"month-disabled";
    expected.name = expected.id_attribute;
    expected.value = u"2012-11";
    expected.label = u"2012-11";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[5]);

    expected.form_control_type = FormControlType::kTextArea;
    expected.id_attribute = u"textarea";
    expected.max_length = FormFieldData::kDefaultMaxLength;
    expected.name = expected.id_attribute;
    expected.value.clear();
    expected.label.clear();
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[6]);

    expected.id_attribute = u"textarea-disabled";
    expected.name = expected.id_attribute;
    expected.value = u"    Banana!  ";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[7]);

    expected.id_attribute = u"textarea-noAC";
    expected.name = expected.id_attribute;
    expected.value = u"Carrot?";
    expected.autocomplete_attribute = "off";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[8]);
    expected.autocomplete_attribute.clear();

    // Verify that the cursor position has been updated.
    EXPECT_EQ(0u, firstname.SelectionStart());
    EXPECT_EQ(0u, firstname.SelectionEnd());
  }

  void TestClearTwoSections(const char* html, bool unowned) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Set the autofilled attribute and specify the section attribute.
    WebInputElement firstname_shipping =
        GetInputElementById("firstname-shipping");
    firstname_shipping.SetAutofillValue("John");
    firstname_shipping.SetAutofillState(WebAutofillState::kAutofilled);
    firstname_shipping.SetAutofillSection("shipping");

    WebInputElement lastname_shipping =
        GetInputElementById("lastname-shipping");
    lastname_shipping.SetAutofillValue("Smith");
    lastname_shipping.SetAutofillState(WebAutofillState::kAutofilled);
    lastname_shipping.SetAutofillSection("shipping");

    WebInputElement city_shipping = GetInputElementById("city-shipping");
    city_shipping.SetAutofillValue("Montreal");
    city_shipping.SetAutofillState(WebAutofillState::kAutofilled);
    city_shipping.SetAutofillSection("shipping");

    WebInputElement firstname_billing =
        GetInputElementById("firstname-billing");
    firstname_billing.SetAutofillValue("John");
    firstname_billing.SetAutofillState(WebAutofillState::kAutofilled);
    firstname_billing.SetAutofillSection("billing");

    WebInputElement lastname_billing = GetInputElementById("lastname-billing");
    lastname_billing.SetAutofillValue("Smith");
    lastname_billing.SetAutofillState(WebAutofillState::kAutofilled);
    lastname_billing.SetAutofillSection("billing");

    WebInputElement city_billing = GetInputElementById("city-billing");
    city_billing.SetAutofillValue("Paris");
    city_billing.SetAutofillState(WebAutofillState::kAutofilled);
    city_billing.SetAutofillSection("billing");

    // Clear the first (shipping) section.
    EXPECT_TRUE(form_cache.ClearSectionWithElement(firstname_shipping));

    // Verify that the autofilled attribute is false only for the shipping
    // section.
    EXPECT_FALSE(firstname_shipping.IsAutofilled());
    EXPECT_FALSE(lastname_shipping.IsAutofilled());
    EXPECT_FALSE(city_shipping.IsAutofilled());
    EXPECT_TRUE(firstname_billing.IsAutofilled());
    EXPECT_TRUE(lastname_billing.IsAutofilled());
    EXPECT_TRUE(city_billing.IsAutofilled());

    // Verify that the shipping section is cleared, but not the billing one.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(
        firstname_shipping, nullptr, /*extract_options=*/{}, &form, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name);
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(6U, fields.size());

    FormFieldData expected;
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;

    // shipping section
    expected.is_autofilled = false;
    expected.id_attribute = u"firstname-shipping";
    expected.name = expected.id_attribute;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname-shipping";
    expected.name = expected.id_attribute;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"city-shipping";
    expected.name = expected.id_attribute;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

    // billing section
    expected.is_autofilled = true;
    expected.id_attribute = u"firstname-billing";
    expected.name = expected.id_attribute;
    expected.value = u"John";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

    expected.id_attribute = u"lastname-billing";
    expected.name = expected.id_attribute;
    expected.value = u"Smith";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[4]);

    expected.id_attribute = u"city-billing";
    expected.name = expected.id_attribute;
    expected.value = u"Paris";
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[5]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(0u, firstname_shipping.SelectionStart());
    EXPECT_EQ(0u, firstname_shipping.SelectionEnd());
  }

  void TestClearSectionWithNodeContainingSelectOne(const char* html,
                                                   bool unowned) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Set the auto-filled attribute.
    WebInputElement firstname = GetInputElementById("firstname");
    firstname.SetAutofillState(WebAutofillState::kAutofilled);
    WebInputElement lastname = GetInputElementById("lastname");
    lastname.SetAutofillState(WebAutofillState::kAutofilled);

    // Set the value and auto-filled attribute of the state element.
    WebSelectElement state =
        web_frame->GetDocument().GetElementById("state").To<WebSelectElement>();
    state.SetValue(WebString::FromUTF8("AK"));
    state.SetAutofillState(WebAutofillState::kAutofilled);

    // Clear the form.
    EXPECT_TRUE(form_cache.ClearSectionWithElement(firstname));

    // Verify that the auto-filled attribute has been turned off.
    EXPECT_FALSE(firstname.IsAutofilled());

    // Verify the form is cleared.
    FormData form;
    FormFieldData field;
    EXPECT_TRUE(FindFormAndFieldForFormControlElement(
        firstname, nullptr, /*extract_options=*/{}, &form, &field));
    if (!unowned) {
      EXPECT_EQ(u"TestForm", form.name);
      EXPECT_EQ(GURL("http://abc.com"), form.action);
    }

    const std::vector<FormFieldData>& fields = form.fields;
    ASSERT_EQ(3U, fields.size());

    FormFieldData expected;

    expected.id_attribute = u"firstname";
    expected.name = expected.id_attribute;
    expected.value.clear();
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

    expected.id_attribute = u"lastname";
    expected.name = expected.id_attribute;
    expected.value.clear();
    expected.form_control_type = FormControlType::kInputText;
    expected.max_length = FormFieldData::kDefaultMaxLength;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

    expected.id_attribute = u"state";
    expected.name_attribute = u"state";
    expected.name = expected.name_attribute;
    expected.value = u"?";
    expected.form_control_type = FormControlType::kSelectOne;
    expected.max_length = 0;
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

    // Verify that the cursor position has been updated.
    EXPECT_EQ(0u, firstname.SelectionStart());
    EXPECT_EQ(0u, firstname.SelectionEnd());
  }

  void TestClearPreviewedElements(const char* html) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    std::vector<WebFormControlElement> elements;
    elements.push_back(GetInputElementById("firstname"));
    elements.push_back(GetInputElementById("lastname"));
    elements.push_back(GetInputElementById("email"));
    elements.push_back(GetInputElementById("email2"));
    elements.push_back(GetInputElementById("phone"));
    WebInputElement firstname = elements[0].To<WebInputElement>();
    WebInputElement lastname = elements[1].To<WebInputElement>();

    // Set the auto-filled attribute.
    for (WebFormControlElement& e : elements) {
      e.SetAutofillState(WebAutofillState::kPreviewed);
    }

    // Set the suggested values on two of the elements.
    firstname.SetSuggestedValue(WebString::FromASCII("Wyatt"));
    lastname.SetSuggestedValue(WebString::FromASCII("Earp"));
    elements[2].SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[3].SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[4].SetSuggestedValue(WebString::FromASCII("650-777-9999"));

    std::vector<bool> is_value_empty(elements.size());
    for (size_t i = 0; i < elements.size(); ++i) {
      is_value_empty[i] = elements[i].Value().IsEmpty();
    }

    // Clear the previewed fields.
    ClearPreviewedElements(mojom::ActionType::kFill, elements, lastname,
                           WebAutofillState::kNotFilled);

    // Verify the previewed fields are cleared.
    for (size_t i = 0; i < elements.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "Element " << i);
      EXPECT_EQ(elements[i].Value().IsEmpty(), is_value_empty[i]);
      EXPECT_TRUE(elements[i].SuggestedValue().IsEmpty());
      EXPECT_FALSE(elements[i].IsAutofilled());
    }

    // Verify that the cursor position has been updated.
    EXPECT_EQ(0u, lastname.SelectionStart());
    EXPECT_EQ(0u, lastname.SelectionEnd());
  }

  void TestClearPreviewedFormWithNonEmptyInitiatingNode(const char* html) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    std::vector<WebFormControlElement> elements;
    elements.push_back(GetInputElementById("firstname"));
    elements.push_back(GetInputElementById("lastname"));
    elements.push_back(GetInputElementById("email"));
    elements.push_back(GetInputElementById("email2"));
    elements.push_back(GetInputElementById("phone"));
    WebInputElement firstname = elements[0].To<WebInputElement>();
    WebInputElement lastname = elements[1].To<WebInputElement>();

    // Set the auto-filled attribute.
    for (WebFormControlElement& e : elements) {
      e.SetAutofillState(WebAutofillState::kPreviewed);
    }

    // Set the suggested values on all of the elements.
    firstname.SetSuggestedValue(WebString::FromASCII("Wyatt"));
    lastname.SetSuggestedValue(WebString::FromASCII("Earp"));
    elements[2].SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[3].SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[4].SetSuggestedValue(WebString::FromASCII("650-777-9999"));

    // Clear the previewed fields.
    ClearPreviewedElements(mojom::ActionType::kFill, elements, firstname,
                           WebAutofillState::kNotFilled);

    // Fields with non-empty values are restored.
    EXPECT_EQ(u"W", firstname.Value().Utf16());
    EXPECT_TRUE(firstname.SuggestedValue().IsEmpty());
    EXPECT_FALSE(firstname.IsAutofilled());
    EXPECT_EQ(1u, firstname.SelectionStart());
    EXPECT_EQ(1u, firstname.SelectionEnd());

    // Verify the previewed fields are cleared.
    for (size_t i = 1; i < elements.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "Element " << i);
      EXPECT_TRUE(elements[i].Value().IsEmpty());
      EXPECT_TRUE(elements[i].SuggestedValue().IsEmpty());
      EXPECT_FALSE(elements[i].IsAutofilled());
    }
  }

  void TestClearPreviewedFormWithAutofilledInitiatingNode(const char* html) {
    LoadHTML(html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    std::vector<WebFormControlElement> elements;
    elements.push_back(GetInputElementById("firstname"));
    elements.push_back(GetInputElementById("lastname"));
    elements.push_back(GetInputElementById("email"));
    elements.push_back(GetInputElementById("email2"));
    elements.push_back(GetInputElementById("phone"));
    WebInputElement firstname = elements[0].To<WebInputElement>();
    WebInputElement lastname = elements[1].To<WebInputElement>();

    // Set the auto-filled attribute.
    for (WebFormControlElement& e : elements) {
      e.SetAutofillState(WebAutofillState::kPreviewed);
    }

    // Set the suggested values on all of the elements.
    firstname.SetSuggestedValue(WebString::FromASCII("Wyatt"));
    lastname.SetSuggestedValue(WebString::FromASCII("Earp"));
    elements[2].SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[3].SetSuggestedValue(WebString::FromASCII("wyatt@earp.com"));
    elements[4].SetSuggestedValue(WebString::FromASCII("650-777-9999"));

    // Clear the previewed fields.
    ClearPreviewedElements(mojom::ActionType::kFill, elements, firstname,
                           WebAutofillState::kAutofilled);

    // Fields with non-empty values are restored.
    EXPECT_EQ(u"W", firstname.Value().Utf16());
    EXPECT_TRUE(firstname.SuggestedValue().IsEmpty());
    EXPECT_TRUE(firstname.IsAutofilled());
    EXPECT_EQ(1u, firstname.SelectionStart());
    EXPECT_EQ(1u, firstname.SelectionEnd());

    // Verify the previewed fields are cleared.
    for (size_t i = 1; i < elements.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "Element " << i);
      EXPECT_TRUE(elements[i].Value().IsEmpty());
      EXPECT_TRUE(elements[i].SuggestedValue().IsEmpty());
      EXPECT_FALSE(elements[i].IsAutofilled());
    }
  }

  void TestClearOnlyAutofilledFields(const char* html) {
    LoadHTML(html);

    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    ASSERT_EQ(1U, forms.size());

    // Set the autofilled attribute.
    WebInputElement firstname = GetInputElementById("firstname");
    firstname.SetAutofillState(WebAutofillState::kNotFilled);
    WebInputElement lastname = GetInputElementById("lastname");
    lastname.SetAutofillState(WebAutofillState::kAutofilled);
    WebInputElement email = GetInputElementById("email");
    email.SetAutofillState(WebAutofillState::kAutofilled);
    WebInputElement phone = GetInputElementById("phone");
    phone.SetAutofillState(WebAutofillState::kAutofilled);

    // Clear the fields.
    EXPECT_TRUE(form_cache.ClearSectionWithElement(firstname));

    // Verify only autofilled fields are cleared.
    EXPECT_EQ(u"Wyatt", firstname.Value().Utf16());
    EXPECT_TRUE(firstname.SuggestedValue().IsEmpty());
    EXPECT_FALSE(firstname.IsAutofilled());
    EXPECT_TRUE(lastname.Value().IsEmpty());
    EXPECT_TRUE(lastname.SuggestedValue().IsEmpty());
    EXPECT_FALSE(lastname.IsAutofilled());
    EXPECT_TRUE(email.Value().IsEmpty());
    EXPECT_TRUE(email.SuggestedValue().IsEmpty());
    EXPECT_FALSE(email.IsAutofilled());
    EXPECT_TRUE(phone.Value().IsEmpty());
    EXPECT_TRUE(phone.SuggestedValue().IsEmpty());
    EXPECT_FALSE(phone.IsAutofilled());
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
};

// We should be able to extract a normal text field.
TEST_F(FormAutofillTest, WebFormControlElementToFormField) {
  LoadHTML("<INPUT type='text' id='element' value='value'/>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  element.SetSelectionRange(1, 4);

  FormFieldData result1;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   /*extract_options=*/{}, &result1);

  FormFieldData expected;
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;

  expected.id_attribute = u"element";
  expected.name = expected.id_attribute;

  expected.value.clear();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result1);
  EXPECT_EQ(0u, result1.selection_start);
  EXPECT_EQ(0u, result1.selection_end);

  FormFieldData result2;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result2);

  expected.value = u"value";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result2);
  EXPECT_EQ(1u, result2.selection_start);
  EXPECT_EQ(4u, result2.selection_end);
  EXPECT_EQ(u"alu", result2.GetSelection());
  EXPECT_EQ(u"alu", result2.GetSelectionAsStringView());
}

// We should be able to extract a text field with autocomplete="off".
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutocompleteOff) {
  LoadHTML("<INPUT type='text' id='element' value='value'"
           "       autocomplete='off'/>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result);

  FormFieldData expected;
  expected.id_attribute = u"element";
  expected.name = expected.id_attribute;
  expected.value = u"value";
  expected.form_control_type = FormControlType::kInputText;
  expected.autocomplete_attribute = "off";
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a text field with maxlength specified.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldMaxLength) {
  LoadHTML("<INPUT type='text' id='element' value='value'"
           "       maxlength='5'/>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result);

  FormFieldData expected;
  expected.id_attribute = u"element";
  expected.name = expected.id_attribute;
  expected.value = u"value";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = 5;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a text field that has been autofilled.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutofilled) {
  LoadHTML("<INPUT type='text' id='element' value='value'/>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebInputElement element = GetInputElementById("element");
  element.SetAutofillState(WebAutofillState::kAutofilled);
  FormFieldData result;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result);

  FormFieldData expected;
  expected.id_attribute = u"element";
  expected.name = expected.id_attribute;
  expected.value = u"value";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  expected.is_autofilled = true;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a radio or a checkbox field that has been
// autofilled.
TEST_F(FormAutofillTest, WebFormControlElementToClickableFormField) {
  LoadHTML("<INPUT type='checkbox' id='checkbox' value='mail' checked/>"
           "<INPUT type='radio' id='radio' value='male'/>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebInputElement element = GetInputElementById("checkbox");
  element.SetAutofillState(WebAutofillState::kAutofilled);
  FormFieldData result;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result);

  FormFieldData expected;
  expected.id_attribute = u"checkbox";
  expected.name = expected.id_attribute;
  expected.value = u"mail";
  expected.form_control_type = FormControlType::kInputCheckbox;
  expected.max_length = 0;
  expected.is_autofilled = true;
  expected.check_status = FormFieldData::CheckStatus::kChecked;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);

  element = GetInputElementById("radio");
  element.SetAutofillState(WebAutofillState::kAutofilled);
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  expected.id_attribute = u"radio";
  expected.name = expected.id_attribute;
  expected.value = u"male";
  expected.form_control_type = FormControlType::kInputRadio;
  expected.max_length = 0;
  expected.is_autofilled = true;
  expected.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract a <select> field.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldSelect) {
  LoadHTML("<SELECT id='element'/>"
           "  <OPTION value='CA'>California</OPTION>"
           "  <OPTION value='TX'>Texas</OPTION>"
           "</SELECT>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result1;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result1);

  FormFieldData expected;
  expected.id_attribute = u"element";
  expected.name = expected.id_attribute;
  expected.max_length = 0;
  expected.form_control_type = FormControlType::kSelectOne;

  expected.value = u"CA";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result1);

  FormFieldData result2;
  WebFormControlElementToFormField(
      WebFormElement(), element, nullptr,
      {ExtractOption::kValue, ExtractOption::kOptionText}, &result2);
  expected.value = u"California";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result2);

  FormFieldData result3;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kOptions}, &result3);
  expected.value.clear();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result3);

  ASSERT_EQ(2U, result3.options.size());
  EXPECT_EQ(u"CA", result3.options[0].value);
  EXPECT_EQ(u"California", result3.options[0].content);
  EXPECT_EQ(u"TX", result3.options[1].value);
  EXPECT_EQ(u"Texas", result3.options[1].content);
}

// We copy extra attributes for the select field.
TEST_F(FormAutofillTest,
       WebFormControlElementToFormFieldSelect_ExtraAttributes) {
  LoadHTML("<SELECT id='element' autocomplete='off'/>"
           "  <OPTION value='CA'>California</OPTION>"
           "  <OPTION value='TX'>Texas</OPTION>"
           "</SELECT>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  element.SetAutofillState(WebAutofillState::kAutofilled);

  FormFieldData result1;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result1);

  FormFieldData expected;
  expected.id_attribute = u"element";
  expected.name = expected.id_attribute;
  expected.max_length = 0;
  expected.form_control_type = FormControlType::kSelectOne;
  // We check that the extra attributes have been copied to |result1|.
  expected.is_autofilled = true;
  expected.autocomplete_attribute = "off";
  expected.should_autocomplete = false;
  expected.is_focusable = true;
  expected.is_visible = true;
  expected.text_direction = base::i18n::LEFT_TO_RIGHT;

  expected.value = u"CA";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result1);
}

// When faced with <select> field with *many* options, we should trim them to a
// reasonable number.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldLongSelect) {
  std::string html = "<SELECT id='element'/>";
  for (size_t i = 0; i < 2 * kMaxListSize; ++i) {
    html += base::StringPrintf("<OPTION value='%" PRIuS "'>"
                               "%" PRIuS "</OPTION>", i, i);
  }
  html += "</SELECT>";
  LoadHTML(html.c_str());

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_TRUE(frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kOptions}, &result);

  EXPECT_TRUE(result.options.empty());
}

// Test that we use the aria-label as the content if the <option> has no text.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldSelectListAriaLabel) {
  LoadHTML(
      "<SELECTLIST id='element'>"
      "<OPTION aria-label='usa'><img/></OPTION>"
      "<OPTION aria-label='uk'><img/></OPTION>"
      "</SELECTLIST>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);
  WebFormControlElement element = GetFormControlElementById("element");

  FormFieldData result;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kOptions}, &result);
  ASSERT_EQ(2u, result.options.size());
  EXPECT_EQ(u"usa", result.options[0].content);
  EXPECT_EQ(u"uk", result.options[1].content);
}

// Test that the content for the <option> can be computed when the <option>s
// have nested HTML nodes.
TEST_F(FormAutofillTest,
       WebFormControlElementToFormFieldSelectListNestedNodes) {
  LoadHTML(
      "<SELECTLIST id='element'>"
      "<OPTION><div><img/><b>+1</b> (Canada)</div></OPTION>"
      "</SELECTLIST>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);
  WebFormControlElement element = GetFormControlElementById("element");

  FormFieldData result;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kOptions}, &result);
  ASSERT_EQ(1u, result.options.size());
  EXPECT_EQ(u"+1 (Canada)", result.options[0].content);
}

// We should be able to extract a <textarea> field.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldTextArea) {
  LoadHTML("<TEXTAREA id='element'>"
             "This element's value&#10;"
             "spans multiple lines."
           "</TEXTAREA>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result_sans_value;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   /*extract_options=*/{}, &result_sans_value);

  FormFieldData expected;
  expected.id_attribute = u"element";
  expected.name = expected.id_attribute;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  expected.form_control_type = FormControlType::kTextArea;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result_sans_value);

  FormFieldData result_with_value;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result_with_value);
  expected.value =
      u"This element's value\n"
      u"spans multiple lines.";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result_with_value);
}

// We should be able to extract an <input type="month"> field.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldMonthInput) {
  LoadHTML("<INPUT type='month' id='element' value='2011-12'>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result_sans_value;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   /*extract_options=*/{}, &result_sans_value);

  FormFieldData expected;
  expected.id_attribute = u"element";
  expected.name = expected.id_attribute;
  expected.max_length = 0;
  expected.form_control_type = FormControlType::kInputMonth;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result_sans_value);

  FormFieldData result_with_value;
  WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                   {ExtractOption::kValue}, &result_with_value);
  expected.value = u"2011-12";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result_with_value);
}

// We should be able to extract password fields.
TEST_F(FormAutofillTest, WebFormControlElementToPasswordFormField) {
  LoadHTML("<FORM name='TestForm' action='http://cnn.com' method='post'>"
           "  <INPUT type='password' id='password' value='secret'/>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("password");
  FormFieldData result;
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);

  FormFieldData expected;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  expected.id_attribute = u"password";
  expected.name = expected.id_attribute;
  expected.form_control_type = FormControlType::kInputPassword;
  expected.value = u"secret";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
}

// We should be able to extract the autocompletetype attribute.
TEST_F(FormAutofillTest, WebFormControlElementToFormFieldAutocompletetype) {
  std::string html =
      "<INPUT type='text' id='absent'/>"
      "<INPUT type='text' id='empty' autocomplete=''/>"
      "<INPUT type='text' id='off' autocomplete='off'/>"
      "<INPUT type='text' id='regular' autocomplete='email'/>"
      "<INPUT type='text' id='multi-valued' "
      "       autocomplete='billing email'/>"
      "<INPUT type='text' id='experimental' x-autocompletetype='email'/>"
      "<INPUT type='month' id='month' autocomplete='cc-exp'/>"
      "<SELECT id='select' autocomplete='state'/>"
      "  <OPTION value='CA'>California</OPTION>"
      "  <OPTION value='TX'>Texas</OPTION>"
      "</SELECT>"
      "<TEXTAREA id='textarea' autocomplete='street-address'>"
      "  Some multi-"
      "  lined value"
      "</TEXTAREA>";
  html +=
      "<INPUT type='text' id='malicious' autocomplete='" +
      std::string(10000, 'x') + "'/>";
  LoadHTML(html.c_str());

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  struct TestCase {
    const std::string element_id;
    FormControlType form_control_type;
    const std::string autocomplete_attribute;
  };
  TestCase test_cases[] = {
      // An absent attribute is equivalent to an empty one.
      {"absent", FormControlType::kInputText, ""},
      // Make sure there are no issues parsing an empty attribute.
      {"empty", FormControlType::kInputText, ""},
      // Make sure there are no issues parsing an attribute value that isn't a
      // type hint.
      {"off", FormControlType::kInputText, "off"},
      // Common case: exactly one type specified.
      {"regular", FormControlType::kInputText, "email"},
      // Verify that we correctly extract multiple tokens as well.
      {"multi-valued", FormControlType::kInputText, "billing email"},
      // Verify that <input type="month"> fields are supported.
      {"month", FormControlType::kInputMonth, "cc-exp"},
      // We previously extracted this data from the experimental
      // 'x-autocompletetype' attribute.  Now that the field type hints are part
      // of the spec under the autocomplete attribute, we no longer support the
      // experimental version.
      {"experimental", FormControlType::kInputText, ""},
      // <select> elements should behave no differently from text fields here.
      {"select", FormControlType::kSelectOne, "state"},
      // <textarea> elements should also behave no differently from text fields.
      {"textarea", FormControlType::kTextArea, "street-address"},
      // Very long attribute values should be replaced by a default string, to
      // prevent malicious websites from DOSing the browser process.
      {"malicious", FormControlType::kInputText, "x-max-data-length-exceeded"},
  };

  WebDocument document = frame->GetDocument();
  for (auto& test_case : test_cases) {
    WebFormControlElement element =
        GetFormControlElementById(WebString::FromASCII(test_case.element_id));
    FormFieldData result;
    WebFormControlElementToFormField(WebFormElement(), element, nullptr,
                                     /*extract_options=*/{}, &result);

    FormFieldData expected;
    expected.id_attribute = ASCIIToUTF16(test_case.element_id);
    expected.name = expected.id_attribute;
    expected.form_control_type = test_case.form_control_type;
    expected.max_length =
        (test_case.form_control_type == FormControlType::kInputText ||
         test_case.form_control_type == FormControlType::kTextArea)
            ? FormFieldData::kDefaultMaxLength
            : 0;
    expected.autocomplete_attribute = test_case.autocomplete_attribute;
    expected.parsed_autocomplete =
        ParseAutocompleteAttribute(test_case.autocomplete_attribute);

    SCOPED_TRACE(test_case.element_id);
    EXPECT_FORM_FIELD_DATA_EQUALS(expected, result);
  }
}

TEST_F(FormAutofillTest, DetectTextDirectionFromDirectStyle) {
  LoadHTML("<STYLE>input{direction:rtl}</STYLE>"
           "<FORM>"
           "  <INPUT type='text' id='element'>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction);
}

TEST_F(FormAutofillTest, DetectTextDirectionFromDirectDIRAttribute) {
  LoadHTML("<FORM>"
           "  <INPUT dir='rtl' type='text' id='element'/>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction);
}

TEST_F(FormAutofillTest, DetectTextDirectionFromParentStyle) {
  LoadHTML("<STYLE>form{direction:rtl}</STYLE>"
           "<FORM>"
           "  <INPUT type='text' id='element'/>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction);
}

TEST_F(FormAutofillTest, DetectTextDirectionFromParentDIRAttribute) {
  LoadHTML("<FORM dir='rtl'>"
           "  <INPUT type='text' id='element'/>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction);
}

TEST_F(FormAutofillTest, DetectTextDirectionWhenStyleAndDIRAttributeMixed) {
  LoadHTML("<STYLE>input{direction:ltr}</STYLE>"
           "<FORM dir='rtl'>"
           "  <INPUT type='text' id='element'/>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, result.text_direction);
}

TEST_F(FormAutofillTest, TextAlignOverridesDirection) {
  // text-align: right
  LoadHTML("<STYLE>input{direction:ltr;text-align:right}</STYLE>"
           "<FORM>"
           "  <INPUT type='text' id='element'/>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction);

  // text-align: left
  LoadHTML("<STYLE>input{direction:rtl;text-align:left}</STYLE>"
           "<FORM>"
           "  <INPUT type='text' id='element'/>"
           "</FORM>");

  frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  element = GetFormControlElementById("element");
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, result.text_direction);
}

TEST_F(FormAutofillTest,
       DetectTextDirectionWhenParentHasBothDIRAttributeAndStyle) {
  LoadHTML("<STYLE>form{direction:ltr}</STYLE>"
           "<FORM dir='rtl'>"
           "  <INPUT type='text' id='element'/>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, result.text_direction);
}

TEST_F(FormAutofillTest, DetectTextDirectionWhenAncestorHasInlineStyle) {
  LoadHTML("<FORM style='direction:ltr'>"
           "  <SPAN dir='rtl'>"
           "    <INPUT type='text' id='element'/>"
           "  </SPAN>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  WebFormControlElementToFormField(element.Form(), element, nullptr,
                                   {ExtractOption::kValue}, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction);
}

TEST_F(FormAutofillTest, WebFormElementToFormData) {
  LoadHTML(
      "<FORM name='TestForm' action='http://cnn.com/submit/?a=1' method='post'>"
      "  <LABEL for='firstname'>First name:</LABEL>"
      "    <INPUT type='text' id='firstname' value='John'/>"
      "  <LABEL for='lastname'>Last name:</LABEL>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  <LABEL for='street-address'>Address:</LABEL>"
      "    <TEXTAREA id='street-address'>"
      "123 Fantasy Ln.&#10;"
      "Apt. 42"
      "</TEXTAREA>"
      "  <LABEL for='state'>State:</LABEL>"
      "    <SELECT id='state'/>"
      "      <OPTION value='CA'>California</OPTION>"
      "      <OPTION value='TX'>Texas</OPTION>"
      "    </SELECT>"
      "  <LABEL for='password'>Password:</LABEL>"
      "    <INPUT type='password' id='password' value='secret'/>"
      "  <LABEL for='month'>Card expiration:</LABEL>"
      "    <INPUT type='month' id='month' value='2011-12'/>"
      "    <INPUT type='submit' name='reply-send' value='Send'/>"
      // The below inputs should be ignored
      "  <LABEL for='notvisible'>Hidden:</LABEL>"
      "    <INPUT type='hidden' id='notvisible' value='apple'/>"
      "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebVector<WebFormElement> forms = frame->GetDocument().Forms();
  ASSERT_EQ(1U, forms.size());

  WebInputElement input_element = GetInputElementById("firstname");

  FormData form;
  FormFieldData field;
  EXPECT_TRUE(WebFormElementToFormData(forms[0], input_element, nullptr,
                                       {ExtractOption::kValue}, &form, &field));
  EXPECT_EQ(u"TestForm", form.name);
  EXPECT_EQ(GetFormRendererId(forms[0]), form.unique_renderer_id);
  EXPECT_EQ(GURL("http://cnn.com/submit/"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(6U, fields.size());

  FormFieldData expected;
  expected.id_attribute = u"firstname";
  expected.name = expected.id_attribute;
  expected.value = u"John";
  expected.label = u"First name:";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.id_attribute = u"lastname";
  expected.name = expected.id_attribute;
  expected.value = u"Smith";
  expected.label = u"Last name:";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.id_attribute = u"street-address";
  expected.name = expected.id_attribute;
  expected.value = u"123 Fantasy Ln.\nApt. 42";
  expected.label = u"Address:";
  expected.form_control_type = FormControlType::kTextArea;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.id_attribute = u"state";
  expected.name = expected.id_attribute;
  expected.value = u"CA";
  expected.label = u"State:";
  expected.form_control_type = FormControlType::kSelectOne;
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  expected.id_attribute = u"password";
  expected.name = expected.id_attribute;
  expected.value = u"secret";
  expected.label = u"Password:";
  expected.form_control_type = FormControlType::kInputPassword;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[4]);

  expected.id_attribute = u"month";
  expected.name = expected.id_attribute;
  expected.value = u"2011-12";
  expected.label = u"Card expiration:";
  expected.form_control_type = FormControlType::kInputMonth;
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[5]);

  // Check unique_renderer_id.
  WebVector<WebFormControlElement> form_control_elements =
      forms[0].GetFormControlElements();
  for (size_t i = 0; i < fields.size(); ++i)
    EXPECT_EQ(GetFieldRendererId(form_control_elements[i]),
              fields[i].unique_renderer_id);
}

TEST_F(FormAutofillTest, WebFormElementConsiderNonControlLabelableElements) {
  LoadHTML("<form id=form>"
           "  <label for='progress'>Progress:</label>"
           "  <progress id='progress'></progress>"
           "  <label for='firstname'>First name:</label>"
           "  <input type='text' id='firstname' value='John'>"
           "</form>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_FALSE(web_form.IsNull());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(web_form, WebFormControlElement(),
                                       nullptr, /*extract_options=*/{}, &form,
                                       nullptr));

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(1U, fields.size());
  EXPECT_EQ(u"firstname", fields[0].name);
}

// TODO(crbug.com/616730) Observe flakiness (if so, investigate and/or disable
// test if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)).
// We should not be able to serialize a form with too many fillable fields.
TEST_F(FormAutofillTest, WebFormElementToFormDataTooManyFields) {
  std::string html =
      "<FORM name='TestForm' action='http://cnn.com' method='post'>";
  for (size_t i = 0; i < (kMaxExtractableFields + 1); ++i) {
    html += "<INPUT type='text'/>";
  }
  html += "</FORM>";
  LoadHTML(html.c_str());

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebVector<WebFormElement> forms = frame->GetDocument().Forms();
  ASSERT_EQ(1U, forms.size());

  WebInputElement input_element = GetInputElementById("firstname");

  FormData form;
  FormFieldData field;
  EXPECT_FALSE(WebFormElementToFormData(forms[0], input_element, nullptr,
                                        {ExtractOption::kValue}, &form,
                                        &field));
}

// Tests that the |should_autocomplete| is set to false for all the fields when
// an autocomplete='off' attribute is set for the form in HTML.
TEST_F(FormAutofillTest, WebFormElementToFormData_AutocompleteOff_OnForm) {
  LoadHTML(
      "<FORM name='TestForm' id='form' action='http://cnn.com' method='post' "
      "autocomplete='off'>"
      "  <LABEL for='firstname'>First name:</LABEL>"
      "    <INPUT type='text' id='firstname' value='John'/>"
      "  <LABEL for='lastname'>Last name:</LABEL>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  <LABEL for='street-address'>Address:</LABEL>"
      "    <INPUT type='text' id='addressline1' value='123 Test st.'/>"
      "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_FALSE(web_form.IsNull());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(web_form, WebFormControlElement(),
                                       nullptr, /*extract_options=*/{}, &form,
                                       nullptr));

  for (const FormFieldData& field : form.fields) {
    EXPECT_FALSE(field.should_autocomplete);
  }
}

// Tests that the |should_autocomplete| is set to false only for the field
// which has an autocomplete='off' attribute set for it in HTML.
TEST_F(FormAutofillTest, WebFormElementToFormData_AutocompleteOff_OnField) {
  LoadHTML(
      "<FORM name='TestForm' id='form' action='http://cnn.com' method='post'>"
      "  <LABEL for='firstname'>First name:</LABEL>"
      "    <INPUT type='text' id='firstname' value='John' autocomplete='off'/>"
      "  <LABEL for='lastname'>Last name:</LABEL>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  <LABEL for='street-address'>Address:</LABEL>"
      "    <INPUT type='text' id='addressline1' value='123 Test st.'/>"
      "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_FALSE(web_form.IsNull());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(web_form, WebFormControlElement(),
                                       nullptr, /*extract_options=*/{}, &form,
                                       nullptr));

  ASSERT_EQ(3U, form.fields.size());

  EXPECT_FALSE(form.fields[0].should_autocomplete);
  EXPECT_TRUE(form.fields[1].should_autocomplete);
  EXPECT_TRUE(form.fields[2].should_autocomplete);
}

// |should_autocomplete| must be set to false for the field with
// autocomplete='one-time-code' attribute set in HTML.
TEST_F(FormAutofillTest, WebFormElementToFormData_AutocompleteOff_OneTimeCode) {
  LoadHTML(
      "<FORM name='TestForm' id='form' action='http://cnn.com' method='post'>"
      "  <INPUT type='text' value='123' autocomplete='one-time-code'/>"
      "</FORM>");
  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_FALSE(web_form.IsNull());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(web_form, WebFormControlElement(),
                                       /*field_data_manager=*/nullptr,
                                       /*extract_options=*/{}, &form,
                                       /*field=*/nullptr));

  ASSERT_EQ(1U, form.fields.size());
  EXPECT_FALSE(form.fields[0].should_autocomplete);
}

// Tests CSS classes are set.
TEST_F(FormAutofillTest, WebFormElementToFormData_CssClasses) {
  LoadHTML(
      "<FORM name='TestForm' id='form' action='http://cnn.com' method='post' "
      "autocomplete='off'>"
      "    <INPUT type='text' id='firstname' class='firstname_field' />"
      "    <INPUT type='text' id='lastname' class='lastname_field' />"
      "    <INPUT type='text' id='addressline1'  />"
      "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_FALSE(web_form.IsNull());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(web_form, WebFormControlElement(),
                                       nullptr, /*extract_options=*/{}, &form,
                                       nullptr));

  EXPECT_EQ(3U, form.fields.size());
  EXPECT_EQ(u"firstname_field", form.fields[0].css_classes);
  EXPECT_EQ(u"lastname_field", form.fields[1].css_classes);
  EXPECT_EQ(std::u16string(), form.fields[2].css_classes);
}

// Tests id attributes are set.
TEST_F(FormAutofillTest, WebFormElementToFormData_IdAttributes) {
  LoadHTML(
      "<FORM name='TestForm' id='form' action='http://cnn.com' method='post' "
      "autocomplete='off'>"
      "    <INPUT type='text' name='name1' id='firstname' />"
      "    <INPUT type='text' name='name2' id='lastname' />"
      "    <INPUT type='text' name='same' id='same' />"
      "    <INPUT type='text' id='addressline1' />"
      "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_FALSE(web_form.IsNull());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(web_form, WebFormControlElement(),
                                       nullptr, /*extract_options=*/{}, &form,
                                       nullptr));

  EXPECT_EQ(4U, form.fields.size());

  // id attributes.
  EXPECT_EQ(u"firstname", form.fields[0].id_attribute);
  EXPECT_EQ(u"lastname", form.fields[1].id_attribute);
  EXPECT_EQ(u"same", form.fields[2].id_attribute);
  EXPECT_EQ(u"addressline1", form.fields[3].id_attribute);

  // name attributes.
  EXPECT_EQ(u"name1", form.fields[0].name_attribute);
  EXPECT_EQ(u"name2", form.fields[1].name_attribute);
  EXPECT_EQ(u"same", form.fields[2].name_attribute);
  EXPECT_EQ(u"", form.fields[3].name_attribute);

  // name for autofill
  EXPECT_EQ(u"name1", form.fields[0].name);
  EXPECT_EQ(u"name2", form.fields[1].name);
  EXPECT_EQ(u"same", form.fields[2].name);
  EXPECT_EQ(u"addressline1", form.fields[3].name);
}

TEST_F(FormAutofillTest, ExtractForms) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  First name: <INPUT type='text' id='firstname' value='John'/>"
      "  Last name: <INPUT type='text' id='lastname' value='Smith'/>"
      "  Email: <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, ExtractMultipleForms) {
  LoadHTML("<FORM name='TestForm' action='http://cnn.com' method='post'>"
           "  <INPUT type='text' id='firstname' value='John'/>"
           "  <INPUT type='text' id='lastname' value='Smith'/>"
           "  <INPUT type='text' id='email' value='john@example.com'/>"
           "  <INPUT type='submit' name='reply-send' value='Send'/>"
           "</FORM>"
           "<FORM name='TestForm2' action='http://zoo.com' method='post'>"
           "  <INPUT type='text' id='firstname' value='Jack'/>"
           "  <INPUT type='text' id='lastname' value='Adams'/>"
           "  <INPUT type='text' id='email' value='jack@example.com'/>"
           "  <INPUT type='submit' name='reply-send' value='Send'/>"
           "</FORM>");

  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_NE(nullptr, web_frame);

  FormCache form_cache(web_frame);
  std::vector<FormData> forms =
      form_cache.UpdateFormCache(nullptr).updated_forms;
  ASSERT_EQ(2U, forms.size());

  // First form.
  const FormData& form = forms[0];
  EXPECT_EQ(u"TestForm", form.name);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;

  expected.id_attribute = u"firstname";
  expected.name = expected.id_attribute;
  expected.value = u"John";
  expected.label = u"John";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.id_attribute = u"lastname";
  expected.name = expected.id_attribute;
  expected.value = u"Smith";
  expected.label = u"Smith";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.id_attribute = u"email";
  expected.name = expected.id_attribute;
  expected.value = u"john@example.com";
  expected.label = u"john@example.com";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  // Second form.
  const FormData& form2 = forms[1];
  EXPECT_EQ(u"TestForm2", form2.name);
  EXPECT_EQ(GURL("http://zoo.com"), form2.action);

  const std::vector<FormFieldData>& fields2 = form2.fields;
  ASSERT_EQ(3U, fields2.size());

  expected.id_attribute = u"firstname";
  expected.name = expected.id_attribute;
  expected.value = u"Jack";
  expected.label = u"Jack";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.id_attribute = u"lastname";
  expected.name = expected.id_attribute;
  expected.value = u"Adams";
  expected.label = u"Adams";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.id_attribute = u"email";
  expected.name = expected.id_attribute;
  expected.value = u"jack@example.com";
  expected.label = u"jack@example.com";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
}

TEST_F(FormAutofillTest, OnlyExtractNewForms) {
  LoadHTML(
      "<FORM id='testform' action='http://cnn.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='John'/>"
      "  <INPUT type='text' id='lastname' value='Smith'/>"
      "  <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");

  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_NE(nullptr, web_frame);

  FormCache form_cache(web_frame);
  std::vector<FormData> forms =
      form_cache.UpdateFormCache(nullptr).updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Second call should give nothing as there are no new forms.
  forms = form_cache.UpdateFormCache(nullptr).updated_forms;
  ASSERT_TRUE(forms.empty());

  // Append to the current form will re-extract.
  ExecuteJavaScriptForTests(
      "var newInput = document.createElement('input');"
      "newInput.setAttribute('type', 'text');"
      "newInput.setAttribute('id', 'telephone');"
      "newInput.value = '12345';"
      "document.getElementById('testform').appendChild(newInput);");
  base::RunLoop().RunUntilIdle();

  forms = form_cache.UpdateFormCache(nullptr).updated_forms;
  ASSERT_EQ(1U, forms.size());

  const std::vector<FormFieldData>& fields = forms[0].fields;
  ASSERT_EQ(4U, fields.size());

  FormFieldData expected;
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;

  expected.id_attribute = u"firstname";
  expected.name = expected.id_attribute;
  expected.value = u"John";
  expected.label = u"John";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.id_attribute = u"lastname";
  expected.name = expected.id_attribute;
  expected.value = u"Smith";
  expected.label = u"Smith";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.id_attribute = u"email";
  expected.name = expected.id_attribute;
  expected.value = u"john@example.com";
  expected.label = u"john@example.com";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.id_attribute = u"telephone";
  expected.name = expected.id_attribute;
  expected.value = u"12345";
  expected.label.clear();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  forms.clear();

  // Completely new form will also be extracted.
  ExecuteJavaScriptForTests(
      "var newForm=document.createElement('form');"
      "newForm.id='new_testform';"
      "newForm.action='http://google.com';"
      "newForm.method='post';"
      "var newFirstname=document.createElement('input');"
      "newFirstname.setAttribute('type', 'text');"
      "newFirstname.setAttribute('id', 'second_firstname');"
      "newFirstname.value = 'Bob';"
      "var newLastname=document.createElement('input');"
      "newLastname.setAttribute('type', 'text');"
      "newLastname.setAttribute('id', 'second_lastname');"
      "newLastname.value = 'Hope';"
      "var newEmail=document.createElement('input');"
      "newEmail.setAttribute('type', 'text');"
      "newEmail.setAttribute('id', 'second_email');"
      "newEmail.value = 'bobhope@example.com';"
      "newForm.appendChild(newFirstname);"
      "newForm.appendChild(newLastname);"
      "newForm.appendChild(newEmail);"
      "document.body.appendChild(newForm);");
  base::RunLoop().RunUntilIdle();

  web_frame = GetMainFrame();
  forms = form_cache.UpdateFormCache(nullptr).updated_forms;
  ASSERT_EQ(1U, forms.size());

  const std::vector<FormFieldData>& fields2 = forms[0].fields;
  ASSERT_EQ(3U, fields2.size());

  expected.id_attribute = u"second_firstname";
  expected.name = expected.id_attribute;
  expected.value = u"Bob";
  expected.label.clear();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[0]);

  expected.id_attribute = u"second_lastname";
  expected.name = expected.id_attribute;
  expected.value = u"Hope";
  expected.label.clear();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[1]);

  expected.id_attribute = u"second_email";
  expected.name = expected.id_attribute;
  expected.value = u"bobhope@example.com";
  expected.label.clear();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields2[2]);
}

// We should not report additional forms for empty forms.
TEST_F(FormAutofillTest, ExtractFormsNoFields) {
  LoadHTML("<FORM name='TestForm' action='http://cnn.com' method='post'>"
           "</FORM>");

  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_NE(nullptr, web_frame);

  FormCache form_cache(web_frame);
  std::vector<FormData> forms =
      form_cache.UpdateFormCache(nullptr).updated_forms;
  ASSERT_TRUE(forms.empty());
}

TEST_F(FormAutofillTest, WebFormElementToFormDataAutocomplete) {
  {
    // Form is still Autofill-able despite autocomplete=off.
    LoadHTML("<FORM name='TestForm' action='http://cnn.com' method='post'"
             " autocomplete=off>"
             "  <INPUT type='text' id='firstname' value='John'/>"
             "  <INPUT type='text' id='lastname' value='Smith'/>"
             "  <INPUT type='text' id='email' value='john@example.com'/>"
             "  <INPUT type='submit' name='reply-send' value='Send'/>"
             "</FORM>");

    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    WebVector<WebFormElement> web_forms = web_frame->GetDocument().Forms();
    ASSERT_EQ(1U, web_forms.size());
    WebFormElement web_form = web_forms[0];

    FormData form;
    EXPECT_TRUE(WebFormElementToFormData(web_form, WebFormControlElement(),
                                         nullptr, /*extract_options=*/{}, &form,
                                         nullptr));
  }
}

TEST_F(FormAutofillTest, FindFormForInputElement) {
  TestFindFormForInputElement(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='John'/>"
      "  <INPUT type='text' id='lastname' value='Smith'/>"
      "  <INPUT type='text' id='email' value='john@example.com'"
      "autocomplete='off' />"
      "  <INPUT type='text' id='phone' value='1.800.555.1234'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>",
      false);
}

TEST_F(FormAutofillTest, FindFormForInputElementForUnownedForm) {
    TestFindFormForInputElement(
        "<HEAD><TITLE>delivery recipient</TITLE></HEAD>"
        "<INPUT type='text' id='firstname' value='John'/>"
        "<INPUT type='text' id='lastname' value='Smith'/>"
        "<INPUT type='text' id='email' value='john@example.com'"
        "autocomplete='off' />"
        "<INPUT type='text' id='phone' value='1.800.555.1234'/>"
        "<INPUT type='submit' name='reply-send' value='Send'/>",
        true);
}

TEST_F(FormAutofillTest, FindFormForTextAreaElement) {
  TestFindFormForTextAreaElement(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='John'/>"
      "  <INPUT type='text' id='lastname' value='Smith'/>"
      "  <INPUT type='text' id='email' value='john@example.com'"
      "autocomplete='off' />"
      "  <TEXTAREA id='street-address'>"
      "123 Fantasy Ln.&#10;"
      "Apt. 42"
      "</TEXTAREA>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>",
      false);
}

TEST_F(FormAutofillTest, FindFormForTextAreaElementForUnownedForm) {
  TestFindFormForTextAreaElement(
      "<HEAD><TITLE>delivery address</TITLE></HEAD>"
      "<INPUT type='text' id='firstname' value='John'/>"
      "<INPUT type='text' id='lastname' value='Smith'/>"
      "<INPUT type='text' id='email' value='john@example.com'"
      "autocomplete='off' />"
      "<TEXTAREA id='street-address'>"
      "123 Fantasy Ln.&#10;"
      "Apt. 42"
      "</TEXTAREA>"
      "<INPUT type='submit' name='reply-send' value='Send'/>",
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
  std::string html("<HEAD><TITLE>accented latin: \xC3\xA0, thai: \xE0\xB8\x81, "
      "control: \x04, nbsp: \xEF\xBB\xBF, non-BMP: \xF0\x9F\x8C\x80; This "
      "should match a CHECKOUT flow despite the non-ASCII chars"
      "</TITLE></HEAD>");
  html.append(kUnownedUntitledFormHtml);
  TestFillForm(html.c_str(), true, nullptr);
}

TEST_F(FormAutofillTest, PreviewForm) {
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  <LABEL for='firstname'> First name: </LABEL>"
      "    <INPUT type='text' id='firstname' value='John'/>"
      "  <LABEL for='lastname'> Last name: </LABEL>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  <LABEL for='email'> Email: </LABEL>"
      "    <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
}

// <label for=fieldId> elements are correctly assigned to their inputs. Multiple
// labels are separated with a space.
// TODO(crbug.com/1339277): Simplify the test using `ExpectLabels()`. This
// requires some refactoring of the fixture, as only owned forms are supported
// at the moment.
TEST_F(FormAutofillTest, LabelForAttribute) {
  LoadHTML(R"(
    <label for=fieldId>foo</label>
    <label for=fieldId>bar</label>
    <input id=fieldId>
  )");
  ASSERT_NE(GetMainFrame(), nullptr);

  base::HistogramTester histogram_tester;
  FormData form;
  // Simulate seeing an unowned form containing just the input "fieldID".
  UnownedFormElementsToFormData({GetFormControlElementById("fieldId")}, {},
                                nullptr, GetMainFrame()->GetDocument(), nullptr,
                                /*extract_options=*/{}, &form, nullptr);
  ASSERT_EQ(form.fields.size(), 1u);
  FormFieldData& form_field_data = form.fields[0];

  EXPECT_EQ(form_field_data.label, u"foo bar");
  EXPECT_EQ(form_field_data.label_source, FormFieldData::LabelSource::kForId);
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  <LABEL for='firstname'><span>First name: </span></LABEL>"
      "    <INPUT type='text' id='firstname' value='John'/>"
      "  <LABEL for='lastname'><span>Last name: </span></LABEL>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  <LABEL for='email'><span>Email: </span></LABEL>"
      "    <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  <LABEL for='firstname'> First name: </LABEL>"
      "    <INPUT type='text' name='firstname' value='John'/>"
      "  <LABEL for='lastname'> Last name: </LABEL>"
      "    <INPUT type='text' name='lastname' value='Smith'/>"
      "  <LABEL for='email'> Email: </LABEL>"
      "    <INPUT type='text' name='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>",
      id_attributes, name_attributes, labels, names, values);
}

// This test has three form control elements, only one of which has a label
// element associated with it.
TEST_F(FormAutofillTest, OneLabelElement) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  First name:"
      "    <INPUT type='text' id='firstname' value='John'/>"
      "  <LABEL for='lastname'>Last name: </LABEL>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  Email:"
      "    <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromText) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  First name:"
      "    <INPUT type='text' id='firstname' value='John'/>"
      "  Last name:"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  Email:"
      "    <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromParagraph) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  <P>First name:</P><INPUT type='text' "
      "                           id='firstname' value='John'/>"
      "  <P>Last name:</P>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  <P>Email:</P>"
      "    <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromBold) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  <B>First name:</B><INPUT type='text' "
      "                           id='firstname' value='John'/>"
      "  <B>Last name:</B>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  <B>Email:</B>"
      "    <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredPriorToImgOrBr) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  First name:<IMG/><INPUT type='text' "
      "                          id='firstname' value='John'/>"
      "  Last name:<IMG/>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  Email:<BR/>"
      "    <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableCell) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TD>First name:</TD>"
      "    <TD><INPUT type='text' id='firstname' value='John'/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>Last name:</TD>"
      "    <TD><INPUT type='text' id='lastname' value='Smith'/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>Email:</TD>"
      "    <TD><INPUT type='text' id='email'"
      "               value='john@example.com'/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type='submit' name='reply-send' value='Send'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableCellTH) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TH>First name:</TH>"
      "    <TD><INPUT type='text' id='firstname' value='John'/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TH>Last name:</TH>"
      "    <TD><INPUT type='text' id='lastname' value='Smith'/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TH>Email:</TH>"
      "    <TD><INPUT type='text' id='email'"
      "               value='john@example.com'/></TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type='submit' name='reply-send' value='Send'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>");
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <FONT>"
      "        First name:"
      "      </FONT>"
      "      <FONT>"
      "        Bogus"
      "      </FONT>"
      "    </TD>"
      "    <TD>"
      "      <FONT>"
      "        <INPUT type='text' id='firstname' value='John'/>"
      "      </FONT>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <FONT>"
      "        Last name:"
      "      </FONT>"
      "    </TD>"
      "    <TD>"
      "      <FONT>"
      "        <INPUT type='text' id='lastname' value='Smith'/>"
      "      </FONT>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <FONT>"
      "        Email:"
      "      </FONT>"
      "    </TD>"
      "    <TD>"
      "      <FONT>"
      "        <INPUT type='text' id='email' value='john@example.com'/>"
      "      </FONT>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type='submit' name='reply-send' value='Send'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>First Name</B>"
      "    </TD>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type='text' id='firstname' value='John'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Last Name</B>"
      "    </TD>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type='text' id='lastname' value='Smith'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Email</B>"
      "    </TD>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type='text' id='email' value='john@example.com'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type='submit' name='reply-send' value='Send'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TD>* First Name</TD>"
      "    <TD>"
      "      Bogus"
      "      <INPUT type='hidden'/>"
      "      <INPUT type='text' id='firstname' value='John'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>* Last Name</TD>"
      "    <TD>"
      "      <INPUT type='text' id='lastname' value='Smith'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>* Email</TD>"
      "    <TD>"
      "      <INPUT type='text' id='email' value='john@example.com'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type='submit' name='reply-send' value='Send'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
      id_attributes, name_attributes, labels, names, values);
}

// <script>, <noscript> and <option> tags are excluded when the labels are
// inferred.
// Also <!-- comment --> is excluded.
TEST_F(FormAutofillTest, LabelsInferredFromTableWithSpecialElements) {
  FormFieldData expected;
  std::vector<FormFieldData> fields;

  expected.id_attribute = u"firstname";
  expected.name_attribute = u"";
  expected.label = u"* First Name";
  expected.name = expected.id_attribute;
  expected.value = u"John";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  fields.push_back(expected);

  expected.id_attribute = u"middlename";
  expected.name_attribute = u"";
  expected.label = u"* Middle Name";
  expected.name = expected.id_attribute;
  expected.value = u"Joe";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  fields.push_back(expected);

  expected.id_attribute = u"lastname";
  expected.name_attribute = u"";
  expected.label = u"* Last Name";
  expected.name = expected.id_attribute;
  expected.value = u"Smith";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  fields.push_back(expected);

  expected.id_attribute = u"country";
  expected.name_attribute = u"";
  expected.label = u"* Country";
  expected.name = expected.id_attribute;
  expected.value = u"US";
  expected.form_control_type = FormControlType::kSelectOne;
  expected.max_length = 0;
  fields.push_back(expected);

  expected.id_attribute = u"email";
  expected.name_attribute = u"";
  expected.label = u"* Email";
  expected.name = expected.id_attribute;
  expected.value = u"john@example.com";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  fields.push_back(expected);

  ExpectLabelsAndTypes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>First Name</B>"
      "    </TD>"
      "    <TD>"
      "      <SCRIPT> <!-- function test() { alert('ignored as label'); } -->"
      "      </SCRIPT>"
      "      <INPUT type='text' id='firstname' value='John'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Middle Name</B>"
      "    </TD>"
      "    <TD>"
      "      <NOSCRIPT>"
      "        <P>Bad</P>"
      "      </NOSCRIPT>"
      "      <INPUT type='text' id='middlename' value='Joe'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Last Name</B>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type='text' id='lastname' value='Smith'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Country</B>"
      "    </TD>"
      "    <TD>"
      "      <SELECT id='country'>"
      "        <OPTION VALUE='US'>The value should be ignored as label."
      "        </OPTION>"
      "        <OPTION VALUE='JP'>JAPAN</OPTION>"
      "      </SELECT>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN>"
      "      <B>Email</B>"
      "    </TD>"
      "    <TD>"
      "      <!-- This comment should be ignored as inferred label.-->"
      "      <INPUT type='text' id='email' value='john@example.com'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD></TD>"
      "    <TD>"
      "      <INPUT type='submit' name='reply-send' value='Send'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
      fields);
}

TEST_F(FormAutofillTest, LabelsInferredFromTableLabels) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <LABEL>First name:</LABEL>"
      "      <INPUT type='text' id='firstname' value='John'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <LABEL>Last name:</LABEL>"
      "      <INPUT type='text' id='lastname' value='Smith'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <LABEL>Email:</LABEL>"
      "      <INPUT type='text' id='email' value='john@example.com'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "<INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromTableTDInterveningElements) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      First name:"
      "      <BR>"
      "      <INPUT type='text' id='firstname' value='John'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      Last name:"
      "      <BR>"
      "      <INPUT type='text' id='lastname' value='Smith'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      Email:"
      "      <BR>"
      "      <INPUT type='text' id='email' value='john@example.com'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "<INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>");
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN><B>First Name</B>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type='text' id='firstname' value='John'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN><B>Last Name</B>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type='text' id='lastname' value='Smith'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <SPAN>*</SPAN><B>Email</B>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type='text' id='email' value='john@example.com'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <INPUT type='submit' name='reply-send' value='Send'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>"
      "</FORM>",
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<TABLE>"
      "  <TR>"
      "    <TD>*First Name</TD>"
      "    <TD>*Last Name</TD>"
      "    <TD>*Email</TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <INPUT type='text' id='firstname' value='John'/>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type='text' id='lastname' value='Smith'/>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type='text' id='email' value='john@example.com'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD colspan='2'>NAME</TD>"
      "    <TD>EMAIL</TD>"
      "  </TR>"
      "  <TR>"
      "    <TD colspan='2'>"
      "      <INPUT type='text' id='name2' value='John Smith'/>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type='text' id='email2' value='john@example2.com'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>Phone</TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <INPUT type='text' id='phone1' value='123'/>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type='text' id='phone2' value='456'/>"
      "    </TD>"
      "    <TD>"
      "      <INPUT type='text' id='phone3' value='7890'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TH>"
      "      Credit Card Number"
      "    </TH>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <INPUT type='text' name='ccnumber' value='4444555544445555'/>"
      "    </TD>"
      "  </TR>"
      "  <TR>"
      "    <TD>"
      "      <INPUT type='submit' name='reply-send' value='Send'/>"
      "    </TD>"
      "  </TR>"
      "</TABLE>",
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<DIV>"
      "  <LI>"
      "    <SPAN>Bogus</SPAN>"
      "  </LI>"
      "  <LI>"
      "    <LABEL><EM>*</EM> Home Phone</LABEL>"
      "    <INPUT type='text' id='areacode' value='415'/>"
      "    <INPUT type='text' id='prefix' value='555'/>"
      "    <INPUT type='text' id='suffix' value='1212'/>"
      "  </LI>"
      "  <LI>"
      "    <INPUT type='submit' name='reply-send' value='Send'/>"
      "  </LI>"
      "</DIV>"
      "</FORM>",
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<DL>"
      "  <DT>"
      "    <SPAN>"
      "      *"
      "    </SPAN>"
      "    <SPAN>"
      "      First name:"
      "    </SPAN>"
      "    <SPAN>"
      "      Bogus"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type='text' id='firstname' value='John'/>"
      "    </FONT>"
      "  </DD>"
      "  <DT>"
      "    <SPAN>"
      "      Last name:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type='text' id='lastname' value='Smith'/>"
      "    </FONT>"
      "  </DD>"
      "  <DT>"
      "    <SPAN>"
      "      Email:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type='text' id='email' value='john@example.com'/>"
      "    </FONT>"
      "  </DD>"
      "  <DT></DT>"
      "  <DD>"
      "    <INPUT type='submit' name='reply-send' value='Send'/>"
      "  </DD>"
      "</DL>"
      "</FORM>",
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  Address Line 1:"
      "    <INPUT type='text' name='Address'/>"
      "  Address Line 2:"
      "    <INPUT type='text' name='Address'/>"
      "  Address Line 3:"
      "    <INPUT type='text' name='Address'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>",
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
  labels.push_back(u"");
  names.push_back(name_attributes.back());
  values.emplace_back();

  id_attributes.push_back(u"");
  name_attributes.push_back(u"dayphone3");
  labels.push_back(u"");
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  Phone:"
      "  <input type='text' name='dayphone1'>"
      "  <img/>"
      "  -"
      "  <img/>"
      "  <input type='text' name='dayphone2'>"
      "  <img/>"
      "  -"
      "  <img/>"
      "  <input type='text' name='dayphone3'>"
      "  ext.:"
      "  <input type='text' name='dayphone4'>"
      "  <input type='text' name='dummy'>"
      "  <input type='submit' name='reply-send' value='Send'>"
      "</FORM>",
      id_attributes, name_attributes, labels, names, values);
}

TEST_F(FormAutofillTest, LabelsInferredFromDivTable) {
  ExpectJohnSmithLabelsAndNameAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<DIV>First name:<BR>"
      "  <SPAN>"
      "    <INPUT type='text' name='firstname' value='John'>"
      "  </SPAN>"
      "</DIV>"
      "<DIV>Last name:<BR>"
      "  <SPAN>"
      "    <INPUT type='text' name='lastname' value='Smith'>"
      "  </SPAN>"
      "</DIV>"
      "<DIV>Email:<BR>"
      "  <SPAN>"
      "    <INPUT type='text' name='email' value='john@example.com'>"
      "  </SPAN>"
      "</DIV>"
      "<input type='submit' name='reply-send' value='Send'>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromDivSiblingTable) {
  ExpectJohnSmithLabelsAndNameAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<DIV>First name:</DIV>"
      "<DIV>"
      "  <SPAN>"
      "    <INPUT type='text' name='firstname' value='John'>"
      "  </SPAN>"
      "</DIV>"
      "<DIV>Last name:</DIV>"
      "<DIV>"
      "  <SPAN>"
      "    <INPUT type='text' name='lastname' value='Smith'>"
      "  </SPAN>"
      "</DIV>"
      "<DIV>Email:</DIV>"
      "<DIV>"
      "  <SPAN>"
      "    <INPUT type='text' name='email' value='john@example.com'>"
      "  </SPAN>"
      "</DIV>"
      "<input type='submit' name='reply-send' value='Send'>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromLabelInDivTable) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<LABEL>First name:</LABEL>"
      "<LABEL for='lastname'>Last name:</LABEL>"
      "<DIV>"
      "  <INPUT type='text' id='firstname' value='John'>"
      "</DIV>"
      "<DIV>"
      "  <INPUT type='text' id='lastname' value='Smith'>"
      "</DIV>"
      "<LABEL>Email:</LABEL>"
      "<DIV>"
      "  <SPAN>"
      "    <INPUT type='text' id='email' value='john@example.com'>"
      "  </SPAN>"
      "</DIV>"
      "<input type='submit' name='reply-send' value='Send'>"
      "</FORM>");
}

TEST_F(FormAutofillTest, LabelsInferredFromDefinitionListRatherThanDivTable) {
  ExpectJohnSmithLabelsAndIdAttributes(
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "<DIV>This is not a label.<BR>"
      "<DL>"
      "  <DT>"
      "    <SPAN>"
      "      First name:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type='text' id='firstname' value='John'/>"
      "    </FONT>"
      "  </DD>"
      "  <DT>"
      "    <SPAN>"
      "      Last name:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type='text' id='lastname' value='Smith'/>"
      "    </FONT>"
      "  </DD>"
      "  <DT>"
      "    <SPAN>"
      "      Email:"
      "    </SPAN>"
      "  </DT>"
      "  <DD>"
      "    <FONT>"
      "      <INPUT type='text' id='email' value='john@example.com'/>"
      "    </FONT>"
      "  </DD>"
      "  <DT></DT>"
      "  <DD>"
      "    <INPUT type='submit' name='reply-send' value='Send'/>"
      "  </DD>"
      "</DL>"
      "</DIV>"
      "</FORM>");
}

TEST_F(FormAutofillTest, FillFormMaxLength) {
  TestFillFormMaxLength(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' maxlength='5'/>"
      "  <INPUT type='text' id='lastname' maxlength='7'/>"
      "  <INPUT type='text' id='email' maxlength='9'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>",
      false);
}

TEST_F(FormAutofillTest, FillFormMaxLengthForUnownedForm) {
  TestFillFormMaxLength(
      "<HEAD><TITLE>delivery recipient info</TITLE></HEAD>"
      "<INPUT type='text' id='firstname' maxlength='5'/>"
      "<INPUT type='text' id='lastname' maxlength='7'/>"
      "<INPUT type='text' id='email' maxlength='9'/>"
      "<INPUT type='submit' name='reply-send' value='Send'/>",
      true);
}

// This test uses negative values of the maxlength attribute for input elements.
// In this case, the maxlength of the input elements is set to the default
// maxlength (defined in WebKit.)
TEST_F(FormAutofillTest, FillFormNegativeMaxLength) {
  TestFillFormNegativeMaxLength(
      "<HEAD><TITLE>delivery recipient info</TITLE></HEAD>"
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' maxlength='-1'/>"
      "  <INPUT type='text' id='lastname' maxlength='-10'/>"
      "  <INPUT type='text' id='email' maxlength='-13'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>",
      false);
}

TEST_F(FormAutofillTest, FillFormNegativeMaxLengthForUnownedForm) {
  TestFillFormNegativeMaxLength(
      "<HEAD><TITLE>delivery recipient info</TITLE></HEAD>"
      "<INPUT type='text' id='firstname' maxlength='-1'/>"
      "<INPUT type='text' id='lastname' maxlength='-10'/>"
      "<INPUT type='text' id='email' maxlength='-13'/>"
      "<INPUT type='submit' name='reply-send' value='Send'/>",
      true);
}

TEST_F(FormAutofillTest, FillFormEmptyName) {
  TestFillFormEmptyName(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname'/>"
      "  <INPUT type='text' id='lastname'/>"
      "  <INPUT type='text' id='email'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      false);
}

TEST_F(FormAutofillTest, FillFormEmptyNameForUnownedForm) {
  TestFillFormEmptyName(
      "<HEAD><TITLE>delivery recipient info</TITLE></HEAD>"
      "<INPUT type='text' id='firstname'/>"
      "<INPUT type='text' id='lastname'/>"
      "<INPUT type='text' id='email'/>"
      "<INPUT type='submit' value='Send'/>",
      true);
}

TEST_F(FormAutofillTest, FillFormEmptyFormNames) {
  TestFillFormEmptyFormNames(
      "<FORM action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname'/>"
      "  <INPUT type='text' id='middlename'/>"
      "  <INPUT type='text' id='lastname'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>"
      "<FORM action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='apple'/>"
      "  <INPUT type='text' id='banana'/>"
      "  <INPUT type='text' id='cantelope'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      false);
}

TEST_F(FormAutofillTest, FillFormEmptyFormNamesForUnownedForm) {
  TestFillFormEmptyFormNames(
      "<HEAD><TITLE>enter delivery preferences</TITLE></HEAD>"
      "<INPUT type='text' id='firstname'/>"
      "<INPUT type='text' id='middlename'/>"
      "<INPUT type='text' id='lastname'/>"
      "<INPUT type='text' id='apple'/>"
      "<INPUT type='text' id='banana'/>"
      "<INPUT type='text' id='cantelope'/>"
      "<INPUT type='submit' value='Send'/>",
      true);
}

TEST_F(FormAutofillTest, ThreePartPhone) {
  LoadHTML("<FORM name='TestForm' action='http://cnn.com' method='post'>"
           "  Phone:"
           "  <input type='text' name='dayphone1'>"
           "  -"
           "  <input type='text' name='dayphone2'>"
           "  -"
           "  <input type='text' name='dayphone3'>"
           "  ext.:"
           "  <input type='text' name='dayphone4'>"
           "  <input type='submit' name='reply-send' value='Send'>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebVector<WebFormElement> forms = frame->GetDocument().Forms();
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(forms[0], WebFormControlElement(),
                                       nullptr, {ExtractOption::kValue}, &form,
                                       nullptr));
  EXPECT_EQ(u"TestForm", form.name);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(4U, fields.size());

  FormFieldData expected;
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;

  expected.label = u"Phone:";
  expected.name_attribute = u"dayphone1";
  expected.name = expected.name_attribute;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.label = u"";
  expected.name_attribute = u"dayphone2";
  expected.name = expected.name_attribute;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.label = u"";
  expected.name_attribute = u"dayphone3";
  expected.name = expected.name_attribute;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.label = u"ext.:";
  expected.name_attribute = u"dayphone4";
  expected.name = expected.name_attribute;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);
}

TEST_F(FormAutofillTest, MaxLengthFields) {
  LoadHTML("<FORM name='TestForm' action='http://cnn.com' method='post'>"
           "  Phone:"
           "  <input type='text' maxlength='3' name='dayphone1'>"
           "  -"
           "  <input type='text' maxlength='3' name='dayphone2'>"
           "  -"
           "  <input type='text' maxlength='4' size='5'"
           "         name='dayphone3'>"
           "  ext.:"
           "  <input type='text' maxlength='5' name='dayphone4'>"
           "  <input type='text' name='default1'>"
           "  <input type='text' maxlength='-1' name='invalid1'>"
           "  <input type='submit' name='reply-send' value='Send'>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebVector<WebFormElement> forms = frame->GetDocument().Forms();
  ASSERT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(forms[0], WebFormControlElement(),
                                       nullptr, {ExtractOption::kValue}, &form,
                                       nullptr));
  EXPECT_EQ(u"TestForm", form.name);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(6U, fields.size());

  FormFieldData expected;
  expected.form_control_type = FormControlType::kInputText;

  expected.name_attribute = u"dayphone1";
  expected.label = u"Phone:";
  expected.name = expected.name_attribute;
  expected.max_length = 3;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.name_attribute = u"dayphone2";
  expected.label = u"";
  expected.name = expected.name_attribute;
  expected.max_length = 3;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.name_attribute = u"dayphone3";
  expected.label = u"";
  expected.name = expected.name_attribute;
  expected.max_length = 4;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  expected.name_attribute = u"dayphone4";
  expected.label = u"ext.:";
  expected.name = expected.name_attribute;
  expected.max_length = 5;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[3]);

  // When unspecified |size|, default is returned.
  expected.name_attribute = u"default1";
  expected.label.clear();
  expected.name = expected.name_attribute;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[4]);

  // When invalid |size|, default is returned.
  expected.name_attribute = u"invalid1";
  expected.label.clear();
  expected.name = expected.name_attribute;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[5]);
}

// This test re-creates the experience of typing in a field then selecting a
// profile from the Autofill suggestions popup.  The field that is being typed
// into should be filled even though it's not technically empty.
TEST_F(FormAutofillTest, FillFormNonEmptyField) {
  TestFillFormNonEmptyField(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname'/>"
      "  <INPUT type='text' id='lastname'/>"
      "  <INPUT type='text' id='email'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      false, nullptr, nullptr, nullptr, nullptr, nullptr);
}

TEST_F(FormAutofillTest, FillFormNonEmptyFieldsWithDefaultValues) {
  TestFillFormNonEmptyField(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='Enter first name'/>"
      "  <INPUT type='text' id='lastname' value='Enter last name'/>"
      "  <INPUT type='text' id='email' value='Enter email'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      false, "Enter last name", "Enter email", nullptr, nullptr, nullptr);
}

TEST_F(FormAutofillTest, FillFormModifyValues) {
  TestFillFormAndModifyValues(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' placeholder='First Name' "
      "value='First Name'/>"
      "  <INPUT type='text' id='lastname' placeholder='Last Name' value='Last "
      "Name'/>"
      "  <INPUT type='text' id='phone' placeholder='Phone' value='Phone'/>"
      "  <INPUT type='text' id='cc' placeholder='Credit Card Number' "
      "value='Credit Card'/>"
      "  <INPUT type='text' id='city' placeholder='City' value='City'/>"
      "  <SELECT id='state' name='state' placeholder='State'>"
      "    <OPTION selected>?</OPTION>"
      "    <OPTION>AA</OPTION>"
      "    <OPTION>AE</OPTION>"
      "    <OPTION>AK</OPTION>"
      "  </SELECT>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      "First Name", "Last Name", "Phone", "Credit Card Number", "City",
      "State");
}

TEST_F(FormAutofillTest, FillFormModifyInitiatingValue) {
  TestFillFormAndModifyInitiatingValue(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='cc' placeholder='Credit Card Number' "
      "value='Credit Card'/>"
      "  <INPUT type='text' id='expiration_date' placeholder='Expiration Date' "
      "value='Expiration Date'/>"
      "  <INPUT type='text' id='name' placeholder='Full Name' "
      "value='Full Name'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      "Credit Card Number", "Expiration Date", "Full Name");
}

TEST_F(FormAutofillTest, FillFormJSModifiesUserInputValue) {
  TestFillFormJSModifiesUserInputValue(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='cc' placeholder='Credit Card Number' "
      "value='Credit Card'/>"
      "  <INPUT type='text' id='expiration_date' placeholder='Expiration Date' "
      "value='Expiration Date'/>"
      "  <INPUT type='text' id='name' placeholder='Full Name' "
      "value='Full Name'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      "Credit Card Number", "Expiration Date", "Full Name");
}

TEST_F(FormAutofillTest, FillFormNonEmptyFieldsWithPlaceholderValues) {
  TestFillFormNonEmptyField(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' placeholder='First Name' "
      "value='First Name'/>"
      "  <INPUT type='text' id='lastname' placeholder='Last Name' value='Last "
      "Name'/>"
      "  <INPUT type='text' id='email' placeholder='Email' value='Email'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      false, nullptr, nullptr, "First Name", "Last Name", "Email");
}

TEST_F(FormAutofillTest, FillFormWithPlaceholderValues) {
  TestFillFormWithPlaceholderValues(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' placeholder='First Name' "
      "value='First Name'/>"
      "  <INPUT type='text' id='lastname' placeholder='Last Name'"
      "Name'/>"
      "  <INPUT type='text' id='email' placeholder='Email' value='Email'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      "First Name", "Last Name", "Email");
}

TEST_F(FormAutofillTest, FillFormNonEmptyFieldForUnownedForm) {
  TestFillFormNonEmptyField(
      "<HEAD><TITLE>delivery recipient info</TITLE></HEAD>"
      "<INPUT type='text' id='firstname'/>"
      "<INPUT type='text' id='lastname'/>"
      "<INPUT type='text' id='email'/>"
      "<INPUT type='submit' value='Send'/>",
      true, nullptr, nullptr, nullptr, nullptr, nullptr);
}

TEST_F(FormAutofillTest, UndoAutofill) {
  LoadHTML(R"(
    <form id="form_id">
        <input id="text_id_1">
        <input id="text_id_2">
        <select id="select_id_1">
          <option value="undo_select_option_1">Foo</option>
          <option value="autofill_select_option_1">Bar</option>
        </select>
        <select id="select_id_2">
          <option value="undo_select_option_2">Foo</option>
          <option value="autofill_select_option_2">Bar</option>
        </select>
        <selectlist id="selectlist_id_1">
          <option value="undo_selectlist_option_1">Foo</option>
          <option value="autofill_selectlist_option_1">Bar</option>
        </selectlist>
        <selectlist id="selectlist_id_2">
          <option value="undo_selectlist_option_2">Foo</option>
          <option value="autofill_selectlist_option_2">Bar</option>
        </selectlist>
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

  WebFormControlElement selectlist_element_1 =
      GetFormControlElementById("selectlist_id_1");
  WebFormControlElement selectlist_element_2 =
      GetFormControlElementById("selectlist_id_2");
  selectlist_element_1.SetAutofillValue("autofill_selectlist_option_1",
                                        WebAutofillState::kAutofilled);
  selectlist_element_2.SetAutofillValue("autofill_selectlist_option_2",
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
  ASSERT_THAT(selectlist_element_1,
              HasAutofillValue("autofill_selectlist_option_1",
                               WebAutofillState::kAutofilled));
  ASSERT_THAT(selectlist_element_2,
              HasAutofillValue("autofill_selectlist_option_2",
                               WebAutofillState::kAutofilled));

  WebVector<WebFormElement> forms = GetMainFrame()->GetDocument().Forms();
  EXPECT_EQ(1U, forms.size());

  FormData form;
  EXPECT_TRUE(WebFormElementToFormData(forms[0], WebFormControlElement(),
                                       nullptr, {ExtractOption::kValue}, &form,
                                       nullptr));

  EXPECT_EQ(form.fields.size(), 6u);
  std::vector<FormFieldData> undo_fields;
  for (size_t i = 0; i < 6; i += 2) {
    std::u16string type = i == 0   ? u"text"
                          : i == 2 ? u"select_option"
                                   : u"selectlist_option";
    form.fields[i].value = u"undo_" + type + u"_1";
    form.fields[i].is_autofilled = false;
    undo_fields.push_back(form.fields[i]);
  }

  form.fields = undo_fields;
  ApplyFormAction(form, text_element_1, mojom::ActionType::kUndo,
                  mojom::ActionPersistence::kFill);
  EXPECT_THAT(text_element_1,
              HasAutofillValue("undo_text_1", WebAutofillState::kNotFilled));
  EXPECT_THAT(text_element_2, HasAutofillValue("autofill_text_2",
                                               WebAutofillState::kAutofilled));
  EXPECT_THAT(select_element_1, HasAutofillValue("undo_select_option_1",
                                                 WebAutofillState::kNotFilled));
  EXPECT_THAT(select_element_2,
              HasAutofillValue("autofill_select_option_2",
                               WebAutofillState::kAutofilled));
  EXPECT_THAT(selectlist_element_1,
              HasAutofillValue("undo_selectlist_option_1",
                               WebAutofillState::kNotFilled));
  EXPECT_THAT(selectlist_element_2,
              HasAutofillValue("autofill_selectlist_option_2",
                               WebAutofillState::kAutofilled));
}

TEST_F(FormAutofillTest, ClearSectionWithNode) {
  TestClearSectionWithNode(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='Wyatt'/>"
      "  <INPUT type='text' id='lastname' value='Earp'/>"
      "  <INPUT type='text' autocomplete='off' id='noAC' value='one'/>"
      "  <INPUT type='text' id='notenabled' disabled='disabled'>"
      "  <INPUT type='month' id='month' value='2012-11'>"
      "  <INPUT type='month' id='month-disabled' value='2012-11'"
      "         disabled='disabled'>"
      "  <TEXTAREA id='textarea'>Apple.</TEXTAREA>"
      "  <TEXTAREA id='textarea-disabled' disabled='disabled'>"
      "    Banana!"
      "  </TEXTAREA>"
      "  <TEXTAREA id='textarea-noAC' autocomplete='off'>Carrot?</TEXTAREA>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      false);
}

// Test regular FillForm function.
TEST_F(FormAutofillTest, ClearTwoSections) {
  TestClearTwoSections(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname-shipping'/>"
      "  <INPUT type='text' id='lastname-shipping'/>"
      "  <INPUT type='text' id='city-shipping'/>"
      "  <INPUT type='text' id='firstname-billing'/>"
      "  <INPUT type='text' id='lastname-billing'/>"
      "  <INPUT type='text' id='city-billing'/>"
      "</FORM>",
      false);
}

TEST_F(FormAutofillTest, ClearSectionWithNodeForUnownedForm) {
  TestClearSectionWithNode(
      "<HEAD><TITLE>store checkout</TITLE></HEAD>"
      "  <!-- Indented on purpose //-->"
      "  <INPUT type='text' id='firstname' value='Wyatt'/>"
      "  <INPUT type='text' id='lastname' value='Earp'/>"
      "  <INPUT type='text' autocomplete='off' id='noAC' value='one'/>"
      "  <INPUT type='text' id='notenabled' disabled='disabled'>"
      "  <INPUT type='month' id='month' value='2012-11'>"
      "  <INPUT type='month' id='month-disabled' value='2012-11'"
      "         disabled='disabled'>"
      "  <TEXTAREA id='textarea'>Apple.</TEXTAREA>"
      "  <TEXTAREA id='textarea-disabled' disabled='disabled'>"
      "    Banana!"
      "  </TEXTAREA>"
      "  <TEXTAREA id='textarea-noAC' autocomplete='off'>Carrot?</TEXTAREA>"
      "  <INPUT type='submit' value='Send'/>",
      true);
}

TEST_F(FormAutofillTest, ClearSectionWithNodeContainingSelectOne) {
  TestClearSectionWithNodeContainingSelectOne(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='Wyatt'/>"
      "  <INPUT type='text' id='lastname' value='Earp'/>"
      "  <SELECT id='state' name='state'>"
      "    <OPTION selected>?</OPTION>"
      "    <OPTION>AA</OPTION>"
      "    <OPTION>AE</OPTION>"
      "    <OPTION>AK</OPTION>"
      "  </SELECT>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>",
      false);
}

TEST_F(FormAutofillTest,
       ClearSectionWithNodeContainingSelectOneForUnownedForm) {
  TestClearSectionWithNodeContainingSelectOne(
      "<HEAD><TITLE>store checkout</TITLE></HEAD>"
      "<INPUT type='text' id='firstname' value='Wyatt'/>"
      "<INPUT type='text' id='lastname' value='Earp'/>"
      "<SELECT id='state' name='state'>"
      "  <OPTION selected>?</OPTION>"
      "  <OPTION>AA</OPTION>"
      "  <OPTION>AE</OPTION>"
      "  <OPTION>AK</OPTION>"
      "</SELECT>"
      "<INPUT type='submit' value='Send'/>",
      true);
}

TEST_F(FormAutofillTest, ClearPreviewedElements) {
  TestClearPreviewedElements(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='Wyatt'/>"
      "  <INPUT type='text' id='lastname'/>"
      "  <INPUT type='text' id='email'/>"
      "  <INPUT type='email' id='email2'/>"
      "  <INPUT type='tel' id='phone'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, ClearPreviewedFormWithElementForUnownedForm) {
  TestClearPreviewedElements(
      "<HEAD><TITLE>store checkout</TITLE></HEAD>"
      "<INPUT type='text' id='firstname' value='Wyatt'/>"
      "<INPUT type='text' id='lastname'/>"
      "<INPUT type='text' id='email'/>"
      "<INPUT type='email' id='email2'/>"
      "<INPUT type='tel' id='phone'/>"
      "<INPUT type='submit' value='Send'/>");
}

TEST_F(FormAutofillTest, ClearPreviewedFormWithNonEmptyInitiatingNode) {
  TestClearPreviewedFormWithNonEmptyInitiatingNode(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='W'/>"
      "  <INPUT type='text' id='lastname'/>"
      "  <INPUT type='text' id='email'/>"
      "  <INPUT type='email' id='email2'/>"
      "  <INPUT type='tel' id='phone'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest,
       ClearPreviewedFormWithNonEmptyInitiatingNodeForUnownedForm) {
  TestClearPreviewedFormWithNonEmptyInitiatingNode(
      "<HEAD><TITLE>shipping details</TITLE></HEAD>"
      "<INPUT type='text' id='firstname' value='W'/>"
      "<INPUT type='text' id='lastname'/>"
      "<INPUT type='text' id='email'/>"
      "<INPUT type='email' id='email2'/>"
      "<INPUT type='tel' id='phone'/>"
      "<INPUT type='submit' value='Send'/>");
}

TEST_F(FormAutofillTest, ClearPreviewedFormWithAutofilledInitiatingNode) {
  TestClearPreviewedFormWithAutofilledInitiatingNode(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='W'/>"
      "  <INPUT type='text' id='lastname'/>"
      "  <INPUT type='text' id='email'/>"
      "  <INPUT type='email' id='email2'/>"
      "  <INPUT type='tel' id='phone'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest,
       ClearPreviewedFormWithAutofilledInitiatingNodeForUnownedForm) {
  TestClearPreviewedFormWithAutofilledInitiatingNode(
      "<HEAD><TITLE>shipping details</TITLE></HEAD>"
      "<INPUT type='text' id='firstname' value='W'/>"
      "<INPUT type='text' id='lastname'/>"
      "<INPUT type='text' id='email'/>"
      "<INPUT type='email' id='email2'/>"
      "<INPUT type='tel' id='phone'/>"
      "<INPUT type='submit' value='Send'/>");
}

// Autofill's "Clear Form" should clear only autofilled fields
TEST_F(FormAutofillTest, ClearOnlyAutofilledFields) {
  TestClearOnlyAutofilledFields(
      "<FORM name='TestForm' action='http://abc.com' method='post'>"
      "  <INPUT type='text' id='firstname' value='Wyatt'/>"
      "  <INPUT type='text' id='lastname' value='Earp'/>"
      "  <INPUT type='email' id='email' value='wyatt@earp.com'/>"
      "  <INPUT type='tel' id='phone' value='650-777-9999'/>"
      "  <INPUT type='submit' value='Send'/>"
      "</FORM>");
}

TEST_F(FormAutofillTest, ClearOnlyAutofilledFieldsForUnownedForm) {
  TestClearOnlyAutofilledFields(
      "<HEAD><TITLE>shipping details</TITLE></HEAD>"
      "<INPUT type='text' id='firstname' value='Wyatt'/>"
      "<INPUT type='text' id='lastname' value='Earp'/>"
      "<INPUT type='email' id='email' value='wyatt@earp.com'/>"
      "<INPUT type='tel' id='phone' value='650-777-9999'/>"
      "<INPUT type='submit' value='Send'/>");
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
      "<FORM name='TestForm' action='http://cnn.com' method='post'>"
      "  <LABEL for='firstname'> First Name: </LABEL>"
      "  <LABEL for='firstname'></LABEL>"
      "    <INPUT type='text' id='firstname' value='John'/>"
      "  <LABEL for='lastname'></LABEL>"
      "  <LABEL for='lastname'> Last Name: </LABEL>"
      "    <INPUT type='text' id='lastname' value='Smith'/>"
      "  <LABEL for='email'> Email: </LABEL>"
      "  <LABEL for='email'> xxx@yyy.com </LABEL>"
      "    <INPUT type='text' id='email' value='john@example.com'/>"
      "  <INPUT type='submit' name='reply-send' value='Send'/>"
      "</FORM>",
      id_attributes, name_attributes, labels, names, values);
}

TEST_F(FormAutofillTest, ClickElement) {
  LoadHTML("<BUTTON id='link'>Button</BUTTON>"
           "<BUTTON name='button'>Button</BUTTON>");
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
  LoadHTML("<FORM name='TestForm' action='http://cnn.com' method='post'>"
           "  <INPUT type='text' id='firstname' value='John'/>"
           "  <INPUT type='text' id='lastname' value='Smith'/>"
           "  <SELECT id='country'>"
           "    <OPTION value='AF'>Afghanistan</OPTION>"
           "    <OPTION value='AL'>Albania</OPTION>"
           "    <OPTION value='DZ'>Algeria</OPTION>"
           "  </SELECT>"
           "  <INPUT type='submit' name='reply-send' value='Send'/>"
           "</FORM>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  // Set the value of the select-one.
  WebSelectElement select_element =
      frame->GetDocument().GetElementById("country").To<WebSelectElement>();
  select_element.SetValue(WebString::FromUTF8("AL"));

  WebVector<WebFormElement> forms = frame->GetDocument().Forms();
  ASSERT_EQ(1U, forms.size());

  FormData form;

  // Extract the country select-one value as text.
  EXPECT_TRUE(WebFormElementToFormData(
      forms[0], WebFormControlElement(), nullptr,
      {ExtractOption::kValue, ExtractOption::kOptionText}, &form, nullptr));
  EXPECT_EQ(u"TestForm", form.name);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;

  expected.id_attribute = u"firstname";
  expected.name = expected.id_attribute;
  expected.value = u"John";
  expected.label = u"John";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.id_attribute = u"lastname";
  expected.name = expected.id_attribute;
  expected.value = u"Smith";
  expected.label = u"Smith";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.id_attribute = u"country";
  expected.name = expected.id_attribute;
  expected.value = u"Albania";
  expected.label.clear();
  expected.form_control_type = FormControlType::kSelectOne;
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);

  form.fields.clear();
  // Extract the country select-one value as value.
  EXPECT_TRUE(WebFormElementToFormData(forms[0], WebFormControlElement(),
                                       nullptr, {ExtractOption::kValue}, &form,
                                       nullptr));
  EXPECT_EQ(u"TestForm", form.name);
  EXPECT_EQ(GURL("http://cnn.com"), form.action);

  ASSERT_EQ(3U, fields.size());

  expected.id_attribute = u"firstname";
  expected.name = expected.id_attribute;
  expected.value = u"John";
  expected.label = u"John";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.id_attribute = u"lastname";
  expected.name = expected.id_attribute;
  expected.value = u"Smith";
  expected.label = u"Smith";
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.id_attribute = u"country";
  expected.name = expected.id_attribute;
  expected.value = u"AL";
  expected.label.clear();
  expected.form_control_type = FormControlType::kSelectOne;
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
}

TEST_F(FormAutofillTest, UnownedFormElementsToFormDataWithoutForm) {
  std::vector<WebFormControlElement> control_elements;

  const DenseSet<ExtractOption> extract_options = {ExtractOption::kValue,
                                                   ExtractOption::kOptions};

  LoadHTML("<HEAD><TITLE>delivery info</TITLE></HEAD>"
           "<DIV>"
           "  <LABEL for='firstname'>First name:</LABEL>"
           "  <LABEL for='lastname'>Last name:</LABEL>"
           "  <INPUT type='text' id='firstname' value='John'/>"
           "  <INPUT type='text' id='lastname' value='Smith'/>"
           "  <LABEL for='email'>Email:</LABEL>"
           "  <INPUT type='text' id='email' value='john@example.com'/>"
           "</DIV>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  control_elements =
      GetUnownedAutofillableFormFieldElements(frame->GetDocument());
  ASSERT_EQ(3U, control_elements.size());

  std::vector<WebElement> iframe_elements;

  FormData form;
  EXPECT_TRUE(UnownedFormElementsToFormData(
      control_elements, iframe_elements, nullptr, frame->GetDocument(), nullptr,
      extract_options, &form, nullptr));

  EXPECT_TRUE(form.name.empty());
  EXPECT_FALSE(form.action.is_valid());

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  FormFieldData expected;
  expected.form_control_type = FormControlType::kInputText;
  expected.max_length = FormFieldData::kDefaultMaxLength;

  expected.id_attribute = u"firstname";
  expected.name = expected.id_attribute;
  expected.value = u"John";
  expected.label = u"First name:";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[0]);

  expected.id_attribute = u"lastname";
  expected.name = expected.id_attribute;
  expected.value = u"Smith";
  expected.label = u"Last name:";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[1]);

  expected.id_attribute = u"email";
  expected.name = expected.id_attribute;
  expected.value = u"john@example.com";
  expected.label = u"Email:";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, fields[2]);
}

TEST_F(FormAutofillTest, UnownedFormElementsToFormDataWithForm) {
  std::vector<WebFormControlElement> control_elements;

  const DenseSet<ExtractOption> extract_options = {ExtractOption::kValue,
                                                   ExtractOption::kOptions};

  LoadHTML(kFormHtml);

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  control_elements =
      GetUnownedAutofillableFormFieldElements(frame->GetDocument());
  ASSERT_TRUE(control_elements.empty());

  std::vector<WebElement> iframe_elements;

  FormData form;
  EXPECT_FALSE(UnownedFormElementsToFormData(
      control_elements, iframe_elements, nullptr, frame->GetDocument(), nullptr,
      extract_options, &form, nullptr));
}

TEST_F(FormAutofillTest, FormlessForms) {
  std::vector<WebFormControlElement> control_elements;

  const DenseSet<ExtractOption> extract_options = {ExtractOption::kValue,
                                                   ExtractOption::kOptions};

  LoadHTML(kUnownedUntitledFormHtml);

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  control_elements =
      GetUnownedAutofillableFormFieldElements(frame->GetDocument());
  ASSERT_FALSE(control_elements.empty());

  std::vector<WebElement> iframe_elements;

  {
    FormData form;
    EXPECT_TRUE(UnownedFormElementsToFormData(
        control_elements, iframe_elements, nullptr, frame->GetDocument(),
        nullptr, extract_options, &form, nullptr));
  }
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
       "<FORM name='TestForm' action='http://abc.com' method='post'>"
       "</FORM>",
       0u, true},
      // A form with less than three fields with no autocomplete type(s) should
      // be extracted because no minimum is being enforced for upload.
      {"Small Form no autocomplete",
       "<FORM name='TestForm' action='http://abc.com' method='post'>"
       "  <INPUT type='text' id='firstname'/>"
       "</FORM>",
       1u, true},
      // A form with less than three fields with at least one autocomplete type
      // should be extracted.
      {"Small Form w/ autocomplete",
       "<FORM name='TestForm' action='http://abc.com' method='post'>"
       "  <INPUT type='text' id='firstname' autocomplete='given-name'/>"
       "</FORM>",
       1u, true},
      // A form with three or more fields should be extracted.
      {"3 Field Form",
       "<FORM name='TestForm' action='http://abc.com' method='post'>"
       "  <INPUT type='text' id='firstname'/>"
       "  <INPUT type='text' id='lastname'/>"
       "  <INPUT type='text' id='email'/>"
       "  <INPUT type='submit' value='Send'/>"
       "</FORM>",
       1u, true},
      // An input field with an autocomplete attribute outside of a form should
      // be extracted.
      {"Small, formless, with autocomplete",
       "<INPUT type='text' id='firstname' autocomplete='given-name'/>"
       "<INPUT type='submit' value='Send'/>",
       1u, false},
      // An input field without an autocomplete attribute outside of a form,
      // with no checkout hints, should not be extracted.
      {"Small, formless, no autocomplete",
       "<INPUT type='text' id='firstname'/>"
       "<INPUT type='submit' value='Send'/>",
       1u, false},
      // A form with one field which is password gets extracted.
      {"Password-Only",
       "<FORM name='TestForm' action='http://abc.com' method='post'>"
       "  <INPUT type='password' id='pw'/>"
       "</FORM>",
       1u, true},
      // A form with two fields which are passwords should be extracted.
      {"two passwords",
       "<FORM name='TestForm' action='http://abc.com' method='post'>"
       "  <INPUT type='password' id='pw'/>"
       "  <INPUT type='password' id='new_pw'/>"
       "</FORM>",
       1u, true},
  };

  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);

    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);

    FormCache form_cache(web_frame);
    std::vector<FormData> forms =
        form_cache.UpdateFormCache(nullptr).updated_forms;
    EXPECT_EQ(test_case.number_of_extracted_forms, forms.size());
    if (!forms.empty())
      EXPECT_EQ(test_case.is_form_tag, forms.back().is_form_tag);
  }
}

TEST_F(FormAutofillTest, WebFormElementNotFoundInForm) {
  LoadHTML(
      "<form id='form'>"
      "  <input type='text' id='firstname' value='John'>"
      "  <input type='text' id='lastname' value='John'>"
      "</form>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_FALSE(web_form.IsNull());

  WebFormControlElement control_element = frame->GetDocument()
                                              .GetElementById("firstname")
                                              .To<WebFormControlElement>();
  ASSERT_FALSE(control_element.IsNull());
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(WebFormElementToFormData(web_form, control_element, nullptr,
                                       /*extract_options=*/{}, &form, &field));

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(2U, fields.size());
  EXPECT_EQ(u"firstname", fields[0].name);
  EXPECT_EQ(u"firstname", field.name);

  frame->ExecuteScript(blink::WebScriptSource(
      WebString("document.getElementById('firstname').remove();")));
  form = {};
  EXPECT_FALSE(WebFormElementToFormData(web_form, control_element, nullptr,
                                        /*extract_options=*/{}, &form, &field));
}

TEST_F(FormAutofillTest, AriaLabelAndDescription) {
  LoadHTML(
      "<form id='form'>"
      "  <div id='label'>aria label</div>"
      "  <div id='description'>aria description</div>"
      "  <input type='text' id='field0' aria-label='inline aria label'>"
      "  <input type='text' id='field1' aria-labelledby='label'>"
      "  <input type='text' id='field2' aria-describedby='description'>"
      "</form>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_FALSE(web_form.IsNull());

  WebFormControlElement control_element =
      frame->GetDocument().GetElementById("field0").To<WebFormControlElement>();
  ASSERT_FALSE(control_element.IsNull());
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(WebFormElementToFormData(web_form, control_element, nullptr,
                                       /*extract_options=*/{}, &form, &field));

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  // Field 0
  EXPECT_EQ(u"inline aria label", fields[0].aria_label);
  EXPECT_EQ(u"", fields[0].aria_description);

  // Field 1
  EXPECT_EQ(u"aria label", fields[1].aria_label);
  EXPECT_EQ(u"", fields[1].aria_description);

  // Field 2
  EXPECT_EQ(u"", fields[2].aria_label);
  EXPECT_EQ(u"aria description", fields[2].aria_description);
}

TEST_F(FormAutofillTest, AriaLabelAndDescription2) {
  LoadHTML(
      "<form id='form'>"
      "  <input type='text' id='field0' aria-label='inline aria label'>"
      "  <input type='text' id='field1' aria-labelledby='label'>"
      "  <input type='text' id='field2' aria-describedby='description'>"
      "</form>"
      "  <div id='label'>aria label</div>"
      "  <div id='description'>aria description</div>");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormElement web_form =
      frame->GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_FALSE(web_form.IsNull());

  WebFormControlElement control_element =
      frame->GetDocument().GetElementById("field0").To<WebFormControlElement>();
  ASSERT_FALSE(control_element.IsNull());
  FormData form;
  FormFieldData field;
  EXPECT_TRUE(WebFormElementToFormData(web_form, control_element, nullptr,
                                       /*extract_options=*/{}, &form, &field));

  const std::vector<FormFieldData>& fields = form.fields;
  ASSERT_EQ(3U, fields.size());

  // Field 0
  EXPECT_EQ(u"inline aria label", fields[0].aria_label);
  EXPECT_EQ(u"", fields[0].aria_description);

  // Field 1
  EXPECT_EQ(u"aria label", fields[1].aria_label);
  EXPECT_EQ(u"", fields[1].aria_description);

  // Field 2
  EXPECT_EQ(u"", fields[2].aria_label);
  EXPECT_EQ(u"aria description", fields[2].aria_description);
}

}  // namespace autofill::form_util
