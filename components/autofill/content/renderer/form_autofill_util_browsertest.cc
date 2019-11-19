// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_select_element.h"

using autofill::mojom::ButtonTitleType;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {
namespace form_util {
namespace {

struct AutofillFieldLabelSourceCase {
  const char* html;
  const FormFieldData::LabelSource label_source;
};

struct AutofillFieldUtilCase {
  const char* description;
  const char* html;
  const char* expected_label;
};

const char kElevenChildren[] =
    "<div id='target'>"
    "<div>child0</div>"
    "<div>child1</div>"
    "<div>child2</div>"
    "<div>child3</div>"
    "<div>child4</div>"
    "<div>child5</div>"
    "<div>child6</div>"
    "<div>child7</div>"
    "<div>child8</div>"
    "<div>child9</div>"
    "<div>child10</div>"
    "</div>";
const char kElevenChildrenExpected[] =
    "child0child1child2child3child4child5child6child7child8";

const char kElevenChildrenNested[] =
    "<div id='target'>"
    "<div>child0"
    "<div>child1"
    "<div>child2"
    "<div>child3"
    "<div>child4"
    "<div>child5"
    "<div>child6"
    "<div>child7"
    "<div>child8"
    "<div>child9"
    "<div>child10"
    "</div></div></div></div></div></div></div></div></div></div></div></div>";
// Take 10 elements -1 for target element, -1 as text is a leaf element.
const char kElevenChildrenNestedExpected[] = "child0child1child2child3child4";

const char kSkipElement[] =
    "<div id='target'>"
    "<div>child0</div>"
    "<div class='skip'>child1</div>"
    "<div>child2</div>"
    "</div>";
// TODO(crbug.com/796918): Should be child0child2
const char kSkipElementExpected[] = "child0";

const char kDivTableExample1[] =
    "<div>"
    "<div>label</div><div><input id='target'/></div>"
    "</div>";
const char kDivTableExample1Expected[] = "label";

const char kDivTableExample2[] =
    "<div>"
    "<div>label</div>"
    "<div>should be skipped<input/></div>"
    "<div><input id='target'/></div>"
    "</div>";
const char kDivTableExample2Expected[] = "label";

const char kDivTableExample3[] =
    "<div>"
    "<div>should be skipped<input/></div>"
    "<div>label</div>"
    "<div><input id='target'/></div>"
    "</div>";
const char kDivTableExample3Expected[] = "label";

const char kDivTableExample4[] =
    "<div>"
    "<div>should be skipped<input/></div>"
    "label"
    "<div><input id='target'/></div>"
    "</div>";
// TODO(crbug.com/796918): Should be label
const char kDivTableExample4Expected[] = "";

const char kDivTableExample5[] =
    "<div>"
    "<div>label<div><input id='target'/></div>behind</div>"
    "</div>";
// TODO(crbug.com/796918): Should be label
const char kDivTableExample5Expected[] = "labelbehind";

const char kDivTableExample6[] =
    "<div>"
    "<div>label<div><div>-<div><input id='target'/></div></div>"
    "</div>";
// TODO(crbug.com/796918): Should be "label" or "label-"
const char kDivTableExample6Expected[] = "";

class FormAutofillUtilsTest : public content::RenderViewTest {
 public:
  FormAutofillUtilsTest() {}
  ~FormAutofillUtilsTest() override {}
};

TEST_F(FormAutofillUtilsTest, FindChildTextTest) {
  static const AutofillFieldUtilCase test_cases[] = {
      {"simple test", "<div id='target'>test</div>", "test"},
      {"Concatenate test", "<div id='target'><span>one</span>two</div>",
       "onetwo"},
      // TODO(crbug.com/796918): should be "onetwo"
      {"Ignore input", "<div id='target'>one<input value='test'/>two</div>",
       "one"},
      {"Trim", "<div id='target'>   one<span>two  </span></div>", "onetwo"},
      {"eleven children", kElevenChildren, kElevenChildrenExpected},
      // TODO(crbug.com/796918): Depth is only 5 elements
      {"eleven children nested", kElevenChildrenNested,
       kElevenChildrenNestedExpected},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = web_frame->GetDocument().GetElementById("target");
    ASSERT_FALSE(target.IsNull());
    EXPECT_EQ(base::UTF8ToUTF16(test_case.expected_label),
              FindChildText(target));
  }
}

TEST_F(FormAutofillUtilsTest, FindChildTextSkipElementTest) {
  static const AutofillFieldUtilCase test_cases[] = {
      {"Skip div element", kSkipElement, kSkipElementExpected},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = web_frame->GetDocument().GetElementById("target");
    ASSERT_FALSE(target.IsNull());
    WebVector<WebElement> web_to_skip =
        web_frame->GetDocument().QuerySelectorAll("div[class='skip']");
    std::set<WebNode> to_skip;
    for (size_t i = 0; i < web_to_skip.size(); ++i) {
      to_skip.insert(web_to_skip[i]);
    }

    EXPECT_EQ(base::UTF8ToUTF16(test_case.expected_label),
              FindChildTextWithIgnoreListForTesting(target, to_skip));
  }
}

TEST_F(FormAutofillUtilsTest, InferLabelForElementTest) {
  std::vector<base::char16> stop_words;
  stop_words.push_back(static_cast<base::char16>('-'));
  static const AutofillFieldUtilCase test_cases[] = {
      {"DIV table test 1", kDivTableExample1, kDivTableExample1Expected},
      {"DIV table test 2", kDivTableExample2, kDivTableExample2Expected},
      {"DIV table test 3", kDivTableExample3, kDivTableExample3Expected},
      {"DIV table test 4", kDivTableExample4, kDivTableExample4Expected},
      {"DIV table test 5", kDivTableExample5, kDivTableExample5Expected},
      {"DIV table test 6", kDivTableExample6, kDivTableExample6Expected},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = web_frame->GetDocument().GetElementById("target");
    ASSERT_FALSE(target.IsNull());
    const WebFormControlElement form_target =
        target.ToConst<WebFormControlElement>();
    ASSERT_FALSE(form_target.IsNull());

    FormFieldData::LabelSource label_source =
        FormFieldData::LabelSource::kUnknown;
    base::string16 label;
    InferLabelForElementForTesting(form_target, stop_words, &label,
                                   &label_source);
    EXPECT_EQ(base::UTF8ToUTF16(test_case.expected_label), label);
  }
}

TEST_F(FormAutofillUtilsTest, InferLabelSourceTest) {
  const char kLabelSourceExpectedLabel[] = "label";
  static const AutofillFieldLabelSourceCase test_cases[] = {
      {"<div><div>label</div><div><input id='target'/></div></div>",
       FormFieldData::LabelSource::kDivTable},
      {"<label>label</label><input id='target'/>",
       FormFieldData::LabelSource::kLabelTag},
      {"<b>l</b><strong>a</strong>bel<input id='target'/>",
       FormFieldData::LabelSource::kCombined},
      {"<p><b>l</b><strong>a</strong>bel</p><input id='target'/>",
       FormFieldData::LabelSource::kPTag},
      {"<input id='target' placeholder='label'/>",
       FormFieldData::LabelSource::kPlaceHolder},
      {"<input id='target' aria-label='label'/>",
       FormFieldData::LabelSource::kAriaLabel},
      {"<input id='target' value='label'/>",
       FormFieldData::LabelSource::kValue},
      {"<li>label<div><input id='target'/></div></li>",
       FormFieldData::LabelSource::kLiTag},
      {"<table><tr><td>label</td><td><input id='target'/></td></tr></table>",
       FormFieldData::LabelSource::kTdTag},
      {"<dl><dt>label</dt><dd><input id='target'></dd></dl>",
       FormFieldData::LabelSource::kDdTag},
  };
  std::vector<base::char16> stop_words;
  stop_words.push_back(static_cast<base::char16>('-'));

  for (auto test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << test_case.label_source);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = web_frame->GetDocument().GetElementById("target");
    ASSERT_FALSE(target.IsNull());
    const WebFormControlElement form_target =
        target.ToConst<WebFormControlElement>();
    ASSERT_FALSE(form_target.IsNull());

    FormFieldData::LabelSource label_source =
        FormFieldData::LabelSource::kUnknown;
    base::string16 label;
    EXPECT_TRUE(autofill::form_util::InferLabelForElementForTesting(
        form_target, stop_words, &label, &label_source));
    EXPECT_EQ(base::UTF8ToUTF16(kLabelSourceExpectedLabel), label);
    EXPECT_EQ(test_case.label_source, label_source);
  }
}

TEST_F(FormAutofillUtilsTest, InferButtonTitleForFormTest) {
  const char kHtml[] =
      "<form id='target'>"
      "  <input type='button' value='Clear field'>"
      "  <input type='button' value='Clear field'>"
      "  <input type='button' value='Clear field'>"
      "  <input type='button' value='\n Show\t password '>"
      "  <button>Sign Up</button>"
      "  <button type='button'>Register</button>"
      "  <a id='Submit' value='Create account'>"
      "  <div name='BTN'> Join </div>"
      "  <span class='button'> Start </span>"
      "  <a class='empty button' value='   \t   \n'>"
      "</form>";

  LoadHTML(kHtml);
  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_NE(nullptr, web_frame);
  const WebElement& target = web_frame->GetDocument().GetElementById("target");
  ASSERT_FALSE(target.IsNull());
  const WebFormElement& form_target = target.ToConst<WebFormElement>();
  ASSERT_FALSE(form_target.IsNull());

  autofill::ButtonTitleList actual = InferButtonTitlesForTesting(form_target);
  autofill::ButtonTitleList expected = {
      {base::UTF8ToUTF16("Clear field"),
       ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE},
      {base::UTF8ToUTF16("Show password"),
       ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE},
      {base::UTF8ToUTF16("Sign Up"),
       ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE},
      {base::UTF8ToUTF16("Register"),
       ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE},
      {base::UTF8ToUTF16("Create account"), ButtonTitleType::HYPERLINK},
      {base::UTF8ToUTF16("Join"), ButtonTitleType::DIV},
      {base::UTF8ToUTF16("Start"), ButtonTitleType::SPAN}};
  EXPECT_EQ(expected, actual);
}

TEST_F(FormAutofillUtilsTest, InferButtonTitleForFormTest_TooLongTitle) {
  std::string title;
  for (int i = 0; i < 300; ++i)
    title += "a";
  std::string kFormHtml = "<form id='target'>";
  for (int i = 0; i < 10; i++) {
    std::string kFieldHtml =
        "<input type='button' value='" + base::NumberToString(i) + title + "'>";
    kFormHtml += kFieldHtml;
  }
  kFormHtml += "</form>";

  LoadHTML(kFormHtml.c_str());
  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_NE(nullptr, web_frame);
  const WebElement& target = web_frame->GetDocument().GetElementById("target");
  ASSERT_FALSE(target.IsNull());
  const WebFormElement& form_target = target.ToConst<WebFormElement>();
  ASSERT_FALSE(form_target.IsNull());

  autofill::ButtonTitleList actual = InferButtonTitlesForTesting(form_target);

  int total_length = 0;
  for (auto title : actual) {
    EXPECT_GE(30u, title.first.length());
    total_length += title.first.length();
  }
  EXPECT_EQ(200, total_length);
}

TEST_F(FormAutofillUtilsTest, InferButtonTitle_Formless) {
  const char kNoFormHtml[] =
      "<div class='reg-form'>"
      "  <input type='button' value='\n Show\t password '>"
      "  <button>Sign Up</button>"
      "  <button type='button'>Register</button>"
      "</div>"
      "<form id='ignored-form'>"
      "  <input type='button' value='Ignore this'>"
      "  <button>Ignore this</button>"
      "  <a id='Submit' value='Ignore this'>"
      "  <div name='BTN'>Ignore this</div>"
      "</form>";

  LoadHTML(kNoFormHtml);
  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_NE(nullptr, web_frame);
  const WebElement& body = web_frame->GetDocument().Body();
  ASSERT_FALSE(body.IsNull());

  autofill::ButtonTitleList actual = InferButtonTitlesForTesting(body);
  autofill::ButtonTitleList expected = {
      {base::UTF8ToUTF16("Show password"),
       ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE},
      {base::UTF8ToUTF16("Sign Up"),
       ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE},
      {base::UTF8ToUTF16("Register"),
       ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE}};
  EXPECT_EQ(expected, actual);
}

TEST_F(FormAutofillUtilsTest, IsEnabled) {
  LoadHTML(
      "<input type='text' id='name1'>"
      "<input type='password' disabled id='name2'>"
      "<input type='password' id='name3'>"
      "<input type='text' id='name4' disabled>");

  const std::vector<WebElement> dummy_fieldsets;

  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);
  std::vector<WebFormControlElement> control_elements;
  WebElementCollection inputs =
      web_frame->GetDocument().GetElementsByHTMLTagName("input");
  for (WebElement element = inputs.FirstItem(); !element.IsNull();
       element = inputs.NextItem()) {
    control_elements.push_back(element.To<WebFormControlElement>());
  }

  autofill::FormData target;
  EXPECT_TRUE(UnownedPasswordFormElementsAndFieldSetsToFormData(
      dummy_fieldsets, control_elements, nullptr, web_frame->GetDocument(),
      nullptr, EXTRACT_NONE, &target, nullptr));
  const struct {
    const char* const name;
    bool enabled;
  } kExpectedFields[] = {
      {"name1", true}, {"name2", false}, {"name3", true}, {"name4", false},
  };
  const size_t number_of_cases = base::size(kExpectedFields);
  ASSERT_EQ(number_of_cases, target.fields.size());
  for (size_t i = 0; i < number_of_cases; ++i) {
    EXPECT_EQ(base::UTF8ToUTF16(kExpectedFields[i].name),
              target.fields[i].name);
    EXPECT_EQ(kExpectedFields[i].enabled, target.fields[i].is_enabled);
  }
}

TEST_F(FormAutofillUtilsTest, IsReadonly) {
  LoadHTML(
      "<input type='text' id='name1'>"
      "<input readonly type='password' id='name2'>"
      "<input type='password' id='name3'>"
      "<input type='text' id='name4' readonly>");

  const std::vector<WebElement> dummy_fieldsets;

  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);
  std::vector<WebFormControlElement> control_elements;
  WebElementCollection inputs =
      web_frame->GetDocument().GetElementsByHTMLTagName("input");
  for (WebElement element = inputs.FirstItem(); !element.IsNull();
       element = inputs.NextItem()) {
    control_elements.push_back(element.To<WebFormControlElement>());
  }

  autofill::FormData target;
  EXPECT_TRUE(UnownedPasswordFormElementsAndFieldSetsToFormData(
      dummy_fieldsets, control_elements, nullptr, web_frame->GetDocument(),
      nullptr, EXTRACT_NONE, &target, nullptr));
  const struct {
    const char* const name;
    bool readonly;
  } kExpectedFields[] = {
      {"name1", false}, {"name2", true}, {"name3", false}, {"name4", true},
  };
  const size_t number_of_cases = base::size(kExpectedFields);
  ASSERT_EQ(number_of_cases, target.fields.size());
  for (size_t i = 0; i < number_of_cases; ++i) {
    EXPECT_EQ(base::UTF8ToUTF16(kExpectedFields[i].name),
              target.fields[i].name);
    EXPECT_EQ(kExpectedFields[i].readonly, target.fields[i].is_readonly);
  }
}

TEST_F(FormAutofillUtilsTest, IsFocusable) {
  LoadHTML(
      "<input type='text' id='name1' value='123'>"
      "<input type='text' id='name2' style='display:none'>");

  const std::vector<WebElement> dummy_fieldsets;

  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_TRUE(web_frame);

  std::vector<WebFormControlElement> control_elements;
  control_elements.push_back(web_frame->GetDocument()
                                 .GetElementById("name1")
                                 .To<WebFormControlElement>());
  control_elements.push_back(web_frame->GetDocument()
                                 .GetElementById("name2")
                                 .To<WebFormControlElement>());

  EXPECT_TRUE(autofill::form_util::IsWebElementVisible(control_elements[0]));
  EXPECT_FALSE(autofill::form_util::IsWebElementVisible(control_elements[1]));

  autofill::FormData target;
  EXPECT_TRUE(UnownedPasswordFormElementsAndFieldSetsToFormData(
      dummy_fieldsets, control_elements, nullptr, web_frame->GetDocument(),
      nullptr, EXTRACT_NONE, &target, nullptr));
  ASSERT_EQ(2u, target.fields.size());
  EXPECT_EQ(base::UTF8ToUTF16("name1"), target.fields[0].name);
  EXPECT_TRUE(target.fields[0].is_focusable);
  EXPECT_EQ(base::UTF8ToUTF16("name2"), target.fields[1].name);
  EXPECT_FALSE(target.fields[1].is_focusable);
}

TEST_F(FormAutofillUtilsTest, FindFormByUniqueId) {
  LoadHTML("<body><form id='form1'></form><form id='form2'></form></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  WebVector<WebFormElement> forms;
  doc.Forms(forms);

  for (const auto& form : forms) {
    EXPECT_EQ(form,
              FindFormByUniqueRendererId(doc, form.UniqueRendererFormId()));
  }

  // Expect null form element for non-existing form id.
  uint32_t non_existing_id = forms[0].UniqueRendererFormId() + 1000;
  EXPECT_TRUE(FindFormByUniqueRendererId(doc, non_existing_id).IsNull());
}

TEST_F(FormAutofillUtilsTest, FindFormControlByUniqueId) {
  LoadHTML(
      "<body><form id='form1'><input id='i1'></form><input id='i2'></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto input1 = doc.GetElementById("i1").To<WebInputElement>();
  auto input2 = doc.GetElementById("i2").To<WebInputElement>();
  uint32_t non_existing_id = input2.UniqueRendererFormControlId() + 1000;

  EXPECT_EQ(input1, FindFormControlElementByUniqueRendererId(
                        doc, input1.UniqueRendererFormControlId()));
  EXPECT_EQ(input2, FindFormControlElementByUniqueRendererId(
                        doc, input2.UniqueRendererFormControlId()));
  EXPECT_TRUE(
      FindFormControlElementByUniqueRendererId(doc, non_existing_id).IsNull());
}

TEST_F(FormAutofillUtilsTest, FindFormControlElementsByUniqueIdNoForm) {
  LoadHTML("<body><input id='i1'><input id='i2'><input id='i3'></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto input1 = doc.GetElementById("i1").To<WebInputElement>();
  auto input3 = doc.GetElementById("i3").To<WebInputElement>();
  uint32_t non_existing_id = input3.UniqueRendererFormControlId() + 1000;

  std::vector<uint32_t> renderer_ids = {input3.UniqueRendererFormControlId(),
                                        non_existing_id,
                                        input1.UniqueRendererFormControlId()};

  auto elements = FindFormControlElementsByUniqueRendererId(doc, renderer_ids);

  ASSERT_EQ(3u, elements.size());
  EXPECT_EQ(input3, elements[0]);
  EXPECT_TRUE(elements[1].IsNull());
  EXPECT_EQ(input1, elements[2]);
}

TEST_F(FormAutofillUtilsTest, FindFormControlElementsByUniqueIdWithForm) {
  LoadHTML(
      "<body><form id='f1'><input id='i1'><input id='i2'></form><input "
      "id='i3'></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto form = doc.GetElementById("f1").To<WebFormElement>();
  auto input1 = doc.GetElementById("i1").To<WebInputElement>();
  auto input3 = doc.GetElementById("i3").To<WebInputElement>();
  uint32_t non_existing_id = input3.UniqueRendererFormControlId() + 1000;

  std::vector<uint32_t> renderer_ids = {input3.UniqueRendererFormControlId(),
                                        non_existing_id,
                                        input1.UniqueRendererFormControlId()};

  auto elements = FindFormControlElementsByUniqueRendererId(
      doc, form.UniqueRendererFormId(), renderer_ids);

  // |input3| is not in the form, so it shouldn't be returned.
  ASSERT_EQ(3u, elements.size());
  EXPECT_TRUE(elements[0].IsNull());
  EXPECT_TRUE(elements[1].IsNull());
  EXPECT_EQ(input1, elements[2]);

  // Expect that no elements are retured for non existing form id.
  uint32_t non_existing_form_id = form.UniqueRendererFormId() + 1000;
  elements = FindFormControlElementsByUniqueRendererId(
      doc, non_existing_form_id, renderer_ids);
  ASSERT_EQ(3u, elements.size());
  EXPECT_TRUE(elements[0].IsNull());
  EXPECT_TRUE(elements[1].IsNull());
  EXPECT_TRUE(elements[2].IsNull());
}

// Tests the extraction of the aria-label attribute.
TEST_F(FormAutofillUtilsTest, GetAriaLabel) {
  LoadHTML("<input id='input' type='text' aria-label='the label'/>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = doc.GetElementById("input").To<WebInputElement>();
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element),
            base::UTF8ToUTF16("the label"));
}

// Tests that aria-labelledby works. Simple case: only one id referenced.
TEST_F(FormAutofillUtilsTest, GetAriaLabelledBySingle) {
  LoadHTML(
      "<div id='billing'>Billing</div>"
      "<div>"
      "    <div id='name'>Name</div>"
      "    <input id='input' type='text' aria-labelledby='name'/>"
      "</div>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = doc.GetElementById("input").To<WebInputElement>();
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element),
            base::UTF8ToUTF16("Name"));
}

// Tests that aria-labelledby works: Complex case: multiple ids referenced.
TEST_F(FormAutofillUtilsTest, GetAriaLabelledByMulti) {
  LoadHTML(
      "<div id='billing'>Billing</div>"
      "<div>"
      "    <div id='name'>Name</div>"
      "    <input id='input' type='text' aria-labelledby='billing name'/>"
      "</div>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = doc.GetElementById("input").To<WebInputElement>();
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element),
            base::UTF8ToUTF16("Billing Name"));
}

// Tests that aria-labelledby takes precedence over aria-label
TEST_F(FormAutofillUtilsTest, GetAriaLabelledByTakesPrecedence) {
  LoadHTML(
      "<div id='billing'>Billing</div>"
      "<div>"
      "    <div id='name'>Name</div>"
      "    <input id='input' type='text' aria-label='ignored' "
      "         aria-labelledby='name'/>"
      "</div>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = doc.GetElementById("input").To<WebInputElement>();
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element),
            base::UTF8ToUTF16("Name"));
}

// Tests that an invalid aria-labelledby reference gets ignored (as opposed to
// crashing, for example).
TEST_F(FormAutofillUtilsTest, GetAriaLabelledByInvalid) {
  LoadHTML(
      "<div id='billing'>Billing</div>"
      "<div>"
      "    <div id='name'>Name</div>"
      "    <input id='input' type='text' aria-labelledby='div1 div2'/>"
      "</div>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = doc.GetElementById("input").To<WebInputElement>();
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element),
            base::UTF8ToUTF16(""));
}

// Tests that invalid aria-labelledby references fall back to aria-label.
TEST_F(FormAutofillUtilsTest, GetAriaLabelledByFallback) {
  LoadHTML(
      "<div id='billing'>Billing</div>"
      "<div>"
      "    <div id='name'>Name</div>"
      "    <input id='input' type='text' aria-label='valid' "
      "          aria-labelledby='div1 div2'/>"
      "</div>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = doc.GetElementById("input").To<WebInputElement>();
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element),
            base::UTF8ToUTF16("valid"));
}

// Tests that aria-describedby works: Simple case: a single id referenced.
TEST_F(FormAutofillUtilsTest, GetAriaDescribedBySingle) {
  LoadHTML(
      "<input id='input' type='text' aria-describedby='div1'/>"
      "<div id='div1'>aria description</div>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = doc.GetElementById("input").To<WebInputElement>();
  EXPECT_EQ(autofill::form_util::GetAriaDescription(doc, element),
            base::UTF8ToUTF16("aria description"));
}

// Tests that aria-describedby works: Complex case: multiple ids referenced.
TEST_F(FormAutofillUtilsTest, GetAriaDescribedByMulti) {
  LoadHTML(
      "<input id='input' type='text' aria-describedby='div1 div2'/>"
      "<div id='div2'>description</div>"
      "<div id='div1'>aria</div>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = doc.GetElementById("input").To<WebInputElement>();
  EXPECT_EQ(autofill::form_util::GetAriaDescription(doc, element),
            base::UTF8ToUTF16("aria description"));
}

// Tests that invalid aria-describedby returns the empty string.
TEST_F(FormAutofillUtilsTest, GetAriaDescribedByInvalid) {
  LoadHTML("<input id='input' type='text' aria-describedby='invalid'/>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = doc.GetElementById("input").To<WebInputElement>();
  EXPECT_EQ(autofill::form_util::GetAriaDescription(doc, element),
            base::UTF8ToUTF16(""));
}

TEST_F(FormAutofillUtilsTest, IsFormVisible) {
  LoadHTML("<body><form id='form1'><input id='i1'></form></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto form = doc.GetElementById("form1").To<WebFormElement>();
  uint32_t form_id = form.UniqueRendererFormId();

  EXPECT_TRUE(autofill::form_util::IsFormVisible(GetMainFrame(), form_id));

  // Hide a form.
  form.SetAttribute("style", "display:none");
  EXPECT_FALSE(autofill::form_util::IsFormVisible(GetMainFrame(), form_id));
}

TEST_F(FormAutofillUtilsTest, IsFormControlVisible) {
  LoadHTML("<body><input id='input1'></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto input = doc.GetElementById("input1").To<WebFormControlElement>();
  uint32_t input_id = input.UniqueRendererFormControlId();

  EXPECT_TRUE(IsFormControlVisible(GetMainFrame(), input_id));

  // Hide a field.
  input.SetAttribute("style", "display:none");
  EXPECT_FALSE(autofill::form_util::IsFormVisible(GetMainFrame(), input_id));
}

TEST_F(FormAutofillUtilsTest, IsActionEmptyFalse) {
  LoadHTML(
      "<body><form id='form1' action='done.html'><input "
      "id='i1'></form></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_form = doc.GetElementById("form1").To<WebFormElement>();

  FormData form_data;
  ASSERT_TRUE(WebFormElementToFormData(
      web_form, WebFormControlElement(), nullptr /*field_data_manager*/,
      EXTRACT_VALUE, &form_data, nullptr /* FormFieldData */));

  EXPECT_FALSE(form_data.is_action_empty);
}

TEST_F(FormAutofillUtilsTest, IsActionEmptyTrue) {
  LoadHTML("<body><form id='form1'><input id='i1'></form></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_form = doc.GetElementById("form1").To<WebFormElement>();

  FormData form_data;
  ASSERT_TRUE(WebFormElementToFormData(
      web_form, WebFormControlElement(), nullptr /*field_data_manager*/,
      EXTRACT_VALUE, &form_data, nullptr /* FormFieldData */));

  EXPECT_TRUE(form_data.is_action_empty);
}

}  // namespace
}  // namespace form_util
}  // namespace autofill
