// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
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
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "third_party/blink/public/web/web_view.h"

namespace autofill::form_util {
namespace {

using autofill::mojom::ButtonTitleType;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebString;
using blink::WebVector;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointwise;
using ::testing::Property;
using ::testing::Values;

struct AutofillFieldUtilCase {
  std::string_view description;
  std::string_view html;
  std::u16string_view expected_label;
};

// An <input> with a label placed on top of it (usually used as a placeholder
// replacement).
const char* kPoorMansPlaceholderFullOverlap = R"(
  <style>
    .fixed_position_and_size {
      position: fixed;
      top: 0;
      left: 0;
      width: 100px;
      height: 20px;
    }
  </style>
  <input id=target class=fixed_position_and_size>
  <span class=fixed_position_and_size>label</span>
)";

// The <input> element partially overlaps the label (placeholder) but the label
// is not fully contained in the <input> element. This is a common case for
// placeholders that moph into a minified version when the user focuses an
// <input> element.
const char* kPoorMansPlaceholderPartialOverlap = R"(
  <style>
    .fixed_position_and_size {
      position: fixed;
      top: 30px;
      left: 0;
      width: 100px;
      height: 20px;
    }
    .overlapping_position_and_size {
      position: fixed;
      top: 25px;
      left: 0;
      width: 100px;
      height: 20px;
    }
  </style>
  <input id=target class=fixed_position_and_size>
  <span class=overlapping_position_and_size>label</span>
)";

// The <input> element touches the next element vertically but does not overlap.
// The label should not be considered a placeholder.
const char* kPoorMansPlaceholderNoOverlap = R"(
  <input id='target'>
  <div>not a label</div>
)";

// The <input> element touches the next element horizontally but does not
// overlap. The label should not be considered a placeholder.
const char* kPoorMansPlaceholderNoOverlap2 = R"(
  <input id=target>
  <span>not a label</span>
)";

// The span exceeds the vertical limits of the input element, which is a
// pattern often observed in error messages. Therefore we don't consider the
// span a label.
const char* kPoorMansPlaceholderPossiblyErrorMessage = R"(
  <style>
    .fixed_position_and_size {
      position: fixed;
      top: 0px;
      left: 0;
      width: 100px;
      height: 20px;
    }
    .label_position_and_size {
      position: fixed;
      top: 15px;
      left: 0;
      width: 100px;
      height: 25px;
    }
  </style>
  <input id=target class=fixed_position_and_size>
  <span class=overlapping_position_and_size>not a label</span>
)";

// The span is not horizontally contained in the input element. We don't
// consider this a label because have seen several cases where the actual
// label was on the left of the input field in a <table> structure and the
// text on the right, which just touched the element contained non-label
// data (e.g. instructions like "don't enter symbols").
const char* kPoorMansPlaceholderNoHorizontalContainment = R"(
  <style>
    .fixed_position_and_size {
      position: fixed;
      top: 0px;
      left: 0;
      width: 100px;
      height: 20px;
    }
    .label_position_and_size {
      position: fixed;
      top: 15px;
      left: 90px;
      width: 100px;
      height: 20px;
    }
  </style>
  <input id=target class=fixed_position_and_size>
  <span class=overlapping_position_and_size>not a label</span>
)";

void VerifyButtonTitleCache(const WebFormElement& form_target,
                            const ButtonTitleList& expected_button_titles,
                            const ButtonTitlesCache& actual_cache) {
  EXPECT_THAT(actual_cache, ElementsAre(Pair(GetFormRendererId(form_target),
                                             expected_button_titles)));
}

bool HaveSameFormControlId(const WebFormControlElement& element,
                           const FormFieldData& field) {
  return GetFieldRendererId(element) == field.renderer_id();
}

class FormAutofillUtilsTest : public content::RenderViewTest {
 public:
  static constexpr CallTimerState kCallTimerStateDummy = {
      .call_site = CallTimerState::CallSite::kUpdateFormCache,
      .last_autofill_agent_reset = {},
      .last_dom_content_loaded = {},
  };

  FormAutofillUtilsTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillReplaceCachedWebElementsByRendererIds},
        /*disabled_features=*/{});
  }
  ~FormAutofillUtilsTest() override = default;

  WebDocument GetDocument() { return GetMainFrame()->GetDocument(); }

  std::optional<FormData> ExtractFormData(
      WebFormElement form,
      DenseSet<ExtractOption> extract_options = {}) {
    return form_util::ExtractFormData(GetDocument(), form, field_data_manager(),
                                      kCallTimerStateDummy, extract_options);
  }

  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
  FindFormAndFieldForFormControlElement(
      WebFormControlElement control,
      DenseSet<ExtractOption> extract_options = {}) {
    return form_util::FindFormAndFieldForFormControlElement(
        control, field_data_manager(), kCallTimerStateDummy, extract_options);
  }

  FieldDataManager& field_data_manager() { return *field_data_manager_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<FieldDataManager> field_data_manager_ =
      base::MakeRefCounted<FieldDataManager>();
};

// Tests that WebFormElementToFormData() sets the
// Form[Field]Data::{name,id_attribute,name_attribute} correctly.
TEST_F(FormAutofillUtilsTest, WebFormElementToFormData_IdAndNames) {
  LoadHTML(R"(
    <form id=form-id name=form-name>
      <input type=text id=input-id name=input-name>
    </form>
  )");
  FormData form_data =
      *ExtractFormData(GetFormElementById(GetDocument(), "form-id"));
  EXPECT_EQ(form_data.name(), u"form-name");
  EXPECT_EQ(form_data.id_attribute(), u"form-id");
  EXPECT_EQ(form_data.name_attribute(), u"form-name");
  ASSERT_EQ(form_data.fields().size(), 1u);
  EXPECT_EQ(form_data.fields()[0].name(), u"input-name");
  EXPECT_EQ(form_data.fields()[0].id_attribute(), u"input-id");
  EXPECT_EQ(form_data.fields()[0].name_attribute(), u"input-name");
}

// Tests that form extraction measures its total time, also split by caller.
TEST_F(FormAutofillUtilsTest, ExtractFormDataMeasuresTotalTime) {
  base::HistogramTester histogram_tester;
  LoadHTML(R"(
    <input>
  )");
  FormData form_data = *ExtractFormData(WebFormElement());
  histogram_tester.ExpectTotalCount("Autofill.TimingPrecise.ExtractFormData",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.TimingPrecise.ExtractFormData.UpdateFormCache", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.TimingInterval.ExtractFormData.UpdateFormCache."
      "AutofillAgentReset",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.TimingInterval.ExtractFormData.UpdateFormCache."
      "DOMContentLoaded",
      1);
}

// Tests that form extraction measures how long label extraction took.
TEST_F(FormAutofillUtilsTest,
       ExtractFormDataMeasuresDurationOfLabelExtraction) {
  base::HistogramTester histogram_tester;
  LoadHTML(R"(
    <form id=form-id>
      <input type=text>
    </form>
  )");
  FormData form_data =
      *ExtractFormData(GetFormElementById(GetDocument(), "form-id"));
  histogram_tester.ExpectTotalCount(
      "Autofill.TimingPrecise.InferLabelForElement", 1);
}

// Tests that large option values/contents are truncated while building the
// FormData.
TEST_F(FormAutofillUtilsTest, TruncateLargeOptionValuesAndContents) {
  std::string huge_option(kMaxStringLength + 10, 'a');
  std::u16string trimmed_option(kMaxStringLength, 'a');

  LoadHTML(base::StringPrintf(R"(
    <form id='form'>
      <select name='form_select' id='form_select'>
        <option value='%s'>%s</option>
      </select>
    </form>
  )",
                              huge_option.c_str(), huge_option.c_str())
               .c_str());

  auto web_form = GetFormElementById(GetDocument(), "form");

  FormData form_data = *ExtractFormData(web_form);
  ASSERT_EQ(form_data.fields().size(), 1u);
  ASSERT_EQ(form_data.fields()[0].options().size(), 1u);
  EXPECT_EQ(form_data.fields()[0].options()[0].value, trimmed_option);
  EXPECT_EQ(form_data.fields()[0].options()[0].text, trimmed_option);
  EXPECT_TRUE(IsValidOption(form_data.fields()[0].options()[0]));
}

// Tests that the SelectOption::value and SelectOption::text are extracted
// correctly.
TEST_F(FormAutofillUtilsTest, ExtractFormData_SelectOptionValueAndText) {
  LoadHTML(R"(
    <select>
    <option value=V label=L     >T</option>
    <option value=V             >T</option>
    <option         label=L     >T</option>
    <option                     >T</option>
    <option value=V             ></option>
    <option         label=L     ></option>
    <option         aria-label=A></option>
    </select>
  )");
  std::optional<FormData> form = ExtractFormData(WebFormElement());
  ASSERT_TRUE(form);
  EXPECT_THAT(form->fields().front().options(),
              ElementsAre(SelectOption{.value = u"V", .text = u"L"},
                          SelectOption{.value = u"V", .text = u"T"},
                          SelectOption{.value = u"T", .text = u"L"},
                          SelectOption{.value = u"T", .text = u"T"},
                          SelectOption{.value = u"V", .text = u""},
                          SelectOption{.value = u"", .text = u"L"},
                          SelectOption{.value = u"", .text = u"A"}));
}

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
      // TODO(crbug.com/40555780): Depth is only 5 elements instead of 10. This
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
    WebElement target = GetElementById(GetDocument(), "target");
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
    WebElement target = GetElementById(GetDocument(), "target");
    WebVector<WebElement> web_to_skip =
        GetDocument().QuerySelectorAll("div[class='skip']");
    std::set<WebNode> to_skip;
    for (const WebElement& element : web_to_skip) {
      to_skip.insert(element);
    }

    EXPECT_EQ(test_case.expected_label,
              FindChildTextWithIgnoreListForTesting(target, to_skip));
  }
}

TEST_F(FormAutofillUtilsTest, InferLabelForElementTest) {
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
       u"label"},
      {"Infer from next sibling",
       "<input id='target' type='checkbox'>hello <b>world</b>", u"hello world"},
      {"Poor man's placeholder", kPoorMansPlaceholderFullOverlap, u"label"},
      {"Poor man's placeholder partial overlap",
       kPoorMansPlaceholderPartialOverlap, u"label"},
      {"Poor man's placeholder no overlap", kPoorMansPlaceholderNoOverlap, u""},
      {"Poor man's placeholder no overlap 2", kPoorMansPlaceholderNoOverlap2,
       u""},
      {"Poor man's placeholder: possibly an error message",
       kPoorMansPlaceholderPossiblyErrorMessage, u""},
      {"Poor man's placeholder: no horizontal containment",
       kPoorMansPlaceholderNoHorizontalContainment, u""},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    LoadHTML(test_case.html);
    WebFormControlElement form_target =
        GetFormControlElementById(GetDocument(), "target");
    std::vector<FormFieldData> fields(1);
    InferLabelForElementsForTesting(
        std::to_array<WebFormControlElement>({form_target}), fields);
    EXPECT_EQ(fields.front().label(), test_case.expected_label);
  }
}

TEST_F(FormAutofillUtilsTest, InferLabelSourceTest) {
  struct AutofillFieldLabelSourceCase {
    const char* html;
    const FormFieldData::LabelSource label_source;
  };
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
      {kPoorMansPlaceholderFullOverlap,
       FormFieldData::LabelSource::kOverlayingLabel}};

  for (auto test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << test_case.label_source);
    LoadHTML(test_case.html);
    WebFormControlElement form_target =
        GetFormControlElementById(GetDocument(), "target");
    std::vector<FormFieldData> fields(1);
    InferLabelForElementsForTesting(
        std::to_array<WebFormControlElement>({form_target}), fields);
    EXPECT_EQ(fields.front().label_source(), test_case.label_source);
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
  WebFormElement form_target = GetFormElementById(GetDocument(), "target");
  ButtonTitlesCache cache;

  autofill::ButtonTitleList actual = GetButtonTitles(form_target, &cache);

  autofill::ButtonTitleList expected = {
      {u"Sign Up", ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE}};
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
  WebFormElement form_target = GetFormElementById(GetDocument(), "target");
  ButtonTitlesCache cache;

  autofill::ButtonTitleList actual = GetButtonTitles(form_target, &cache);

  int total_length = 0;
  for (const auto& [title, title_type] : actual) {
    EXPECT_GE(30u, title.length());
    total_length += title.length();
  }
  EXPECT_EQ(200, total_length);
}

TEST_F(FormAutofillUtilsTest, GetButtonTitles_NoCache) {
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
  WebFormElement form_target = GetFormElementById(GetDocument(), "target");

  autofill::ButtonTitleList expected = {
      {u"Sign Up", ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE}};
  autofill::ButtonTitleList actual =
      GetButtonTitles(form_target, /*button_titles_cache=*/nullptr);
  EXPECT_EQ(expected, actual);
}

TEST_F(FormAutofillUtilsTest, GetButtonTitles_NoForm) {
  // Attempting to get button titles from a null form should produce an empty
  // list and not crash.
  WebFormElement form;
  ASSERT_FALSE(form);
  EXPECT_EQ(GetButtonTitles(form, /*button_titles_cache=*/nullptr).size(), 0u);
}

TEST_F(FormAutofillUtilsTest, IsEnabled) {
  LoadHTML(
      "<input type='text' id='name1'>"
      "<input type='password' disabled id='name2'>"
      "<input type='password' id='name3'>"
      "<input type='text' id='name4' disabled>");
  std::optional<FormData> form = *ExtractFormData(WebFormElement());
  EXPECT_THAT(
      form, Optional(Property(
                &FormData::fields,
                ElementsAre(
                    AllOf(Property(&FormFieldData::name, u"name1"),
                          Property(&FormFieldData::is_enabled, IsTrue())),
                    AllOf(Property(&FormFieldData::name, u"name2"),
                          Property(&FormFieldData::is_enabled, IsFalse())),
                    AllOf(Property(&FormFieldData::name, u"name3"),
                          Property(&FormFieldData::is_enabled, IsTrue())),
                    AllOf(Property(&FormFieldData::name, u"name4"),
                          Property(&FormFieldData::is_enabled, IsFalse()))))));
}

TEST_F(FormAutofillUtilsTest, IsReadonly) {
  LoadHTML(
      "<input type='text' id='name1'>"
      "<input readonly type='password' id='name2'>"
      "<input type='password' id='name3'>"
      "<input type='text' id='name4' readonly>");
  std::optional<FormData> form = *ExtractFormData(WebFormElement());
  EXPECT_THAT(
      form, Optional(Property(
                &FormData::fields,
                ElementsAre(
                    AllOf(Property(&FormFieldData::name, u"name1"),
                          Property(&FormFieldData::is_readonly, IsFalse())),
                    AllOf(Property(&FormFieldData::name, u"name2"),
                          Property(&FormFieldData::is_readonly, IsTrue())),
                    AllOf(Property(&FormFieldData::name, u"name3"),
                          Property(&FormFieldData::is_readonly, IsFalse())),
                    AllOf(Property(&FormFieldData::name, u"name4"),
                          Property(&FormFieldData::is_readonly, IsTrue()))))));
}

TEST_F(FormAutofillUtilsTest, IsFocusable) {
  LoadHTML(
      "<input type='text' id='name1' value='123'>"
      "<input type='text' id='name2' style='display:none'>");
  std::optional<FormData> form = *ExtractFormData(WebFormElement());
  EXPECT_THAT(
      form,
      Optional(Property(
          &FormData::fields,
          ElementsAre(
              AllOf(Property(&FormFieldData::name, u"name1"),
                    Property(&FormFieldData::is_focusable, IsTrue())),
              AllOf(Property(&FormFieldData::name, u"name2"),
                    Property(&FormFieldData::is_focusable, IsFalse()))))));
}

TEST_F(FormAutofillUtilsTest, FindFormByUniqueId) {
  LoadHTML("<body><form id='form1'></form><form id='form2'></form></body>");
  WebVector<WebFormElement> forms = GetDocument().Forms();

  for (const auto& form : forms)
    EXPECT_EQ(form, GetFormByRendererId(GetFormRendererId(form)));

  // Expect null form element for non-existing form id.
  FormRendererId non_existing_form_id(GetFormRendererId(forms[0]).value() +
                                      1000);
  EXPECT_FALSE(GetFormByRendererId(non_existing_form_id));
}

// Used in ParameterizedGetFormControlByRendererIdTest.
struct FindFormControlTestParam {
  std::string queried_field;
  bool expectation;
};

// Tests GetFormControlByRendererId().
class ParameterizedGetFormControlByRendererIdTest
    : public FormAutofillUtilsTest,
      public testing::WithParamInterface<FindFormControlTestParam> {};

TEST_P(ParameterizedGetFormControlByRendererIdTest,
       GetFormControlByRendererId) {
  LoadHTML(R"(
    <body>
      <input id="nonexistentField">
      <form id="form1"><input id="ownedField1"></form>
      <form id="form2"><input id="ownedField2"></form>
      <input id="unownedField">
    </body>
  )");

  WebFormControlElement queried_field =
      GetFormControlElementById(GetDocument(), GetParam().queried_field);
  FieldRendererId queried_field_id = GetFieldRendererId(queried_field);

  ExecuteJavaScriptForTests(
      R"(document.getElementById('nonexistentField').remove();)");
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(GetParam().expectation,
            queried_field == GetFormControlByRendererId(queried_field_id));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ParameterizedGetFormControlByRendererIdTest,
    Values(FindFormControlTestParam{"nonexistentField", false},
           FindFormControlTestParam{"ownedField1", true},
           FindFormControlTestParam{"ownedField2", true},
           FindFormControlTestParam{"unownedField", true}));

// Tests the extraction of the aria-label attribute.
TEST_F(FormAutofillUtilsTest, GetAriaLabel) {
  LoadHTML("<input id='input' type='text' aria-label='the label'/>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaLabelForTesting(doc, element), u"the label");
}

// Tests that aria-labelledby works. Simple case: only one id referenced.
TEST_F(FormAutofillUtilsTest, GetAriaLabelledBySingle) {
  LoadHTML(
      "<div id='billing'>Billing</div>"
      "<div>"
      "    <div id='name'>Name</div>"
      "    <input id='input' type='text' aria-labelledby='name'/>"
      "</div>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaLabelForTesting(doc, element), u"Name");
}

// Tests that aria-labelledby works: Complex case: multiple ids referenced.
TEST_F(FormAutofillUtilsTest, GetAriaLabelledByMulti) {
  LoadHTML(
      "<div id='billing'>Billing</div>"
      "<div>"
      "    <div id='name'>Name</div>"
      "    <input id='input' type='text' aria-labelledby='billing name'/>"
      "</div>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaLabelForTesting(doc, element), u"Billing Name");
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

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaLabelForTesting(doc, element), u"Name");
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

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaLabelForTesting(doc, element), u"");
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

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaLabelForTesting(doc, element), u"valid");
}

// Tests that aria-describedby works: Simple case: a single id referenced.
TEST_F(FormAutofillUtilsTest, GetAriaDescribedBySingle) {
  LoadHTML(
      "<input id='input' type='text' aria-describedby='div1'/>"
      "<div id='div1'>aria description</div>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaDescriptionForTesting(doc, element), u"aria description");
}

// Tests that aria-describedby works: Complex case: multiple ids referenced.
TEST_F(FormAutofillUtilsTest, GetAriaDescribedByMulti) {
  LoadHTML(
      "<input id='input' type='text' aria-describedby='div1 div2'/>"
      "<div id='div2'>description</div>"
      "<div id='div1'>aria</div>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaDescriptionForTesting(doc, element), u"aria description");
}

// Tests that invalid aria-describedby returns the empty string.
TEST_F(FormAutofillUtilsTest, GetAriaDescribedByInvalid) {
  LoadHTML("<input id='input' type='text' aria-describedby='invalid'/>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaDescriptionForTesting(doc, element), u"");
}

// Tests IsOwnedByFrame().
TEST_F(FormAutofillUtilsTest, IsOwnedByFrame) {
  LoadHTML(R"(
    <body>
      <div id="div"></div>
      <iframe id="child_frame"></iframe>
    </body>
  )");

  WebDocument doc = GetDocument();
  content::RenderFrame* main_frame = GetMainRenderFrame();
  content::RenderFrame* child_frame = GetIframeById(doc, "child_frame");
  WebElement div = GetElementById(doc, "div");

  EXPECT_FALSE(IsOwnedByFrame(WebElement(), /*frame=*/nullptr));
  EXPECT_FALSE(IsOwnedByFrame(WebElement(), main_frame));
  EXPECT_FALSE(IsOwnedByFrame(div, /*frame=*/nullptr));
  EXPECT_FALSE(IsOwnedByFrame(div, child_frame));
  EXPECT_TRUE(IsOwnedByFrame(div, main_frame));
  ExecuteJavaScriptForTests(R"(document.getElementById('div').remove();)");
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(IsOwnedByFrame(div, main_frame));
}

TEST_F(FormAutofillUtilsTest, ExtractFormData_IsActionEmptyFalse) {
  LoadHTML(
      "<body><form id='form1' action='done.html'><input "
      "id='i1'></form></body>");
  WebDocument doc = GetDocument();
  auto web_form = GetFormElementById(doc, "form1");

  FormData form_data = *ExtractFormData(web_form);
  EXPECT_FALSE(form_data.is_action_empty());
}

TEST_F(FormAutofillUtilsTest, ExtractFormData_IsActionEmptyTrue) {
  LoadHTML("<body><form id='form1'><input id='i1'></form></body>");
  WebDocument doc = GetDocument();
  auto web_form = GetFormElementById(doc, "form1");

  FormData form_data = *ExtractFormData(web_form);
  EXPECT_TRUE(form_data.is_action_empty());
}

TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_ExtractBounds) {
  LoadHTML("<body><form id='form1'><input id='i1'></form></body>");
  WebDocument doc = GetDocument();
  auto web_control = GetFormControlElementById(doc, "i1");
  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
      form_and_field = FindFormAndFieldForFormControlElement(
          web_control, {ExtractOption::kBounds});

  ASSERT_TRUE(form_and_field);
  auto& [form, field] = *form_and_field;
  EXPECT_FALSE(form.fields().back().bounds().IsEmpty());
}

TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_NotExtractBounds) {
  LoadHTML("<body><form id='form1'><input id='i1'></form></body>");
  WebDocument doc = GetDocument();
  auto web_control = GetFormControlElementById(doc, "i1");
  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
      form_and_field = FindFormAndFieldForFormControlElement(web_control);

  ASSERT_TRUE(form_and_field);
  auto& [form, field] = *form_and_field;
  EXPECT_TRUE(form.fields().back().bounds().IsEmpty());
}

TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_ExtractUnownedBounds) {
  LoadHTML("<body><input id='i1'></body>");
  WebDocument doc = GetDocument();
  auto web_control = GetFormControlElementById(doc, "i1");
  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
      form_and_field = FindFormAndFieldForFormControlElement(
          web_control, {ExtractOption::kBounds});

  ASSERT_TRUE(form_and_field);
  auto& [form, field] = *form_and_field;
  EXPECT_FALSE(form.fields().back().bounds().IsEmpty());
}

TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_GetDataListOptions) {
  LoadHTML(
      "<body><input list='datalist_id' name='count' id='i1'><datalist "
      "id='datalist_id'><option value='1'><option "
      "value='2'></datalist></body>");
  WebDocument doc = GetDocument();
  auto web_control = GetElementById(doc, "i1").To<WebInputElement>();
  std::vector<SelectOption> options = GetDataListOptionsForTesting(web_control);
  ASSERT_EQ(options.size(), 2u);
  EXPECT_EQ(options[0].value, u"1");
  EXPECT_EQ(options[1].value, u"2");
  EXPECT_EQ(options[0].text, u"");
  EXPECT_EQ(options[1].text, u"");
}

TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_GetDataListOptionsWithLabels) {
  LoadHTML(
      "<body><input list='datalist_id' name='count' id='i1'><datalist "
      "id='datalist_id'><option value='1'>one</option><option "
      "value='2'>two</option></datalist></body>");
  WebDocument doc = GetDocument();
  auto web_control = GetElementById(doc, "i1").To<WebInputElement>();
  std::vector<SelectOption> options = GetDataListOptionsForTesting(web_control);
  ASSERT_EQ(options.size(), 2u);
  EXPECT_EQ(options[0].value, u"1");
  EXPECT_EQ(options[1].value, u"2");
  EXPECT_EQ(options[0].text, u"one");
  EXPECT_EQ(options[1].text, u"two");
}

TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_ExtractDataList) {
  LoadHTML(
      "<body><input list='datalist_id' name='count' id='i1'><datalist "
      "id='datalist_id'><option value='1'>one</option><option "
      "value='2'>two</option></datalist></body>");
  WebDocument doc = GetDocument();
  auto web_control = GetElementById(doc, "i1").To<WebInputElement>();
  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
      form_and_field = FindFormAndFieldForFormControlElement(
          web_control, {ExtractOption::kDatalist});

  ASSERT_TRUE(form_and_field);
  auto& [form, field] = *form_and_field;
  auto& options = form.fields().back().datalist_options();
  ASSERT_EQ(options.size(), 2u);
  EXPECT_EQ(options[0].value, u"1");
  EXPECT_EQ(options[1].value, u"2");
  EXPECT_EQ(options[0].text, u"one");
  EXPECT_EQ(options[1].text, u"two");
  EXPECT_EQ(field->datalist_options().size(), options.size());
}

TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_NotExtractDataList) {
  LoadHTML(
      "<body><input list='datalist_id' name='count' id='i1'><datalist "
      "id='datalist_id'><option value='1'>one</option><option "
      "value='2'>two</option></datalist></body>");
  WebDocument doc = GetDocument();
  auto web_control = GetElementById(doc, "i1").To<WebInputElement>();
  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
      form_and_field = FindFormAndFieldForFormControlElement(
          web_control, {ExtractOption::kBounds});

  ASSERT_TRUE(form_and_field);
  auto& [form, field] = *form_and_field;
  EXPECT_TRUE(form.fields().back().datalist_options().empty());
}

TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_Disconnected) {
  LoadHTML(R"(<input name=count id=t>)");
  WebDocument doc = GetDocument();
  auto form_control = GetElementById(doc, "t").To<WebInputElement>();
  ExecuteJavaScriptForTests(R"(document.getElementById('t').remove();)");
  EXPECT_EQ(FindFormAndFieldForFormControlElement(form_control), std::nullopt);
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
    WebDocument doc = GetDocument();
    std::vector<WebElement> result;
    WebElementCollection iframes = doc.GetElementsByHTMLTagName("iframe");
    for (WebElement iframe = iframes.FirstItem(); iframe;
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
      EXPECT_EQ(IsVisibleIframeForTesting(iframe), expectation);
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

// Tests the visibility detection of fields.
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
    WebDocument doc = GetDocument();
    std::vector<WebElement> result;
    WebElementCollection inputs = doc.GetElementsByHTMLTagName("input");
    for (WebElement input = inputs.FirstItem(); input;
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
      EXPECT_EQ(IsWebElementVisibleForTesting(input), expectation);
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

  WebDocument doc = GetDocument();
  EXPECT_EQ(
      GetClosestAncestorFormElementForTesting(GetElementById(doc, "unowned")),
      WebFormElement());
  EXPECT_EQ(
      GetClosestAncestorFormElementForTesting(GetElementById(doc, "owned1")),
      GetFormElementById(doc, "outer_form"));
  EXPECT_EQ(
      GetClosestAncestorFormElementForTesting(GetElementById(doc, "owned2")),
      GetFormElementById(doc, "inner_form"));
  EXPECT_EQ(
      GetClosestAncestorFormElementForTesting(GetElementById(doc, "owned3")),
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
      WebDocument doc = GetDocument();
      WebNode lhs = GetElementById(doc, test.lhs_id);
      WebNode rhs = GetElementById(doc, test.rhs_id);
      WebNode ancestor_hint = ancestor_hint_id.empty()
                                  ? WebNode()
                                  : GetElementById(doc, ancestor_hint_id);
      EXPECT_EQ(test.lhs_id < test.rhs_id,
                IsDOMPredecessorForTesting(lhs, rhs, ancestor_hint));
      EXPECT_EQ(test.rhs_id < test.lhs_id,
                IsDOMPredecessorForTesting(rhs, lhs, ancestor_hint));
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
    : public FormAutofillUtilsTest,
      public testing::WithParamInterface<FieldFramesTestParam> {
 public:
  FieldFramesTest() = default;
  ~FieldFramesTest() override = default;
};

// Check if the unowned form control elements are properly extracted.
// Form control elements are button, fieldset, input, textarea, output and
// select elements.
TEST_F(FormAutofillUtilsTest, GetFormFieldElements_Unowned) {
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

  WebDocument doc = GetDocument();
  std::vector<WebFormControlElement> unowned_form_fields =
      form_util::GetOwnedFormControlsForTesting(doc, WebFormElement());

  EXPECT_THAT(unowned_form_fields,
              ElementsAre(GetFormControlElementById(doc, "unowned_button"),
                          GetFormControlElementById(doc, "unowned_fieldset"),
                          GetFormControlElementById(doc, "unowned_input"),
                          GetFormControlElementById(doc, "unowned_textarea"),
                          GetFormControlElementById(doc, "unowned_output"),
                          GetFormControlElementById(doc, "unowned_select")));
}

// Tests that FormData::fields and FormData::child_frames are extracted fully
// and in the correct relative order.
TEST_P(FieldFramesTest, ExtractFormData_ExtractFieldsAndFrames) {
  FieldFramesTestParam test_case = GetParam();
  SCOPED_TRACE(testing::Message() << "HTML: " << test_case.html);
  LoadHTML(test_case.html.c_str());
  WebDocument doc = GetDocument();

  // Extract the |form_data|.
  auto form_element = test_case.form_id
                          ? GetFormElementById(doc, test_case.form_id)
                          : WebFormElement();
  FormRendererId host_form = GetFormRendererId(form_element);
  std::optional<FormData> form_data = ExtractFormData(form_element);
  ASSERT_TRUE(form_data);

  // Check that all fields and iframes were extracted.
  EXPECT_EQ(form_data->fields().size() + form_data->child_frames().size(),
            test_case.fields_and_frames.size());

  // Check that all fields were extracted. Do so by checking for each |field| in
  // `test_case.fields_and_frames` that the DOM element with ID `field.id`
  // corresponds to the next (`i`th) field in `form_data->fields`.
  size_t i = 0;
  for (const FieldOrFrame& field : test_case.fields_and_frames) {
    if (field.is_frame)
      continue;
    SCOPED_TRACE(testing::Message() << "Checking the " << i
                                    << "th field (id = " << field.id << ")");
    WebElement element = GetElementById(doc, field.id);
    ASSERT_TRUE(element);
    ASSERT_TRUE(element.IsFormControlElement());
    EXPECT_EQ(form_data->fields()[i].host_form_id(), host_form);
    EXPECT_TRUE(HaveSameFormControlId(element.To<WebFormControlElement>(),
                                      form_data->fields()[i]));
    ++i;
  }

  // Check that all frames were extracted into `form_data->child_frames`
  // analogously to the above loop for `form_data->fields`.
  //
  // In addition, check that `form_data->child_frames[i].predecessor` encodes
  // the correct ordering, i.e., that `form_data->child_frames[i].predecessor`
  // is the index of the last field in `form_data->fields` that precedes the
  // `i`th frame in `form_data->child_frames`.
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
    EXPECT_FALSE(absl::visit(is_empty, form_data->child_frames()[i].token));
    EXPECT_EQ(form_data->child_frames()[i].token, GetFrameToken(doc, frame.id));
    EXPECT_EQ(form_data->child_frames()[i].predecessor, preceding_field_index);
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
          html +=
              field_or_frame.is_frame
                  ? base::StringPrintf("<iframe id='%s'></iframe>",
                                       field_or_frame.id)
                  : base::StringPrintf("<input id='%s'>", field_or_frame.id);
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
      DCHECK(std::ranges::all_of(cases, [&](const auto& case1) {
        return std::ranges::all_of(cases, [&](const auto& case2) {
          return &case1 == &case2 || case1.html != case2.html;
        });
      }));
      return cases;
    }()));

TEST_F(FormAutofillUtilsTest, ExtractFormData_WebFormElementToFormData) {
  LoadHTML(R"(
    <form id='form'>
      <input id='input'>
      <select name='form_select' id='select'>
        <option value='june'>june</option>
        <option value='july' selected>july</option>
      </select>
    </form>
  )");

  WebDocument doc = GetDocument();

  auto form_element = GetFormElementById(doc, "form");
  FormData form_data = *ExtractFormData(form_element);
  EXPECT_EQ(form_data.fields().size(), 2u);

  {
    WebElement element = GetElementById(doc, "input");
    ASSERT_TRUE(element);
    ASSERT_TRUE(element.IsFormControlElement());
    EXPECT_TRUE(HaveSameFormControlId(element.To<WebFormControlElement>(),
                                      form_data.fields()[0]));
  }

  WebElement element = GetElementById(doc, "select");
  ASSERT_TRUE(element);
  ASSERT_TRUE(element.IsFormControlElement());
  EXPECT_TRUE(HaveSameFormControlId(element.To<WebFormControlElement>(),
                                    form_data.fields()[1]));
}

// Tests that if the number of iframes exceeds kMaxExtractableChildFrames,
// child frames of that form are not extracted.
TEST_F(FormAutofillUtilsTest, ExtractFormData_ExtractNoFramesIfTooManyIframes) {
  auto CreateFormElement = [this](const char* element) {
    std::string js = base::StringPrintf(
        "document.forms[0].appendChild(document.createElement('%s'))", element);
    ExecuteJavaScriptForTests(js.c_str());
  };

  LoadHTML(R"(<html><body><form id='f'></form>)");
  for (size_t i = 0; i < kMaxExtractableFields - 1; ++i) {
    CreateFormElement("input");
  }
  for (size_t i = 0; i < kMaxExtractableChildFrames; ++i) {
    CreateFormElement("iframe");
  }

  // Ensure that Android runs at default page scale.
  web_view_->SetPageScaleFactor(1.0);

  WebDocument doc = GetDocument();
  WebFormElement form = GetFormElementById(doc, "f");
  {
    FormData form_data = *ExtractFormData(form);
    EXPECT_EQ(form_data.fields().size(), kMaxExtractableFields - 1);
    EXPECT_EQ(form_data.child_frames().size(), kMaxExtractableChildFrames);
  }

  // There may be multiple checks (e.g., == kMaxExtractableChildFrames, <=
  // kMaxExtractableChildFrames, < kMaxExtractableChildFrames), so we test
  // different numbers of <iframe> elements.
  for (int i = 0; i < 3; ++i) {
    CreateFormElement("iframe");
    FormData form_data = *ExtractFormData(form);
    EXPECT_EQ(form_data.fields().size(), kMaxExtractableFields - 1);
    EXPECT_TRUE(form_data.child_frames().empty());
  }
}

// Tests that if the number of fields exceeds |kMaxExtractableFields|, neither
// fields nor child frames of that form are extracted.
TEST_F(FormAutofillUtilsTest, ExtractNoFieldsOrFramesIfTooManyFields) {
  auto CreateFormElement = [this](const char* element) {
    std::string js = base::StringPrintf(
        "document.forms[0].appendChild(document.createElement('%s'))", element);
    ExecuteJavaScriptForTests(js.c_str());
  };

  LoadHTML(R"(<html><body><form id='f'></form>)");
  for (size_t i = 0; i < kMaxExtractableFields - 1; ++i) {
    CreateFormElement("input");
  }
  for (size_t i = 0; i < kMaxExtractableChildFrames; ++i) {
    CreateFormElement("iframe");
  }

  // Ensure that Android runs at default page scale.
  web_view_->SetPageScaleFactor(1.0);

  WebDocument doc = GetDocument();
  WebFormElement form = GetFormElementById(doc, "f");
  {
    FormData form_data = *ExtractFormData(form);
    EXPECT_EQ(form_data.fields().size(), kMaxExtractableFields - 1);
    EXPECT_EQ(form_data.child_frames().size(), kMaxExtractableChildFrames);
  }

  // There may be multiple checks (e.g., == kMaxExtractableFields, <=
  // kMaxExtractableFields, < kMaxExtractableFields), so we test different
  // numbers of <input> elements.
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(base::NumberToString(i));
    CreateFormElement("input");
    ASSERT_FALSE(ExtractFormData(form));
  }
}

// Verifies that the callback happens even if no sequences of 4 digits are
// found.
TEST_F(FormAutofillUtilsTest, TraverseDomForFourDigitCombinations_NoMatches) {
  std::vector<std::string> matches = {"dummy data"};
  LoadHTML(R"(123 444)");
  WebDocument document = GetDocument();
  TraverseDomForFourDigitCombinations(
      document, base::BindLambdaForTesting(
                    [&](const std::vector<std::string>& regex_search) {
                      matches = regex_search;
                    }));
  EXPECT_THAT(matches, IsEmpty());
}

// Verifies that the matches correctly returns all four digit combinations.
TEST_F(FormAutofillUtilsTest,
       TraverseDomForFourDigitCombinations_MatchesFound) {
  std::vector<std::string> matches;
  LoadHTML(R"(
    <body>
      <p>1234 ****2345 **3456 **** 4567 ●●●●5678 </p>
      <form>
        <input>
      </form>
    </body>)");
  WebDocument document = GetDocument();
  TraverseDomForFourDigitCombinations(
      document, base::BindLambdaForTesting(
                    [&](const std::vector<std::string>& regex_search) {
                      matches = regex_search;
                    }));
  EXPECT_THAT(matches, ElementsAre("1234", "2345", "3456", "4567", "5678"));

  LoadHTML(R"(
    <form>Enter your CVC for card 2345:
      <input type="text">
    </form>)");
  document = GetDocument();
  TraverseDomForFourDigitCombinations(
      document, base::BindLambdaForTesting(
                    [&](const std::vector<std::string>& regex_search) {
                      matches = regex_search;
                    }));
  EXPECT_THAT(matches, ElementsAre("2345"));

  LoadHTML(R"(
    <table>
      <tr>
        <td>Enter your CVC for card 2345</td>
        <td>
            <form><input type="text"></form>
        </td>
      </tr>
    </table>)");
  document = GetDocument();
  TraverseDomForFourDigitCombinations(
      document, base::BindLambdaForTesting(
                    [&](const std::vector<std::string>& regex_search) {
                      matches = regex_search;
                    }));
  EXPECT_THAT(matches, ElementsAre("2345"));
}

// Ensure that we don't return duplicate values.
TEST_F(FormAutofillUtilsTest,
       TraverseDomForFourDigitCombinations_MatchesFoundWithDuplicates) {
  std::vector<std::string> matches;
  LoadHTML(R"(
    <body>
      <p>1234 ****1234 **1234 **** 1234 ····1234 ●●●●1234</p>
      <form>
          <input></input>
      </form>
    </body>)");
  WebDocument document = GetDocument();
  TraverseDomForFourDigitCombinations(
      document, base::BindLambdaForTesting(
                    [&](const std::vector<std::string>& regex_search) {
                      matches = regex_search;
                    }));
  // After deduping, we only have one final match.
  EXPECT_THAT(matches, ElementsAre("1234"));
}

// Ensures that we correctly perform checks on the last four digit combinations
// for year values.
TEST_F(FormAutofillUtilsTest,
       TraverseDomForFourDigitCombinations_YearsRemoved) {
  std::vector<std::string> matches = {"dummy_data"};
  LoadHTML(R"(
    <body>
      <form>
          <p>1999 2000 1234 2001 2002 2003 2004</p>
      </form>
    </body>)");
  WebDocument document = GetDocument();
  TraverseDomForFourDigitCombinations(
      document, base::BindLambdaForTesting(
                    [&](const std::vector<std::string>& regex_search) {
                      matches = regex_search;
                    }));
  // We have no matches as they are years.
  EXPECT_THAT(matches, IsEmpty());

  LoadHTML(R"(
    <body>
      <form>
        <select>
          <option value="1998">1998</option>
          <option value="1999">1999</option>
          <option value="2000">2000</option>
          <option value="2001">2001</option>
          <option value="2002">2002</option>
        </select>
      </form>
    </body>)");
  document = GetDocument();
  TraverseDomForFourDigitCombinations(
      document, base::BindLambdaForTesting(
                    [&](const std::vector<std::string>& regex_search) {
                      matches = regex_search;
                    }));
  // We have no matches as there are more than two years.
  EXPECT_THAT(matches, IsEmpty());

  LoadHTML(R"(
    <body>
      <form>
        <select>
          <option value="1999">1999</option>
          <option value="2000">2000</option>
          <option value="4545">4545</option>
          <option value="6782">6782</option>
        </select>
      </form>
    </body>)");
  document = GetDocument();
  TraverseDomForFourDigitCombinations(
      document, base::BindLambdaForTesting(
                    [&](const std::vector<std::string>& regex_search) {
                      matches = regex_search;
                    }));
  // We keep all four matches as there are potential years but not enough to
  // disqualify.
  EXPECT_THAT(matches, ElementsAre("1999", "2000", "4545", "6782"));
}

MATCHER(SameNode, "") {
  return std::get<0>(arg).Equals(std::get<1>(arg));
}

void PrefixTraverseAndAppend(WebNode node, std::vector<WebNode>& out) {
  out.push_back(node);
  for (WebNode child = node.FirstChild(); child; child = child.NextSibling()) {
    PrefixTraverseAndAppend(child, out);
  }
}

// Tests that the appropriate web node is returned when iterating through the
// web DOM in forward direction.
TEST_F(FormAutofillUtilsTest, NextWebNode_Forward) {
  LoadHTML(R"(
    <html>
      <head></head>
      <body>
        <div>
          <div>
            <div>A</div>
            <div>B</div>
          </div>
          <div>
            <div>C</div>
            <div>D</div>
            <div>E</div>
          </div>
          <div>
            <div>F</div>
            <div>G</div>
          </div>
        </div>
      </body>
    </html>)");
  std::vector<WebNode> expected_elements;
  PrefixTraverseAndAppend(GetDocument(), expected_elements);

  std::vector<WebNode> found_elements;
  for (WebNode node = GetDocument(); node;
       node = NextWebNodeForTesting(node, /*forward=*/true)) {
    found_elements.push_back(node);
  }

  EXPECT_THAT(found_elements, Pointwise(SameNode(), expected_elements));
}

// Tests that the appropriate web node is returned when iterating through the
// web DOM in backwards direction.
TEST_F(FormAutofillUtilsTest, NextWebNode_Backward) {
  LoadHTML(R"(
    <html>
      <head></head>
      <body>
        <div>
          <div>
            <div>A</div>
            <div>B</div>
          </div>
          <div>
            <div>C</div>
            <div>D</div>
            <div>E</div>
          </div>
          <div>
            <div>F</div>
            <div>G</div>
          </div>
        </div>
      </body>
    </html>)");
  std::vector<WebNode> expected_elements;
  PrefixTraverseAndAppend(GetDocument(), expected_elements);
  std::reverse(expected_elements.begin(), expected_elements.end());

  std::vector<WebNode> found_elements;
  for (WebNode node = expected_elements[0]; node;
       node = NextWebNodeForTesting(node, /*forward=*/false)) {
    found_elements.push_back(node);
  }

  EXPECT_THAT(found_elements, Pointwise(SameNode(), expected_elements));
}

// Tests that GetMaxLength() of non-text form controls is 0, and text form
// controls default to the maximum 32 bit integer (and *not* 64 bit integer, so
// that we can still do arithmetic with the maximum length).
TEST_F(FormAutofillUtilsTest, GetMaxLength) {
  struct TestCase {
    const char* html;
    uint64_t expected_max_length;
  };
  static constexpr TestCase test_cases[] = {
      {"<input id=field>", FormFieldData::kDefaultMaxLength},
      {"<input id=field type=text>", FormFieldData::kDefaultMaxLength},
      {"<input id=field type=text maxlength=-1>",
       FormFieldData::kDefaultMaxLength},
      {"<input id=field type=password>", FormFieldData::kDefaultMaxLength},
      {"<input id=field type=text maxlength=123>", 123},
      {"<textarea id=field>", FormFieldData::kDefaultMaxLength},
      {"<textarea id=field maxlength=123>", 123},
      {"<input id=field type=submit>", 0},
      {"<select id=field></select>", 0},
  };
  for (auto test_case : test_cases) {
    SCOPED_TRACE(test_case.html);
    LoadHTML(test_case.html);
    WebFormControlElement field = GetElementById(GetDocument(), "field")
                                      .DynamicTo<WebFormControlElement>();
    EXPECT_TRUE(field);
    EXPECT_EQ(test_case.expected_max_length, GetMaxLengthForTesting(field));
  }
}

TEST_F(FormAutofillUtilsTest, ContentEditableWritingSuggestionsFalseInherited) {
  LoadHTML(
      R"(<body writingsuggestions=false>
         <div id=my-id contenteditable></div>
         </body>)");
  WebElement content_editable = GetDocument().GetElementById("my-id");
  ASSERT_TRUE(content_editable);
  std::optional<FormData> form = FindFormForContentEditable(content_editable);
  ASSERT_EQ(form->fields().size(), 1u);
  const FormFieldData& field = form->fields()[0];
  EXPECT_FALSE(field.allows_writing_suggestions());
}

TEST_F(FormAutofillUtilsTest, ContentEditableWritingSuggestionsFalse) {
  LoadHTML(
      R"(<body>
         <div id=my-id writingsuggestions=false contenteditable></div>
         </body>)");
  WebElement content_editable = GetDocument().GetElementById("my-id");
  ASSERT_TRUE(content_editable);
  std::optional<FormData> form = FindFormForContentEditable(content_editable);
  ASSERT_EQ(form->fields().size(), 1u);
  const FormFieldData& field = form->fields()[0];
  EXPECT_FALSE(field.allows_writing_suggestions());
}

TEST_F(FormAutofillUtilsTest, FindFormForContentEditableSuccess) {
  LoadHTML(
      R"(<body>
         <div id=my-id
              name=my-name
              class=my-class
              autocomplete=given-name
              contenteditable>
            This is the <code>textContent</code>!
         </div>
         </body>)");
  WebElement content_editable = GetDocument().GetElementById("my-id");
  ASSERT_TRUE(content_editable);
  std::optional<FormData> form = FindFormForContentEditable(content_editable);
  ASSERT_EQ(form->fields().size(), 1u);
  const FormFieldData& field = form->fields()[0];
  EXPECT_TRUE(form->renderer_id());
  EXPECT_EQ(*form->renderer_id(), *field.renderer_id());
  EXPECT_EQ(form->renderer_id(), field.host_form_id());
  EXPECT_EQ(field.parsed_autocomplete()->field_type, HtmlFieldType::kGivenName);
  EXPECT_EQ(field.name(), u"my-id");
  EXPECT_EQ(field.id_attribute(), u"my-id");
  EXPECT_EQ(field.name_attribute(), u"my-name");
  EXPECT_EQ(field.css_classes(), u"my-class");
  EXPECT_EQ(field.value(),
            u"\n            This is the textContent!\n         ");
  EXPECT_TRUE(field.allows_writing_suggestions());
}

TEST_F(FormAutofillUtilsTest, FindFormForContentEditableAbridgedSuccess) {
  // HTML with 1500 characters of pi in the contenteditable div
  LoadHTML(
      R"(<body>
         <div id=my-id
              name=my-name
              class=my-class
              autocomplete=given-name
              contenteditable>3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679821480865132823066470938446095505822317253594081284811174502841027019385211055596446229489549303819644288109756659334461284756482337867831652712019091456485669234603486104543266482133936072602491412737245870066063155881748815209209628292540917153643678925903600113305305488204665213841469519415116094330572703657595919530921861173819326117931051185480744623799627495673518857527248912279381830119491298336733624406566430860213949463952247371907021798609437027705392171762931767523846748184676694051320005681271452635608277857713427577896091736371787214684409012249534301465495853710507922796892589235420199561121290219608640344181598136297747713099605187072113499999983729780499510597317328160963185950244594553469083026425223082533446850352619311881710100031378387528865875332083814206171776691473035982534904287554687311595628638823537875937519577818577805321712268066130019278766111959092164201989380952572010654858632788659361533818279682303019520353018529689957736225994138912497217752834791315155748572424541506959508295331168617278558890750983817546374649393192550604009277016711390098488240128583616035637076601047101819429555961989467678374494482553797747268471040475346462080466842590694912933136770289891521047521620569660240580381501935112533824300355876402474964732639141992726042699227967823547816360093417216412199245863150302861829745557067498385054945885869269956909272107975093029</div>
         </body>)");
  WebElement content_editable = GetDocument().GetElementById("my-id");
  ASSERT_TRUE(content_editable);
  std::optional<FormData> form = FindFormForContentEditable(content_editable);
  ASSERT_EQ(form->fields().size(), 1u);
  const FormFieldData& field = form->fields()[0];
  EXPECT_TRUE(form->renderer_id());
  EXPECT_EQ(*form->renderer_id(), *field.renderer_id());
  EXPECT_EQ(form->renderer_id(), field.host_form_id());
  EXPECT_EQ(field.parsed_autocomplete()->field_type, HtmlFieldType::kGivenName);
  EXPECT_EQ(field.name(), u"my-id");
  EXPECT_EQ(field.id_attribute(), u"my-id");
  EXPECT_EQ(field.name_attribute(), u"my-name");
  EXPECT_EQ(field.css_classes(), u"my-class");
  // Only extract 1024 characters from the div.
  EXPECT_EQ(field.value().length(), 1024u);
  EXPECT_EQ(
      field.value(),
      u"3."
      u"14159265358979323846264338327950288419716939937510582097494459230781640"
      u"62862089986280348253421170679821480865132823066470938446095505822317253"
      u"59408128481117450284102701938521105559644622948954930381964428810975665"
      u"93344612847564823378678316527120190914564856692346034861045432664821339"
      u"36072602491412737245870066063155881748815209209628292540917153643678925"
      u"90360011330530548820466521384146951941511609433057270365759591953092186"
      u"11738193261179310511854807446237996274956735188575272489122793818301194"
      u"91298336733624406566430860213949463952247371907021798609437027705392171"
      u"76293176752384674818467669405132000568127145263560827785771342757789609"
      u"17363717872146844090122495343014654958537105079227968925892354201995611"
      u"21290219608640344181598136297747713099605187072113499999983729780499510"
      u"59731732816096318595024459455346908302642522308253344685035261931188171"
      u"01000313783875288658753320838142061717766914730359825349042875546873115"
      u"95628638823537875937519577818577805321712268066130019278766111959092164"
      u"2019893809525720106548586327");
}

TEST_F(FormAutofillUtilsTest, FindFormForContentEditableFailures) {
  LoadHTML(
      R"(<body>
         <div id=ce1></div>
         <div contenteditable><div id=ce2 contenteditable></div></div>
         <form id=ce3 contenteditable></form>
         <textarea id=ce4 contenteditable><div contenteditable></textarea>
         </body>)");
  WebDocument doc = GetDocument();
  ASSERT_FALSE(FindFormForContentEditable(doc.GetElementById("ce1")));
  ASSERT_FALSE(FindFormForContentEditable(doc.GetElementById("ce2")));
  ASSERT_FALSE(FindFormForContentEditable(doc.GetElementById("ce3")));
  ASSERT_FALSE(FindFormForContentEditable(doc.GetElementById("ce4")));
}

TEST_F(FormAutofillUtilsTest, ExtractFormData_OwnedForm) {
  base::HistogramTester histogram_tester;
  LoadHTML(R"(
      <html><title>Checkout</title></head>
      <form id=form_of_interest>
      <input type=text name=text_input>
      <input type=checkbox name=check_input>
      <input type=number name=number_input>
      <select name=select_input>
        <option value=option_1></option>
        <option value=option_2></option>
      </select>
      </form>
      <form><input type=text name=excluded/></form>
      </html>)");
  WebDocument doc = GetDocument();
  EXPECT_THAT(
      ExtractFormData(GetFormElementById(doc, "form_of_interest")),
      Optional(Property(
          &FormData::fields,
          ElementsAre(Property(&FormFieldData::name, u"text_input"),
                      Property(&FormFieldData::name, u"check_input"),
                      Property(&FormFieldData::name, u"number_input"),
                      Property(&FormFieldData::name, u"select_input")))));
  histogram_tester.ExpectTotalCount("Autofill.ExtractFormUnowned.FieldCount",
                                    0);
  histogram_tester.ExpectUniqueSample("Autofill.ExtractFormOwned.FieldCount", 4,
                                      1);
}

TEST_F(FormAutofillUtilsTest, ExtractFormData_UnownedForm) {
  base::HistogramTester histogram_tester;
  LoadHTML(R"(
      <html><title>Checkout</title></head>
      <input type=text name=text_input>
      <input type=checkbox name=check_input>
      <input type=number name=number_input>
      <select name=select_input>
        <option value=option_1></option>
        <option value=option_2></option>
      </select>
      <form><input type=text name=excluded/></form>
      </html>)");
  WebDocument doc = GetDocument();
  EXPECT_THAT(
      ExtractFormData(WebFormElement()),
      Optional(Property(
          &FormData::fields,
          ElementsAre(Property(&FormFieldData::name, u"text_input"),
                      Property(&FormFieldData::name, u"check_input"),
                      Property(&FormFieldData::name, u"number_input"),
                      Property(&FormFieldData::name, u"select_input")))));
  histogram_tester.ExpectTotalCount("Autofill.ExtractFormOwned.FieldCount", 0);
  histogram_tester.ExpectUniqueSample("Autofill.ExtractFormUnowned.FieldCount",
                                      4, 1);
}

// Tests that the owning form of a form control element in light DOM is its
// associated form (i.e. the form explicitly set via form attribute or its
// closest ancestor).
// Also tests that GetFormControlElements(f) == {t | GetOwningForm(t) == f} for
// every form f that owns some t.
TEST_F(FormAutofillUtilsTest, GetOwningFormInLightDom) {
  LoadHTML(R"(
    <html>
      <body>
        <form id=f>
          <input id=t1>
          <input id=t2>
        </form>
        <input id=t3>
      </body>
    </html>)");
  WebDocument doc = GetDocument();
  WebFormElement f = GetFormElementById(doc, "f");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(doc, "t1");
  WebFormControlElement t2 = GetFormControlElementById(doc, "t2");
  WebFormControlElement t3 = GetFormControlElementById(doc, "t3");
  EXPECT_EQ(GetOwningForm(t1), f);
  EXPECT_EQ(GetOwningForm(t2), f);
  EXPECT_EQ(GetOwningForm(t3), f_unowned);
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f), ElementsAre(t1, t2));
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f_unowned), ElementsAre(t3));
}

// Tests that explicit association overrules DOM ancestry when determining the
// owning form.
// Also tests that GetFormControlElements(f) == {t | GetOwningForm(t) == f} for
// every form f that owns some t.
TEST_F(FormAutofillUtilsTest, GetOwningFormInLightDomWithExplicitAssociation) {
  LoadHTML(R"(
    <html>
      <body>
        <div>
          <form id=f1>
            <input id=t1>
            <input id=t2 form=f2>
          </form>
        </div>
        <form id=f2>
          <input id=t3>
          <input id=t4 form=f1>
          <input id=t5 form=f_unowned>
        </form>
        <input id=t6 form=f1>
        <input id=t7 form=f2>
        <input id=t8>
      </body>
    </html>)");
  WebDocument doc = GetDocument();
  WebFormElement f1 = GetFormElementById(doc, "f1");
  WebFormElement f2 = GetFormElementById(doc, "f2");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(doc, "t1");
  WebFormControlElement t2 = GetFormControlElementById(doc, "t2");
  WebFormControlElement t3 = GetFormControlElementById(doc, "t3");
  WebFormControlElement t4 = GetFormControlElementById(doc, "t4");
  WebFormControlElement t5 = GetFormControlElementById(doc, "t5");
  WebFormControlElement t6 = GetFormControlElementById(doc, "t6");
  WebFormControlElement t7 = GetFormControlElementById(doc, "t7");
  WebFormControlElement t8 = GetFormControlElementById(doc, "t8");

  EXPECT_EQ(GetOwningForm(t1), f1);
  EXPECT_EQ(GetOwningForm(t2), f2);
  EXPECT_EQ(GetOwningForm(t3), f2);
  EXPECT_EQ(GetOwningForm(t4), f1);
  EXPECT_EQ(GetOwningForm(t5), f_unowned);
  EXPECT_EQ(GetOwningForm(t6), f1);
  EXPECT_EQ(GetOwningForm(t7), f2);
  EXPECT_EQ(GetOwningForm(t8), f_unowned);
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f1), ElementsAre(t1, t4, t6));
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f2), ElementsAre(t2, t3, t7));
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f_unowned),
              ElementsAre(t5, t8));
}

// Tests that input elements in shadow DOM whose closest ancestor is in the
// light DOM are extracted correctly.
// Also tests that GetFormControlElements(f) == {t | GetOwningForm(t) == f} for
// every form f that owns some t.
TEST_F(FormAutofillUtilsTest, GetOwningFormInShadowDomWithoutFormInShadowDom) {
  LoadHTML(R"(
    <html>
      <body>
        <form id=f1>
          <div id=host1>
            <template shadowrootmode=open>
              <div>
                <input id=t1>
              </div>
            </template>
            <input id=t2>
          </div>
        </form>
        <div id=host2>
          <template shadowrootmode=open>
            <input id=t3>
          </template>
        </div>
      </body>
    </html>)");
  WebDocument doc = GetDocument();
  WebNode shadow_root1 = GetElementById(doc, "host1").ShadowRoot();
  WebNode shadow_root2 = GetElementById(doc, "host2").ShadowRoot();
  WebFormElement f1 = GetFormElementById(doc, "f1");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(doc, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root2, "t3");

  EXPECT_EQ(GetOwningForm(t1), f1);
  EXPECT_EQ(GetOwningForm(t2), f1);
  EXPECT_EQ(GetOwningForm(t3), f_unowned);
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f1), ElementsAre(t1, t2));
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f_unowned), ElementsAre(t3));
}

// Tests that the owning form of a form control element is the furthest
// shadow-including ancestor form element (in absence of explicit associations).
// Also tests that GetFormControlElements(f) == {t | GetOwningForm(t) == f} for
// every form f that owns some t.
TEST_F(FormAutofillUtilsTest, GetOwningFormInShadowDomWithFormInShadowDom) {
  LoadHTML(R"(
    <html>
      <body>
        <form id=f1>
          <div id=host1>
            <template shadowrootmode=open>
              <div>
                <form id=f2>
                  <input id=t1>
                </form>
              </div>
              <input id=t2>
            </template>
          </div>
        </form>
        <div id=host2>
          <template shadowrootmode=open>
            <form id=f3>
              <input id=t3>
            </form>
          </template>
        </div>
      </body>
    </html>)");
  WebDocument doc = GetDocument();
  WebNode shadow_root1 = GetElementById(doc, "host1").ShadowRoot();
  WebNode shadow_root2 = GetElementById(doc, "host2").ShadowRoot();
  WebFormElement f1 = GetFormElementById(doc, "f1");
  WebFormElement f3 = GetFormElementById(shadow_root2, "f3");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(shadow_root1, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root2, "t3");

  EXPECT_EQ(GetOwningForm(t1), f1);
  EXPECT_EQ(GetOwningForm(t2), f1);
  EXPECT_EQ(GetOwningForm(t3), f3);
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f1), ElementsAre(t1, t2));
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f3), ElementsAre(t3));
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f_unowned), IsEmpty());
}

// Tests that the owning form is returned correctly even if there are multiple
// levels of Shadow DOM.
// Also tests that GetFormControlElements(f) == {t | GetOwningForm(t) == f} for
// every form f that owns some t.
TEST_F(FormAutofillUtilsTest,
       GetOwningFormInShadowDomWithFormInShadowDomWithMultipleLevels) {
  LoadHTML(R"(
    <html>
      <body>
        <form id=f1>
          <div id=host1>
            <template shadowrootmode=open>
              <form id=f2>
                <input id=t1>
              </form>
              <div id=host2>
                <template shadowrootmode=open>
                  <form id=f3>
                    <input id=t2>
                  </form>
                  <input id=t3>
                </template>
                <input id=t4>
              </div>
              <input id=t5>
            </template>
          </div>
        </form>
      </body>
    </html>)");
  WebDocument doc = GetDocument();
  WebNode shadow_root1 = GetElementById(doc, "host1").ShadowRoot();
  WebNode shadow_root2 = GetElementById(shadow_root1, "host2").ShadowRoot();
  WebFormElement f1 = GetFormElementById(doc, "f1");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(shadow_root2, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root2, "t3");
  WebFormControlElement t4 = GetFormControlElementById(shadow_root1, "t4");
  WebFormControlElement t5 = GetFormControlElementById(shadow_root1, "t5");

  EXPECT_EQ(GetOwningForm(t1), f1);
  EXPECT_EQ(GetOwningForm(t2), f1);
  EXPECT_EQ(GetOwningForm(t3), f1);
  EXPECT_EQ(GetOwningForm(t4), f1);
  EXPECT_EQ(GetOwningForm(t5), f1);
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f1),
              ElementsAre(t1, t2, t3, t4, t5));
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f_unowned), IsEmpty());
}

// Tests that the owning form is computed correctly for form control elements
// inside the shadow DOM that have explicit form attributes.
// Also tests that GetFormControlElements(f) == {t | GetOwningForm(t) == f} for
// every form f that owns some t.
TEST_F(FormAutofillUtilsTest,
       GetOwningFormInShadowDomWithFormInShadowDomAndExplicitAssociation) {
  LoadHTML(R"(
    <html>
      <body>
        <form id=f1>
          <div id=host1>
            <template shadowrootmode=open>
              <form id=f2>
                <input id=t1>
              </form>
              <input id=t2>
              <form id=f3>
                <input id=t3 form=f2>
              </form>
              <input id=t4 form=f2>
              <input id=t5 form=f3>
              <input id=t6 form=f1>
            </template>
          </div>
        </form>
        <div id=host2>
          <template shadowrootmode=open>
            <form id=f4>
              <input id=t7>
            </form>
          </template>
        </div>
      </body>
    </html>)");
  WebDocument doc = GetDocument();
  WebNode shadow_root1 = GetElementById(doc, "host1").ShadowRoot();
  WebNode shadow_root2 = GetElementById(doc, "host2").ShadowRoot();
  WebFormElement f1 = GetFormElementById(doc, "f1");
  WebFormElement f4 = GetFormElementById(shadow_root2, "f4");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(shadow_root1, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root1, "t3");
  WebFormControlElement t4 = GetFormControlElementById(shadow_root1, "t4");
  WebFormControlElement t5 = GetFormControlElementById(shadow_root1, "t5");
  WebFormControlElement t6 = GetFormControlElementById(shadow_root1, "t6");
  WebFormControlElement t7 = GetFormControlElementById(shadow_root2, "t7");

  EXPECT_EQ(GetOwningForm(t1), f1);
  EXPECT_EQ(GetOwningForm(t2), f1);
  EXPECT_EQ(GetOwningForm(t3), f1);
  EXPECT_EQ(GetOwningForm(t4), f1);
  EXPECT_EQ(GetOwningForm(t5), f1);
  EXPECT_EQ(GetOwningForm(t6), f1);
  EXPECT_EQ(GetOwningForm(t7), f4);
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f1),
              ElementsAre(t1, t2, t3, t4, t5, t6));
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f4), ElementsAre(t7));
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f_unowned), IsEmpty());
}

// Tests that the owning form is computed correctly for nested forms.
TEST_F(FormAutofillUtilsTest, GetOwningFormWithNestedFormsInLightDom) {
  LoadHTML(R"(
    <html>
      <body>
        <form id=f1>
        </form>
      </body>
    </html>)");
  // Specify the form using Javascript to avoid that the renderer flattens the
  // forms.
  ExecuteJavaScriptForTests(R"(
    var f2 = document.createElement('form');
    f2.id = 'f2';
    f1.appendChild(f2);

    var t1 = document.createElement('input');
    t1.id = 't1';
    f2.appendChild(t1);
  )");

  WebDocument doc = GetDocument();
  WebFormElement f1 = GetFormElementById(doc, "f1");
  WebFormControlElement t1 = GetFormControlElementById(doc, "t1");

  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f1), ElementsAre(t1));
  EXPECT_EQ(GetOwningForm(t1), f1);
}

// Tests that GetOwnedFormControls() doesn't return disconnected elements.
TEST_F(FormAutofillUtilsTest, GetOwnedFormControlsRequiresConnectedness) {
  LoadHTML(R"(
    <html>
      <body>
        <form id=f>
          <input id=t>
        </form>
      </body>
    </html>)");
  WebDocument doc = GetDocument();
  WebFormElement f = GetFormElementById(doc, "f");
  WebFormControlElement t = GetFormControlElementById(doc, "t");
  EXPECT_THAT(f.GetFormControlElements(), ElementsAre(t));  // nocheck
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f), ElementsAre(t));

  ExecuteJavaScriptForTests(R"(
    document.getElementById('f').remove();
  )");
  // Blink still gives us the disconnected element, but in Autofill we don't
  // want it.
  EXPECT_THAT(f.GetFormControlElements(), ElementsAre(t));  // nocheck
  EXPECT_THAT(GetOwnedFormControlsForTesting(doc, f), IsEmpty());
}

}  // namespace
}  // namespace autofill::form_util
