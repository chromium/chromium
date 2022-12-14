// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "third_party/blink/public/web/web_view.h"

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
using ::testing::ElementsAre;
using ::testing::Values;

namespace autofill {
namespace form_util {
namespace {

struct AutofillFieldUtilCase {
  const char* description;
  const char* html;
  const char16_t* expected_label;
};

// An <input> with a label placed on top of it (usually used as a placeholder
// replacement).
const char* kPoorMansPlaceholder = R"(
  <style>
    .fixed_position_and_size {
      position: fixed;
      top: 0;
      left: 0;
      width: 100px;
      height: 20px;
    }
  </style>
  <input id='target' class=fixed_position_and_size>
  <span class=fixed_position_and_size>label</span>
)";

void VerifyButtonTitleCache(const WebFormElement& form_target,
                            const ButtonTitleList& expected_button_titles,
                            const ButtonTitlesCache& actual_cache) {
  EXPECT_THAT(actual_cache,
              testing::ElementsAre(testing::Pair(GetFormRendererId(form_target),
                                                 expected_button_titles)));
}

class FormAutofillUtilsTest : public content::RenderViewTest {
 public:
  FormAutofillUtilsTest() {}
  ~FormAutofillUtilsTest() override {}
};

class FormAutofillUtilsTestWithIframesEnabled : public FormAutofillUtilsTest {
 public:
  FormAutofillUtilsTestWithIframesEnabled() {
    scoped_feature_list_.InitAndEnableFeature(features::kAutofillAcrossIframes);
  }
  ~FormAutofillUtilsTestWithIframesEnabled() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FormAutofillUtilsTest, FindChildTextTest) {
  static const AutofillFieldUtilCase test_cases[] = {
      {"simple test", "<div id='target'>test</div>", u"test"},
      {"Concatenate test", "<div id='target'><span>one</span>two</div>",
       u"onetwo"},
      // Test that "two" is not inferred, because for the purpose of label
      // extraction, we only care about text before the input element.
      {"Ignore input", "<div id='target'>one<input value='test'/>two</div>",
       u"one"},
      {"Trim", "<div id='target'>   one<span>two  </span></div>", u"onetwo"},
      {"eleven children",
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
       "<div>child10</div>",
       u"child0child1child2child3child4child5child6child7child8"},
      // TODO(crbug.com/796918): Depth is only 5 elements instead of 10. This
      // happens because every div and every text node decrease the depth.
      {"eleven children nested",
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
       "</div></div></div></div></div></div></div></div></div></div></div></"
       "div>",
       u"child0child1child2child3child4"},
      {"Skip script tags",
       "<div id='target'><script>alert('hello');</script>label</div>",
       u"label"},
      {"Script tag whitespacing",
       "<div id='target'>Auto<script>alert('hello');</script>fill</div>",
       u"Autofill"}};
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = GetElementById(web_frame->GetDocument(), "target");
    EXPECT_EQ(test_case.expected_label, FindChildText(target));
  }
}

TEST_F(FormAutofillUtilsTest, FindChildTextSkipElementTest) {
  static const AutofillFieldUtilCase test_cases[] = {
      // Test that everything after the "skip" div is discarded.
      {"Skip div element", R"(
       <div id=target>
         <div>child0</div>
         <div class=skip>child1</div>
         <div>child2</div>
       </div>)",
       u"child0"},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebElement target = GetElementById(web_frame->GetDocument(), "target");
    WebVector<WebElement> web_to_skip =
        web_frame->GetDocument().QuerySelectorAll("div[class='skip']");
    std::set<WebNode> to_skip;
    for (size_t i = 0; i < web_to_skip.size(); ++i) {
      to_skip.insert(web_to_skip[i]);
    }

    EXPECT_EQ(test_case.expected_label,
              FindChildTextWithIgnoreListForTesting(target, to_skip));
  }
}

TEST_F(FormAutofillUtilsTest, InferLabelForElementTest) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillSupportPoorMansPlaceholder);

  static const AutofillFieldUtilCase test_cases[] = {
      {"DIV table test 1", R"(
       <div>
         <div>label</div><div><input id=target></div>
       </div>)",
       u"label"},
      {"DIV table test 2", R"(
       <div>
         <div>label</div>
         <div>should be skipped<input></div>
         <div><input id=target></div>
       </div>)",
       u"label"},
      {"DIV table test 3", R"(
       <div>
         <div>should be skipped<input></div>
         <div>label</div>
         <div><input id=target></div>
       </div>)",
       u"label"},
      {"DIV table test 4", R"(
       <div>
         <div>should be skipped<input></div>
         label
         <div><input id=target></div>
       </div>)",
       u"label"},
      {"DIV table test 5",
       "<div>"
       "<div>label<div><input id='target'/></div>behind</div>"
       "</div>",
       u"label"},
      {"DIV table test 6", R"(
       <div>
         label
         <div>*</div>
         <div><input id='target'></div>
       </div>)",
       // TODO(crbug.com/796918): Should be "label" or "label*". This happens
       // because "*" is inferred, but discarded because `!IsLabelValid()`.
       u""},
      {"Infer from next sibling",
       "<input id='target' type='checkbox'>hello <b>world</b>", u"hello world"},
      {"Poor man's placeholder", kPoorMansPlaceholder, u"label"},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebFormControlElement form_target =
        GetFormControlElementById(web_frame->GetDocument(), "target");

    FormFieldData::LabelSource label_source =
        FormFieldData::LabelSource::kUnknown;
    std::u16string label;
    InferLabelForElementForTesting(form_target, label, label_source);
    EXPECT_EQ(test_case.expected_label, label);
  }
}

TEST_F(FormAutofillUtilsTest, InferLabelSourceTest) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillSupportPoorMansPlaceholder);

  struct AutofillFieldLabelSourceCase {
    const char* html;
    const FormFieldData::LabelSource label_source;
  };
  const char16_t kLabelSourceExpectedLabel[] = u"label";
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
      // In the next test, the text node is picked up on the way up the DOM-tree
      // by the div extraction logic.
      {"<li>label<div><input id='target'/></div></li>",
       FormFieldData::LabelSource::kDivTable},
      {"<li><span>label</span><div><input id='target'/></div></li>",
       FormFieldData::LabelSource::kLiTag},
      {"<table><tr><td>label</td><td><input id='target'/></td></tr></table>",
       FormFieldData::LabelSource::kTdTag},
      {"<dl><dt>label</dt><dd><input id='target'></dd></dl>",
       FormFieldData::LabelSource::kDdTag},
      {kPoorMansPlaceholder, FormFieldData::LabelSource::kOverlayingLabel}};

  for (auto test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << test_case.label_source);
    LoadHTML(test_case.html);
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_NE(nullptr, web_frame);
    WebFormControlElement form_target =
        GetFormControlElementById(web_frame->GetDocument(), "target");

    FormFieldData::LabelSource label_source =
        FormFieldData::LabelSource::kUnknown;
    std::u16string label;
    EXPECT_TRUE(autofill::form_util::InferLabelForElementForTesting(
        form_target, label, label_source));
    EXPECT_EQ(kLabelSourceExpectedLabel, label);
    EXPECT_EQ(test_case.label_source, label_source);
  }
}

TEST_F(FormAutofillUtilsTest, GetButtonTitles) {
  constexpr char kHtml[] =
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
  WebFormElement form_target =
      GetFormElementById(web_frame->GetDocument(), "target");
  ButtonTitlesCache cache;

  autofill::ButtonTitleList actual =
      GetButtonTitles(form_target, web_frame->GetDocument(), &cache);

  autofill::ButtonTitleList expected = {
      {u"Clear field", ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE},
      {u"Show password", ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE},
      {u"Sign Up", ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE},
      {u"Register", ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE},
      {u"Create account", ButtonTitleType::HYPERLINK},
      {u"Join", ButtonTitleType::DIV},
      {u"Start", ButtonTitleType::SPAN}};
  EXPECT_EQ(expected, actual);

  VerifyButtonTitleCache(form_target, expected, cache);
}

TEST_F(FormAutofillUtilsTest, GetButtonTitles_TooLongTitle) {
  std::string kFormHtml = "<form id='target'>";
  for (int i = 0; i < 10; i++) {
    std::string kFieldHtml = "<input type='button' value='" +
                             base::NumberToString(i) + std::string(300, 'a') +
                             "'>";
    kFormHtml += kFieldHtml;
  }
  kFormHtml += "</form>";

  LoadHTML(kFormHtml.c_str());
  WebLocalFrame* web_frame = GetMainFrame();
  ASSERT_NE(nullptr, web_frame);
  WebFormElement form_target =
      GetFormElementById(web_frame->GetDocument(), "target");
  ButtonTitlesCache cache;

  autofill::ButtonTitleList actual =
      GetButtonTitles(form_target, web_frame->GetDocument(), &cache);

  int total_length = 0;
  for (const auto& [title, title_type] : actual) {
    EXPECT_GE(30u, title.length());
    total_length += title.length();
  }
  EXPECT_EQ(200, total_length);
}

TEST_F(FormAutofillUtilsTest, GetButtonTitles_Formless) {
  constexpr char kNoFormHtml[] =
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
  WebFormElement form_target;
  ASSERT_FALSE(web_frame->GetDocument().Body().IsNull());
  ButtonTitlesCache cache;

  autofill::ButtonTitleList actual =
      GetButtonTitles(form_target, web_frame->GetDocument(), &cache);
  autofill::ButtonTitleList expected = {
      {u"Show password", ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE},
      {u"Sign Up", ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE},
      {u"Register", ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE}};
  EXPECT_EQ(expected, actual);

  VerifyButtonTitleCache(form_target, expected, cache);
}

TEST_F(FormAutofillUtilsTest, GetButtonTitles_DisabledIfNoCache) {
  // Button titles scraping for unowned forms can be time-consuming and disabled
  // in Beta and Stable. To disable button titles computation, |buttons_cache|
  // should be null.
  constexpr char kNoFormHtml[] =
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
  WebFormElement form_target;
  ASSERT_FALSE(web_frame->GetDocument().Body().IsNull());

  autofill::ButtonTitleList actual =
      GetButtonTitles(form_target, web_frame->GetDocument(), nullptr);

  EXPECT_TRUE(actual.empty());
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

  std::vector<WebElement> iframe_elements;

  autofill::FormData target;
  EXPECT_TRUE(UnownedFormElementsAndFieldSetsToFormData(
      dummy_fieldsets, control_elements, iframe_elements, nullptr,
      web_frame->GetDocument(), nullptr, EXTRACT_NONE, &target, nullptr));
  const struct {
    const char16_t* const name;
    bool enabled;
  } kExpectedFields[] = {
      {u"name1", true},
      {u"name2", false},
      {u"name3", true},
      {u"name4", false},
  };
  const size_t number_of_cases = std::size(kExpectedFields);
  ASSERT_EQ(number_of_cases, target.fields.size());
  for (size_t i = 0; i < number_of_cases; ++i) {
    EXPECT_EQ(kExpectedFields[i].name, target.fields[i].name);
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

  std::vector<WebElement> iframe_elements;

  autofill::FormData target;
  EXPECT_TRUE(UnownedFormElementsAndFieldSetsToFormData(
      dummy_fieldsets, control_elements, iframe_elements, nullptr,
      web_frame->GetDocument(), nullptr, EXTRACT_NONE, &target, nullptr));
  const struct {
    const char16_t* const name;
    bool readonly;
  } kExpectedFields[] = {
      {u"name1", false},
      {u"name2", true},
      {u"name3", false},
      {u"name4", true},
  };
  const size_t number_of_cases = std::size(kExpectedFields);
  ASSERT_EQ(number_of_cases, target.fields.size());
  for (size_t i = 0; i < number_of_cases; ++i) {
    EXPECT_EQ(kExpectedFields[i].name, target.fields[i].name);
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
  control_elements.push_back(
      GetFormControlElementById(web_frame->GetDocument(), "name1"));
  control_elements.push_back(
      GetFormControlElementById(web_frame->GetDocument(), "name2"));

  EXPECT_TRUE(autofill::form_util::IsWebElementFocusable(control_elements[0]));
  EXPECT_FALSE(autofill::form_util::IsWebElementFocusable(control_elements[1]));

  std::vector<WebElement> iframe_elements;

  autofill::FormData target;
  EXPECT_TRUE(UnownedFormElementsAndFieldSetsToFormData(
      dummy_fieldsets, control_elements, iframe_elements, nullptr,
      web_frame->GetDocument(), nullptr, EXTRACT_NONE, &target, nullptr));
  ASSERT_EQ(2u, target.fields.size());
  EXPECT_EQ(u"name1", target.fields[0].name);
  EXPECT_TRUE(target.fields[0].is_focusable);
  EXPECT_EQ(u"name2", target.fields[1].name);
  EXPECT_FALSE(target.fields[1].is_focusable);
}

TEST_F(FormAutofillUtilsTest, FindFormByUniqueId) {
  LoadHTML("<body><form id='form1'></form><form id='form2'></form></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  WebVector<WebFormElement> forms = doc.Forms();

  for (const auto& form : forms)
    EXPECT_EQ(form, FindFormByUniqueRendererId(doc, GetFormRendererId(form)));

  // Expect null form element for non-existing form id.
  FormRendererId non_existing_id(forms[0].UniqueRendererFormId() + 1000);
  EXPECT_TRUE(FindFormByUniqueRendererId(doc, non_existing_id).IsNull());
}

// Used in ParameterizedFindFormControlByRendererIdTest.
struct FindFormControlTestParam {
  std::string queried_field;
  absl::optional<std::string> form_to_be_searched;
  bool expectation;
};

// Tests FindFormControlElementByUniqueRendererId().
class ParameterizedFindFormControlByRendererIdTest
    : public FormAutofillUtilsTest,
      public testing::WithParamInterface<FindFormControlTestParam> {};

TEST_P(ParameterizedFindFormControlByRendererIdTest,
       FindFormControlElementByUniqueRendererId) {
  LoadHTML(R"(
    <body>
      <input id="nonexistentField">
      <form id="form1"><input id="ownedField1"></form>
      <form id="form2"><input id="ownedField2"></form>
      <input id="unownedField">
    </body>
  )");
  WebDocument doc = GetMainFrame()->GetDocument();

  absl::optional<FormRendererId> form_to_be_searched_id;
  if (GetParam().form_to_be_searched.has_value()) {
    if (GetParam().form_to_be_searched.value().empty()) {
      // Only the unowned form will be searched.
      form_to_be_searched_id = FormRendererId();
    } else {
      // Only the given form will be searched.
      form_to_be_searched_id = GetFormRendererId(
          GetFormElementById(doc, GetParam().form_to_be_searched.value()));
    }
  }

  WebFormControlElement queried_field =
      GetFormControlElementById(doc, GetParam().queried_field);
  FieldRendererId queried_field_id = GetFieldRendererId(queried_field);

  ExecuteJavaScriptForTests(
      R"(document.getElementById('nonexistentField').remove();)");
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(
      GetParam().expectation,
      queried_field == FindFormControlElementByUniqueRendererId(
                           doc, queried_field_id, form_to_be_searched_id));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ParameterizedFindFormControlByRendererIdTest,
    Values(FindFormControlTestParam{"nonexistentField", absl::nullopt, false},
           FindFormControlTestParam{"nonexistentField", std::string(), false},
           FindFormControlTestParam{"nonexistentField", "form1", false},
           FindFormControlTestParam{"nonexistentField", "form2", false},
           FindFormControlTestParam{"ownedField1", absl::nullopt, true},
           FindFormControlTestParam{"ownedField1", std::string(), false},
           FindFormControlTestParam{"ownedField1", "form1", true},
           FindFormControlTestParam{"ownedField1", "form2", false},
           FindFormControlTestParam{"ownedField2", absl::nullopt, true},
           FindFormControlTestParam{"ownedField2", std::string(), false},
           FindFormControlTestParam{"ownedField2", "form1", false},
           FindFormControlTestParam{"ownedField2", "form2", true},
           FindFormControlTestParam{"unownedField", absl::nullopt, true},
           FindFormControlTestParam{"unownedField", std::string(), true},
           FindFormControlTestParam{"unownedField", "form1", false},
           FindFormControlTestParam{"unownedField", "form2", false}));

TEST_F(FormAutofillUtilsTest, FindFormControlElementsByUniqueIdNoForm) {
  LoadHTML("<body><input id='i1'><input id='i2'><input id='i3'></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto input1 = GetFormControlElementById(doc, "i1");
  auto input3 = GetFormControlElementById(doc, "i3");
  FieldRendererId non_existing_id(input3.UniqueRendererFormControlId() + 1000);

  std::vector<FieldRendererId> renderer_ids = {
      GetFieldRendererId(input3), non_existing_id, GetFieldRendererId(input1)};

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
  auto form = GetFormElementById(doc, "f1");
  auto input1 = GetFormControlElementById(doc, "i1");
  auto input3 = GetFormControlElementById(doc, "i3");
  FieldRendererId non_existing_id(input3.UniqueRendererFormControlId() + 1000);

  std::vector<FieldRendererId> renderer_ids = {
      GetFieldRendererId(input3), non_existing_id, GetFieldRendererId(input1)};

  auto elements = FindFormControlElementsByUniqueRendererId(
      doc, GetFormRendererId(form), renderer_ids);

  // |input3| is not in the form, so it shouldn't be returned.
  ASSERT_EQ(3u, elements.size());
  EXPECT_TRUE(elements[0].IsNull());
  EXPECT_TRUE(elements[1].IsNull());
  EXPECT_EQ(input1, elements[2]);

  // Expect that no elements are returned for non existing form id.
  FormRendererId non_existing_form_id(form.UniqueRendererFormId() + 1000);
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
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element), u"the label");
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
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element), u"Name");
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
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element), u"Billing Name");
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
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element), u"Name");
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
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element), u"");
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
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(autofill::form_util::GetAriaLabel(doc, element), u"valid");
}

// Tests that aria-describedby works: Simple case: a single id referenced.
TEST_F(FormAutofillUtilsTest, GetAriaDescribedBySingle) {
  LoadHTML(
      "<input id='input' type='text' aria-describedby='div1'/>"
      "<div id='div1'>aria description</div>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(autofill::form_util::GetAriaDescription(doc, element),
            u"aria description");
}

// Tests that aria-describedby works: Complex case: multiple ids referenced.
TEST_F(FormAutofillUtilsTest, GetAriaDescribedByMulti) {
  LoadHTML(
      "<input id='input' type='text' aria-describedby='div1 div2'/>"
      "<div id='div2'>description</div>"
      "<div id='div1'>aria</div>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(autofill::form_util::GetAriaDescription(doc, element),
            u"aria description");
}

// Tests that invalid aria-describedby returns the empty string.
TEST_F(FormAutofillUtilsTest, GetAriaDescribedByInvalid) {
  LoadHTML("<input id='input' type='text' aria-describedby='invalid'/>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(autofill::form_util::GetAriaDescription(doc, element), u"");
}

// Tests IsOwnedByFrame().
TEST_F(FormAutofillUtilsTestWithIframesEnabled, IsOwnedByFrame) {
  LoadHTML(R"(
    <body>
      <div id="div"></div>
      <iframe id="child_frame"></iframe>
    </body>
  )");

  WebDocument doc = GetMainFrame()->GetDocument();
  content::RenderFrame* main_frame = GetMainRenderFrame();
  content::RenderFrame* child_frame = GetIframeById(doc, "child_frame");
  WebElement div = GetElementById(doc, "div");

  EXPECT_FALSE(IsOwnedByFrame(WebElement(), nullptr));
  EXPECT_FALSE(IsOwnedByFrame(WebElement(), main_frame));
  EXPECT_FALSE(IsOwnedByFrame(div, nullptr));
  EXPECT_FALSE(IsOwnedByFrame(div, child_frame));
  EXPECT_TRUE(IsOwnedByFrame(div, main_frame));
  ExecuteJavaScriptForTests(R"(document.getElementById('div').remove();)");
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(IsOwnedByFrame(div, main_frame));
}

TEST_F(FormAutofillUtilsTest, IsActionEmptyFalse) {
  LoadHTML(
      "<body><form id='form1' action='done.html'><input "
      "id='i1'></form></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_form = GetFormElementById(doc, "form1");

  FormData form_data;
  ASSERT_TRUE(WebFormElementToFormData(
      web_form, WebFormControlElement(), nullptr /*field_data_manager*/,
      EXTRACT_VALUE, &form_data, nullptr /* FormFieldData */));

  EXPECT_FALSE(form_data.is_action_empty);
}

TEST_F(FormAutofillUtilsTest, IsActionEmptyTrue) {
  LoadHTML("<body><form id='form1'><input id='i1'></form></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_form = GetFormElementById(doc, "form1");

  FormData form_data;
  ASSERT_TRUE(WebFormElementToFormData(
      web_form, WebFormControlElement(), nullptr /*field_data_manager*/,
      EXTRACT_VALUE, &form_data, nullptr /* FormFieldData */));

  EXPECT_TRUE(form_data.is_action_empty);
}

TEST_F(FormAutofillUtilsTest, ExtractBounds) {
  LoadHTML("<body><form id='form1'><input id='i1'></form></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_control = GetFormControlElementById(doc, "i1");

  FormData form_data;
  ASSERT_TRUE(FindFormAndFieldForFormControlElement(
      web_control, nullptr /*field_data_manager*/, EXTRACT_BOUNDS, &form_data,
      nullptr /* FormFieldData */));

  EXPECT_FALSE(form_data.fields.back().bounds.IsEmpty());
}

TEST_F(FormAutofillUtilsTest, NotExtractBounds) {
  LoadHTML("<body><form id='form1'><input id='i1'></form></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_control = GetFormControlElementById(doc, "i1");

  FormData form_data;
  ASSERT_TRUE(FindFormAndFieldForFormControlElement(
      web_control, nullptr /*field_data_manager*/, &form_data,
      nullptr /* FormFieldData */));

  EXPECT_TRUE(form_data.fields.back().bounds.IsEmpty());
}

TEST_F(FormAutofillUtilsTest, ExtractUnownedBounds) {
  LoadHTML("<body><input id='i1'></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_control = GetFormControlElementById(doc, "i1");

  FormData form_data;
  ASSERT_TRUE(FindFormAndFieldForFormControlElement(
      web_control, nullptr /*field_data_manager*/, EXTRACT_BOUNDS, &form_data,
      nullptr /* FormFieldData */));

  EXPECT_FALSE(form_data.fields.back().bounds.IsEmpty());
}

TEST_F(FormAutofillUtilsTest, GetDataListSuggestions) {
  LoadHTML(
      "<body><input list='datalist_id' name='count' id='i1'><datalist "
      "id='datalist_id'><option value='1'><option "
      "value='2'></datalist></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_control = GetElementById(doc, "i1").To<WebInputElement>();
  std::vector<std::u16string> values;
  std::vector<std::u16string> labels;
  GetDataListSuggestions(web_control, &values, &labels);
  ASSERT_EQ(values.size(), 2u);
  ASSERT_EQ(labels.size(), 2u);
  EXPECT_EQ(values[0], u"1");
  EXPECT_EQ(values[1], u"2");
  EXPECT_EQ(labels[0], u"");
  EXPECT_EQ(labels[1], u"");
}

TEST_F(FormAutofillUtilsTest, GetDataListSuggestionsWithLabels) {
  LoadHTML(
      "<body><input list='datalist_id' name='count' id='i1'><datalist "
      "id='datalist_id'><option value='1'>one</option><option "
      "value='2'>two</option></datalist></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_control = GetElementById(doc, "i1").To<WebInputElement>();
  std::vector<std::u16string> values;
  std::vector<std::u16string> labels;
  GetDataListSuggestions(web_control, &values, &labels);
  ASSERT_EQ(values.size(), 2u);
  ASSERT_EQ(labels.size(), 2u);
  EXPECT_EQ(values[0], u"1");
  EXPECT_EQ(values[1], u"2");
  EXPECT_EQ(labels[0], u"one");
  EXPECT_EQ(labels[1], u"two");
}

TEST_F(FormAutofillUtilsTest, ExtractDataList) {
  LoadHTML(
      "<body><input list='datalist_id' name='count' id='i1'><datalist "
      "id='datalist_id'><option value='1'>one</option><option "
      "value='2'>two</option></datalist></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_control = GetElementById(doc, "i1").To<WebInputElement>();
  FormData form_data;
  FormFieldData form_field_data;
  ASSERT_TRUE(FindFormAndFieldForFormControlElement(
      web_control, nullptr /*field_data_manager*/, EXTRACT_DATALIST, &form_data,
      &form_field_data));

  auto& values = form_data.fields.back().datalist_values;
  auto& labels = form_data.fields.back().datalist_labels;
  ASSERT_EQ(values.size(), 2u);
  ASSERT_EQ(labels.size(), 2u);
  EXPECT_EQ(values[0], u"1");
  EXPECT_EQ(values[1], u"2");
  EXPECT_EQ(labels[0], u"one");
  EXPECT_EQ(labels[1], u"two");
  EXPECT_EQ(form_field_data.datalist_values, values);
  EXPECT_EQ(form_field_data.datalist_labels, labels);
}

TEST_F(FormAutofillUtilsTest, NotExtractDataList) {
  LoadHTML(
      "<body><input list='datalist_id' name='count' id='i1'><datalist "
      "id='datalist_id'><option value='1'>one</option><option "
      "value='2'>two</option></datalist></body>");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto web_control = GetElementById(doc, "i1").To<WebInputElement>();
  FormData form_data;
  FormFieldData form_field_data;
  ASSERT_TRUE(FindFormAndFieldForFormControlElement(
      web_control, nullptr /*field_data_manager*/, &form_data,
      &form_field_data));

  EXPECT_TRUE(form_data.fields.back().datalist_values.empty());
  EXPECT_TRUE(form_data.fields.back().datalist_labels.empty());
}

// Tests the visibility detection of iframes.
// This test checks many scenarios. It's intentionally not a parameterized test
// for performance reasons.
// This test is very similar to the IsWebElementVisibleTest test.
TEST_F(FormAutofillUtilsTest, IsVisibleIframeTest) {
  // Test cases of <iframe> elements with different styles.
  //
  // The `data-[in]visible` attribute represents whether IsVisibleIframe()
  // is expected to classify the iframe as [in]visible.
  //
  // Since IsVisibleIframe() falls short of what the human user will consider
  // visible or invisible, there are false positives and false negatives. For
  // example, IsVisibleIframe() does not check opacity, so <iframe
  // style="opacity: 0.0"> is a false positive (it's visible to
  // IsVisibleIframe() but invisible to the human).
  //
  // The `data-false="{POSITIVE,NEGATIVE}"` attribute indicates whether the test
  // case is a false positive/negative compared to human visibility perception.
  // In such a case, not meeting the expectation actually indicates an
  // improvement of IsVisibleIframe(), as it means a false positive/negative has
  // been fixed.
  //
  // The sole purpose of the `data-false` attribute is to document this and to
  // print a message when such a test fails.
  LoadHTML(R"(
      <body>
        <iframe srcdoc="<input>" data-visible   style=""></iframe>
        <iframe srcdoc="<input>" data-visible   style="display: block;"></iframe>
        <iframe srcdoc="<input>" data-visible   style="visibility: visible;"></iframe>

        <iframe srcdoc="<input>" data-invisible style="display: none;"></iframe>
        <iframe srcdoc="<input>" data-invisible style="visibility: hidden;"></iframe>
        <div style="display: none;">     <iframe srcdoc="<input>" data-invisible></iframe></div>
        <div style="visibility: hidden;"><iframe srcdoc="<input>" data-invisible></iframe></div>

        <iframe srcdoc="<input>" data-visible   style="width: 15px; height: 15px;"></iframe>
        <iframe srcdoc="<input>" data-invisible style="width: 15px; height:  5px;"></iframe>
        <iframe srcdoc="<input>" data-invisible style="width:  5px; height: 15px;"></iframe>
        <iframe srcdoc="<input>" data-invisible style="width:  5px; height:  5px;"></iframe>

        <iframe srcdoc="<input>" data-invisible style="width: 1px; height: 1px;"></iframe>
        <iframe srcdoc="<input>" data-invisible style="width: 1px; height: 1px; overflow: visible;" data-false="NEGATIVE"></iframe>

        <iframe srcdoc="<input>" data-visible   style="opacity: 0.0;" data-false="POSITIVE"></iframe>
        <iframe srcdoc="<input>" data-visible   style="opacity: 0.0;" data-false="POSITIVE"></iframe>
        <iframe srcdoc="<input>" data-visible   style="position: absolute; clip: rect(0,0,0,0);" data-false="POSITIVE"></iframe>

        <iframe srcdoc="<input>" data-visible   style="width: 100px; height: 100px; position: absolute; left:    -75px;"></iframe>
        <iframe srcdoc="<input>" data-visible   style="width: 100px; height: 100px; position: absolute; top:     -75px;"></iframe>
        <iframe srcdoc="<input>" data-visible   style="width: 100px; height: 100px; position: absolute; left:   -200px;" data-false="POSITIVE"></iframe>
        <iframe srcdoc="<input>" data-visible   style="width: 100px; height: 100px; position: absolute; top:    -200px;" data-false="POSITIVE"></iframe>
        <iframe srcdoc="<input>" data-visible   style="width: 100px; height: 100px; position: absolute; right:  -200px;" data-false="POSITIVE"></iframe>
        <iframe srcdoc="<input>" data-visible   style="width: 100px; height: 100px; position: absolute; bottom: -200px;" data-false="POSITIVE"></iframe>

        <iframe srcdoc="<input>" data-visible   style=""></iframe> <!-- Finish with a visible frame to make sure all <iframe> tags have been closed -->

        <div style="width: 10000; height: 10000"></div>
      </body>)");

  // Ensure that Android runs at default page scale.
  web_view_->SetPageScaleFactor(1.0);

  std::vector<WebElement> iframes = [this] {
    WebDocument doc = GetMainFrame()->GetDocument();
    std::vector<WebElement> result;
    WebElementCollection iframes = doc.GetElementsByHTMLTagName("iframe");
    for (WebElement iframe = iframes.FirstItem(); !iframe.IsNull();
         iframe = iframes.NextItem()) {
      result.push_back(iframe);
    }
    return result;
  }();
  ASSERT_GE(iframes.size(), 23u);

  auto RunTestCases = [](const std::vector<WebElement>& iframes) {
    for (WebElement iframe : iframes) {
      gfx::Rect bounds = iframe.BoundsInWidget();
      bool expectation = iframe.HasAttribute("data-visible");
      SCOPED_TRACE(
          testing::Message()
          << "Iframe with style \n  " << iframe.GetAttribute("style").Ascii()
          << "\nwith dimensions w=" << bounds.width()
          << ",h=" << bounds.height() << " and position x=" << bounds.x()
          << ",y=" << bounds.y()
          << (iframe.HasAttribute("data-false") ? "\nwhich used to be a FALSE "
                                                : "")
          << iframe.GetAttribute("data-false").Ascii());
      ASSERT_TRUE(iframe.HasAttribute("data-visible") !=
                  iframe.HasAttribute("data-invisible"));
      EXPECT_EQ(IsVisibleIframe(iframe), expectation);
    }
  };

  RunTestCases(iframes);

  {
    ExecuteJavaScriptForTests(
        "window.scrollTo(document.body.scrollWidth,document.body.scrollHeight)"
        ";");
    content::RunAllTasksUntilIdle();
    SCOPED_TRACE(testing::Message() << "Scrolled to bottom right");
    RunTestCases(iframes);
  }
}

// Tests the visibility detection of iframes.
// This test checks many scenarios. It's intentionally not a parameterized test
// for performance reasons.
// This test is very similar to the IsVisibleIframeTest test.
TEST_F(FormAutofillUtilsTest, IsWebElementVisibleTest) {
  // Test cases of <input> elements with different types and styles.
  //
  // The `data-[in]visible` attribute represents whether IsWebElementVisible()
  // is expected to classify the input as [in]visible.
  //
  // Since IsWebElementVisible() falls short of what the human user will
  // consider visible or invisible, there are false positives and false
  // negatives. For example, IsWebElementVisible() does not check opacity, so
  // <input style="opacity: 0.0"> is a false positive (it's visible to
  // IsWebElementVisible() but invisible to the human).
  //
  // The `data-false="{POSITIVE,NEGATIVE}"` attribute indicates whether the test
  // case is a false positive/negative compared to human visibility perception.
  // In such a case, not meeting the expectation actually indicates an
  // improvement of IsWebElementVisible(), as it means a false positive/negative
  // has been fixed.
  //
  // The sole purpose of the `data-false` attribute is to document this and to
  // print a message when such a test fails.
  LoadHTML(R"(
      <body>
        <input type="text" data-visible   style="">
        <input type="text" data-visible   style="display: block;">
        <input type="text" data-visible   style="visibility: visible;">

        <input type="text" data-invisible style="display: none;">
        <input type="text" data-invisible style="visibility: hidden;">
        <div style="display: none;">     <input type="text" data-invisible></div>
        <div style="visibility: hidden;"><input type="text" data-invisible></div>

        <input type="text" data-visible   style="width: 15px; height: 15px;">
        <input type="text" data-invisible style="width: 15px; height:  5px;">
        <input type="text" data-invisible style="width:  5px; height: 15px;">
        <input type="text" data-invisible style="width:  5px; height:  5px;">

        <input type="text" data-invisible style="width: 1px; height: 1px;">
        <input type="text" data-invisible style="width: 1px; height: 1px; overflow: visible;" data-false="NEGATIVE">

        <input type="text" data-visible   style="opacity: 0.0;" data-false="POSITIVE">
        <input type="text" data-visible   style="opacity: 0.0;" data-false="POSITIVE">
        <input type="text" data-visible   style="position: absolute; clip: rect(0,0,0,0);" data-false="POSITIVE">

        <input type="text" data-visible   style="width: 100px; position: absolute; left:    -75px;">
        <input type="text" data-visible   style="width: 100px; position: absolute; top:     -75px;">
        <input type="text" data-visible   style="width: 100px; position: absolute; left:   -200px;" data-false="POSITIVE">
        <input type="text" data-visible   style="width: 100px; position: absolute; top:    -200px;" data-false="POSITIVE">
        <input type="text" data-visible   style="width: 100px; position: absolute; right:  -200px;" data-false="POSITIVE">
        <input type="text" data-visible   style="width: 100px; position: absolute; bottom: -200px;" data-false="POSITIVE">

        <input type="checkbox" data-visible   style="">
        <input type="checkbox" data-invisible style="display: none;">
        <input type="checkbox" data-invisible style="visibility: hidden;">
        <input type="checkbox" data-visible   style="width: 15px; height: 15px;">
        <input type="checkbox" data-visible   style="width: 15px; height:  5px;">
        <input type="checkbox" data-visible   style="width:  5px; height: 15px;">
        <input type="checkbox" data-visible   style="width:  5px; height:  5px;">

        <input type="radio" data-visible   style="">
        <input type="radio" data-invisible style="display: none;">
        <input type="radio" data-invisible style="visibility: hidden;">
        <input type="radio" data-visible   style="width: 15px; height: 15px;">
        <input type="radio" data-visible   style="width: 15px; height:  5px;">
        <input type="radio" data-visible   style="width:  5px; height: 15px;">
        <input type="radio" data-visible   style="width:  5px; height:  5px;">

        <div style="width: 10000; height: 10000"></div>
      </body>)");

  // Ensure that Android runs at default page scale.
  web_view_->SetPageScaleFactor(1.0);

  std::vector<WebElement> inputs = [this] {
    WebDocument doc = GetMainFrame()->GetDocument();
    std::vector<WebElement> result;
    WebElementCollection inputs = doc.GetElementsByHTMLTagName("input");
    for (WebElement input = inputs.FirstItem(); !input.IsNull();
         input = inputs.NextItem()) {
      result.push_back(input);
    }
    return result;
  }();
  ASSERT_GE(inputs.size(), 36u);

  auto RunTestCases = [](const std::vector<WebElement>& inputs) {
    for (WebElement input : inputs) {
      gfx::Rect bounds = input.BoundsInWidget();
      bool expectation = input.HasAttribute("data-visible");
      SCOPED_TRACE(
          testing::Message()
          << "Iframe with style \n  " << input.GetAttribute("style").Ascii()
          << "\nwith dimensions w=" << bounds.width()
          << ",h=" << bounds.height() << " and position x=" << bounds.x()
          << ",y=" << bounds.y()
          << (input.HasAttribute("data-false") ? "\nwhich used to be a FALSE "
                                               : "")
          << input.GetAttribute("data-false").Ascii());
      ASSERT_TRUE(input.HasAttribute("data-visible") !=
                  input.HasAttribute("data-invisible"));
      EXPECT_EQ(IsWebElementVisible(input), expectation);
    }
  };

  RunTestCases(inputs);

  {
    ExecuteJavaScriptForTests(
        "window.scrollTo(document.body.scrollWidth,document.body.scrollHeight)"
        ";");
    content::RunAllTasksUntilIdle();
    SCOPED_TRACE(testing::Message() << "Scrolled to bottom right");
    RunTestCases(inputs);
  }
}

// Tests `GetClosestAncestorFormElement(element)`.
TEST_F(FormAutofillUtilsTest, GetClosestAncestorFormElement) {
  LoadHTML(R"(
      <body>
        <iframe id=unowned></iframe>
        <form id=outer_form>
          <iframe id=owned1></iframe>
          <!-- A nested 'inner_form' with an iframe 'owned2' will be
               created dynamically. -->
          <form id=non_existent>
            <iframe id=owned3></iframe>
          </form>
        </form>
      </body>)");
  ExecuteJavaScriptForTests(R"(
      const inner_form = document.createElement('form');
      inner_form.id = 'inner_form';
      const owned2 = document.createElement('iframe');
      owned2.id = 'owned2';
      inner_form.appendChild(owned2);
      document.getElementById('outer_form').appendChild(inner_form);
    )");
  content::RunAllTasksUntilIdle();

  WebDocument doc = GetMainFrame()->GetDocument();
  EXPECT_EQ(GetClosestAncestorFormElement(GetElementById(doc, "unowned")),
            WebFormElement());
  EXPECT_EQ(GetClosestAncestorFormElement(GetElementById(doc, "owned1")),
            GetFormElementById(doc, "outer_form"));
  EXPECT_EQ(GetClosestAncestorFormElement(GetElementById(doc, "owned2")),
            GetFormElementById(doc, "inner_form"));
  EXPECT_EQ(GetClosestAncestorFormElement(GetElementById(doc, "owned3")),
            GetFormElementById(doc, "outer_form"));
  EXPECT_EQ(WebFormControlElement(),
            GetFormElementById(doc, "non_existent_form", AllowNull(true)));
}

// Tests that `IsDOMPredecessor(lhs, rhs, ancestor_hint)` holds iff a DOM
// traversal visits the DOM element with ID `lhs` before the one with ID `rhs`,
// where `ancestor_hint` is the ID of an ancestor DOM node.
//
// For this test, DOM element IDs should be named so that if X as visited
// before Y, then X.id is lexicographically less than Y.id.
TEST_F(FormAutofillUtilsTest, IsDomPredecessorTest) {
  LoadHTML(R"(
      <body id=0>
        <div id=00>
          <input id=000>
          <input id=001>
          <div id=002>
            <input id=0020>
          </div>
          <div id=003>
            <input id=0030>
          </div>
          <input id=004>
        </div>
        <div id=01>
          <iframe id=010></iframe>
          <input id=011>
        </div>
      </body>)");

  // The parameter type of IsDomPredecessorTest. The attributes are the IDs of
  // the left and right hand side DOM nodes that are to be compared, and some
  // common ancestor of them.
  struct IsDomPredecessorTestParam {
    std::string lhs_id;
    std::string rhs_id;
    std::vector<std::string> ancestor_hint_ids = {"",    "0",   "00",
                                                  "002", "003", "01"};
  };
  std::vector<IsDomPredecessorTestParam> test_cases = {
      IsDomPredecessorTestParam{"000", "000"},
      IsDomPredecessorTestParam{"001", "001"},
      IsDomPredecessorTestParam{"000", "001"},
      IsDomPredecessorTestParam{"000", "001"},
      IsDomPredecessorTestParam{"000", "0020"},
      IsDomPredecessorTestParam{"000", "0020"},
      IsDomPredecessorTestParam{"000", "004"},
      IsDomPredecessorTestParam{"000", "004"},
      IsDomPredecessorTestParam{"0020", "0030"},
      IsDomPredecessorTestParam{"0020", "0030"},
      IsDomPredecessorTestParam{"0030", "004"},
      IsDomPredecessorTestParam{"000", "010"},
      IsDomPredecessorTestParam{"0030", "010"},
      IsDomPredecessorTestParam{"0030", "011"},
      IsDomPredecessorTestParam{"010", "011"}};

  for (const auto& test : test_cases) {
    for (const auto& ancestor_hint_id : test.ancestor_hint_ids) {
      SCOPED_TRACE(testing::Message()
                   << "lhs=" << test.lhs_id << " rhs=" << test.rhs_id
                   << " ancestor_hint_id=" << ancestor_hint_id);
      ASSERT_NE(test.lhs_id, ancestor_hint_id);
      ASSERT_NE(test.rhs_id, ancestor_hint_id);
      WebDocument doc = GetMainFrame()->GetDocument();
      WebNode lhs = GetElementById(doc, test.lhs_id);
      WebNode rhs = GetElementById(doc, test.rhs_id);
      WebNode ancestor_hint = ancestor_hint_id.empty()
                                  ? WebNode()
                                  : GetElementById(doc, ancestor_hint_id);
      EXPECT_EQ(test.lhs_id < test.rhs_id,
                IsDOMPredecessor(lhs, rhs, ancestor_hint));
      EXPECT_EQ(test.rhs_id < test.lhs_id,
                IsDOMPredecessor(rhs, lhs, ancestor_hint));
    }
  }
}

// The DOM ID of an <input> or <iframe>.
struct FieldOrFrame {
  bool is_frame = false;
  const char* id;
};

// A FieldFramesTest test case contains HTML code. The form with DOM ID
// |form_id| (nullptr for the synthetic form) shall be extracted and its fields
// and frames shall match |fields_and_frames|.
struct FieldFramesTestParam {
  std::string html;
  const char* form_id;
  std::vector<FieldOrFrame> fields_and_frames;
};

class FieldFramesTest
    : public FormAutofillUtilsTestWithIframesEnabled,
      public testing::WithParamInterface<FieldFramesTestParam> {
 public:
  FieldFramesTest() = default;
  ~FieldFramesTest() override = default;
};

// Check if the unowned form control elements are properly extracted.
// Form control elements are button, fieldset, input, textarea, output and
// select elements.
TEST_F(FormAutofillUtilsTest, GetUnownedFormFieldElements) {
  LoadHTML(R"(
    <button id='unowned_button'>Unowned button</button>
    <fieldset id='unowned_fieldset'>
      <label>Unowned fieldset</label>
    </fieldset>
    <input id='unowned_input'>
    <textarea id='unowned_textarea'>I am unowned</textarea>
    <output id='unowned_output'>Unowned output</output>
    <select id='unowned_select'>
      <option value='first'>first</option>
      <option value='second' selected>second</option>
    </select>
    <object id='unowned_object'></object>

    <form id='form'>
      <button id='form_button'>Form button</button>
      <fieldset id='form_fieldset'>
        <label>Form fieldset</label>
      </fieldset>
      <input id='form_input'>
      <textarea id='form_textarea'>I am in a form</textarea>
      <output id='form_output'>Form output</output>
      <select name='form_select' id='form_select'>
        <option value='june'>june</option>
        <option value='july' selected>july</option>
      </select>
      <object id='form_object'></object>
    </form>
  )");

  WebDocument doc = GetMainFrame()->GetDocument();
  std::vector<blink::WebElement> unowned_fieldsets;
  std::vector<WebFormControlElement> unowned_form_fields =
      GetUnownedFormFieldElements(doc, &unowned_fieldsets);

  EXPECT_THAT(unowned_form_fields,
              ElementsAre(GetFormControlElementById(doc, "unowned_button"),
                          GetFormControlElementById(doc, "unowned_fieldset"),
                          GetFormControlElementById(doc, "unowned_input"),
                          GetFormControlElementById(doc, "unowned_textarea"),
                          GetFormControlElementById(doc, "unowned_output"),
                          GetFormControlElementById(doc, "unowned_select")));
  EXPECT_THAT(unowned_fieldsets,
              ElementsAre(GetFormControlElementById(doc, "unowned_fieldset")));
}

// Tests that FormData::fields and FormData::child_frames are extracted fully
// and in the correct relative order.
TEST_P(FieldFramesTest, ExtractFieldsAndFrames) {
  FieldFramesTestParam test_case = GetParam();
  SCOPED_TRACE(testing::Message() << "HTML: " << test_case.html);
  LoadHTML(test_case.html.c_str());
  WebDocument doc = GetMainFrame()->GetDocument();

  // Extract the |form_data|.
  FormData form_data;
  FormRendererId host_form;
  if (!test_case.form_id) {  // Synthetic form.
    std::vector<blink::WebElement> fieldsets;
    std::vector<WebFormControlElement> control_elements =
        GetUnownedAutofillableFormFieldElements(doc, &fieldsets);
    std::vector<WebElement> iframe_elements =
        form_util::GetUnownedIframeElements(doc);
    ASSERT_TRUE(UnownedFormElementsAndFieldSetsToFormData(
        fieldsets, control_elements, iframe_elements, nullptr, doc, nullptr,
        EXTRACT_NONE, &form_data, nullptr));
    host_form = FormRendererId();
  } else {  // Real <form>.
    ASSERT_GT(std::strlen(test_case.form_id), 0u);
    auto form_element = GetFormElementById(doc, test_case.form_id);
    ASSERT_TRUE(WebFormElementToFormData(form_element, WebFormControlElement(),
                                         nullptr, EXTRACT_NONE, &form_data,
                                         nullptr));
    host_form = GetFormRendererId(form_element);
  }

  // Check that all fields and iframes were extracted.
  EXPECT_EQ(form_data.fields.size() + form_data.child_frames.size(),
            test_case.fields_and_frames.size());

  // Check that all fields were extracted. Do so by checking for each |field| in
  // `test_case.fields_and_frames` that the DOM element with ID `field.id`
  // corresponds to the next (`i`th) field in `form_data.fields`.
  size_t i = 0;
  for (const FieldOrFrame& field : test_case.fields_and_frames) {
    if (field.is_frame)
      continue;
    SCOPED_TRACE(testing::Message() << "Checking the " << i
                                    << "th field (id = " << field.id << ")");
    WebElement element = GetElementById(doc, field.id);
    ASSERT_FALSE(element.IsNull());
    ASSERT_TRUE(element.IsFormControlElement());
    WebFormControlElement control = element.To<WebFormControlElement>();
    ASSERT_FALSE(control.IsNull());
    FieldRendererId renderer_id(control.UniqueRendererFormControlId());
    EXPECT_EQ(form_data.fields[i].host_form_id, host_form);
    EXPECT_EQ(form_data.fields[i].unique_renderer_id, renderer_id);
    ++i;
  }

  // Check that all frames were extracted into `form_data.child_frames`
  // analogously to the above loop for `form_data.fields`.
  //
  // In addition, check that `form_data.child_frames[i].predecessor` encodes the
  // correct ordering, i.e., that `form_data.child_frames[i].predecessor` is the
  // index of the last field in `form_data.fields` that precedes the `i`th frame
  // in `form_data.child_frames`.
  i = 0;
  int preceding_field_index = -1;
  for (const auto& frame : test_case.fields_and_frames) {
    if (!frame.is_frame) {
      ++preceding_field_index;
      continue;
    }
    SCOPED_TRACE(testing::Message() << "Checking the " << i
                                    << "th frame (id = " << frame.id << ")");
    auto is_empty = [](auto token) { return token.is_empty(); };
    EXPECT_FALSE(absl::visit(is_empty, form_data.child_frames[i].token));
    EXPECT_EQ(form_data.child_frames[i].token, GetFrameToken(doc, frame.id));
    EXPECT_EQ(form_data.child_frames[i].predecessor, preceding_field_index);
    ++i;
  }
}

// Creates 32 test cases containing forms which differ in five bits: whether or
// not the form of interest is a synthetic form, and whether the first, second,
// third, and fourth element are a form control field or an iframe. This form is
// additionally surrounded by two other forms before and after itself. An
// example:
//   <body>
//     <!-- Two forms not of interest follow -->
//     <form><input><iframe></iframe></form>
//     <input><iframe></iframe>
//     <!-- The form of interest follows -->
//     <form id='MY_FORM_ID'>
//       <input>
//       <input>
//       <iframe></iframe>
//       <iframe></iframe>
//     </form>
//     <!-- Two forms not of interest follow -->
//     <form><input><iframe></iframe></form>
//     <input><iframe></iframe>
//   </body>
INSTANTIATE_TEST_SUITE_P(
    FormAutofillUtilsTest,
    FieldFramesTest,
    testing::ValuesIn([] {
      // Creates a FieldFramesTestParam. The fifth bit encodes whether the form
      // is a synthetic form or not, and the first four bits encode whether its
      // four elements are fields or frames.
      //
      // The choice of four is to cover multiple elements of the same kind
      // following each other and being surrounded by other fields, e.g.,
      // <input><iframe><iframe><input>.
      auto MakeTestCase = [](std::bitset<5> bitset) {
        std::vector<FieldOrFrame> fields_and_frames{
            {.is_frame = bitset.test(0), .id = "0"},
            {.is_frame = bitset.test(1), .id = "1"},
            {.is_frame = bitset.test(2), .id = "2"},
            {.is_frame = bitset.test(3), .id = "3"},
        };
        bool is_synthetic_form = bitset.test(4);
        const char* form_id = is_synthetic_form ? nullptr : "MY_FORM_ID";

        // Create a HTML page according to |is_synthetic_form| and
        // |fields_and_frames|: it contains four <input> or <iframe> elements,
        // potentially contained in a <form>. Additionally, before and after
        // this form, it contains some other <input> and <iframe> elements that
        // do not belong to the form of interest.
        std::string html;
        for (const FieldOrFrame& field_or_frame : fields_and_frames) {
          html += base::StringPrintf(field_or_frame.is_frame
                                         ? "<iframe id='%s'></iframe>"
                                         : "<input id='%s'>",
                                     field_or_frame.id);
        }
        if (!is_synthetic_form) {
          html = base::StringPrintf("<form id='%s'>%s</form>", form_id,
                                    html.c_str());
          const char* other_forms =
              "<input><iframe></iframe> <form><input><iframe></iframe></form>";
          html = base::StrCat({other_forms, html, other_forms});
        } else {
          const char* other_form = "<form><input><iframe></iframe></form>";
          html = base::StrCat({other_form, html, other_form});
        }
        html = base::StringPrintf("<body>%s</body>", html.c_str());
        return FieldFramesTestParam{.html = html,
                                    .form_id = form_id,
                                    .fields_and_frames = fields_and_frames};
      };

      // Create all combinations of test cases.
      std::vector<FieldFramesTestParam> cases;
      for (uint64_t bitset = 0; bitset < (1 << 5); ++bitset)
        cases.push_back(MakeTestCase(std::bitset<5>(bitset)));

      // Check that we have 32 distinct test cases.
      DCHECK_EQ(cases.size(), 32u);
      DCHECK(base::ranges::all_of(cases, [&](const auto& case1) {
        return base::ranges::all_of(cases, [&](const auto& case2) {
          return &case1 == &case2 || case1.html != case2.html;
        });
      }));
      return cases;
    }()));

// Tests that if the number of iframes exceeds kMaxParseableChildFrames,
// child frames of that form are not extracted.
TEST_F(FormAutofillUtilsTestWithIframesEnabled,
       ExtractNoFramesIfTooManyIframes) {
  auto CreateFormElement = [this](const char* element) {
    std::string js = base::StringPrintf(
        "document.forms[0].appendChild(document.createElement('%s'))", element);
    ExecuteJavaScriptForTests(js.c_str());
  };

  LoadHTML(R"(<html><body><form id='f'></form>)");
  for (size_t i = 0; i < kMaxParseableFields - 1; ++i)
    CreateFormElement("input");
  for (size_t i = 0; i < kMaxParseableChildFrames; ++i)
    CreateFormElement("iframe");

  // Ensure that Android runs at default page scale.
  web_view_->SetPageScaleFactor(1.0);

  WebDocument doc = GetMainFrame()->GetDocument();
  WebFormElement form = GetFormElementById(doc, "f");
  {
    FormData form_data;
    ASSERT_TRUE(WebFormElementToFormData(form, WebFormControlElement(), nullptr,
                                         EXTRACT_NONE, &form_data, nullptr));
    EXPECT_EQ(form_data.fields.size(), kMaxParseableFields - 1);
    EXPECT_EQ(form_data.child_frames.size(), kMaxParseableChildFrames);
  }

  // There may be multiple checks (e.g., == kMaxParseableChildFrames, <=
  // kMaxParseableChildFrames, < kMaxParseableChildFrames), so we test different
  // numbers of <iframe> elements.
  for (int i = 0; i < 3; ++i) {
    CreateFormElement("iframe");
    FormData form_data;
    ASSERT_TRUE(WebFormElementToFormData(form, WebFormControlElement(), nullptr,
                                         EXTRACT_NONE, &form_data, nullptr));
    EXPECT_EQ(form_data.fields.size(), kMaxParseableFields - 1);
    EXPECT_TRUE(form_data.child_frames.empty());
  }
}

// Tests that if the number of fields exceeds |kMaxParseableFields|, neither
// fields nor child frames of that form are extracted.
TEST_F(FormAutofillUtilsTestWithIframesEnabled,
       ExtractNoFieldsOrFramesIfTooManyFields) {
  auto CreateFormElement = [this](const char* element) {
    std::string js = base::StringPrintf(
        "document.forms[0].appendChild(document.createElement('%s'))", element);
    ExecuteJavaScriptForTests(js.c_str());
  };

  LoadHTML(R"(<html><body><form id='f'></form>)");
  for (size_t i = 0; i < kMaxParseableFields - 1; ++i)
    CreateFormElement("input");
  for (size_t i = 0; i < kMaxParseableChildFrames; ++i)
    CreateFormElement("iframe");

  // Ensure that Android runs at default page scale.
  web_view_->SetPageScaleFactor(1.0);

  WebDocument doc = GetMainFrame()->GetDocument();
  WebFormElement form = GetFormElementById(doc, "f");
  {
    FormData form_data;
    ASSERT_TRUE(WebFormElementToFormData(form, WebFormControlElement(), nullptr,
                                         EXTRACT_NONE, &form_data, nullptr));
    EXPECT_EQ(form_data.fields.size(), kMaxParseableFields - 1);
    EXPECT_EQ(form_data.child_frames.size(), kMaxParseableChildFrames);
  }

  // There may be multiple checks (e.g., == kMaxParseableFields, <=
  // kMaxParseableFields, < kMaxParseableFields), so we test different numbers
  // of <input> elements.
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(base::NumberToString(i));
    CreateFormElement("input");
    FormData form_data;
    ASSERT_FALSE(WebFormElementToFormData(form, WebFormControlElement(),
                                          nullptr, EXTRACT_NONE, &form_data,
                                          nullptr));
    EXPECT_TRUE(form_data.fields.empty());
    EXPECT_TRUE(form_data.child_frames.empty());
  }
}

// Fills a form, resets the form using <input type=reset>, and fills it again.
// Tests that the form is actually filled on the second fill
// (crbug.com/1291619).
TEST_F(FormAutofillUtilsTest, FillAndResetAndFillAgainForm) {
  LoadHTML(R"(
    <body>
      <form id="f">
        <input id="f0">
        <select id="f1">
          <option value="Bar">Bar</option>
          <option value="Foo">Foo</option>
          <option value="Zoo">Zoo</option>
        </select>
        <input id="reset" type="reset">
      </form>
    </body>
  )");
  WebDocument doc = GetMainFrame()->GetDocument();
  auto field_manager = base::MakeRefCounted<FieldDataManager>();

  FormData form;
  ExtractFormData(GetFormElementById(doc, "f"), *field_manager, &form);
  ASSERT_EQ(form.fields.size(), 2u);
  form.fields[0].value = u"Foo";
  form.fields[1].value = u"Foo";
  form.fields[0].is_autofilled = true;
  form.fields[1].is_autofilled = true;

  // First fill of the form.
  FillOrPreviewForm(form, GetFormControlElementById(doc, "f0"),
                    mojom::RendererFormDataAction::kFill);
  // Autofilling f0 leaves f0.UserHasEditedTheField() == false.
  // TODO(crbug.com/1291619): Is this desired?
  EXPECT_TRUE(GetFormControlElementById(doc, "f1").UserHasEditedTheField());
  EXPECT_EQ(GetFormControlElementById(doc, "f0").Value().Ascii(), "Foo");
  EXPECT_EQ(GetFormControlElementById(doc, "f1").Value().Ascii(), "Foo");

  // Click reset button.
  GetFormControlElementById(doc, "reset").SimulateClick();
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(GetFormControlElementById(doc, "f0").UserHasEditedTheField());
  EXPECT_FALSE(GetFormControlElementById(doc, "f1").UserHasEditedTheField());
  EXPECT_EQ(GetFormControlElementById(doc, "f0").Value().Ascii(), "");
  EXPECT_EQ(GetFormControlElementById(doc, "f1").Value().Ascii(), "Bar");

  // Fill form again.
  FillOrPreviewForm(form, GetFormControlElementById(doc, "f0"),
                    mojom::RendererFormDataAction::kFill);
  // Autofilling f0 leaves f0.UserHasEditedTheField() == false.
  // TODO(crbug.com/1291619): Is this desired?
  EXPECT_TRUE(GetFormControlElementById(doc, "f1").UserHasEditedTheField());
  EXPECT_EQ(GetFormControlElementById(doc, "f0").Value().Ascii(), "Foo");
  EXPECT_EQ(GetFormControlElementById(doc, "f1").Value().Ascii(), "Foo");
}

}  // namespace
}  // namespace form_util
}  // namespace autofill
