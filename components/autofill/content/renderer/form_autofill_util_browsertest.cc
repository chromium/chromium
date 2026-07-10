// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/focus_test_utils.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/synchronous_form_cache.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
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
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "third_party/blink/public/web/web_view.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/public/web/win/web_font_rendering.h"
#endif

namespace autofill::form_util {
namespace {

using ::autofill::mojom::ButtonTitleType;
using ::base::UTF8ToUTF16;
using ::blink::WebAutofillState;
using ::blink::WebDocument;
using ::blink::WebElement;
using ::blink::WebElementCollection;
using ::blink::WebFormControlElement;
using ::blink::WebFormElement;
using ::blink::WebInputElement;
using ::blink::WebLocalFrame;
using ::blink::WebNode;
using ::blink::WebSelectElement;
using ::blink::WebString;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AllOfArray;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Matcher;
using ::testing::Message;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointwise;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

struct AutofillFieldUtilCase {
  std::string_view description;
  std::string_view html;
  // The expected value with kAutofillBetterLocalHeuristicPlaceholderSupport
  // disabled
  std::u16string_view expected_label;
  // The expected value with the above feature enabled
  std::u16string_view better_placeholder_support_expected_label;
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
  <input id=target class=fixed_position_and_size value="value"/>
  <span class=fixed_position_and_size>poor man's placeholder</span>
)";

// The <input> element partially overlaps the label (placeholder) but the label
// is not fully contained in the <input> element. This is a common case for
// placeholders that morph into a minified version when the user focuses an
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
  <input id=target class=fixed_position_and_size value="value"/>
  <span class=overlapping_position_and_size>placeholder</span>
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

const char* kPlaceholderAndPoorMansPlaceholder =
    R"(
    <style>
      .fixed_position_and_size {
        position: fixed;
        top: 0;
        left: 0;
        width: 100px;
        height: 20px;
      }
    </style>
    <input id=target class=fixed_position_and_size placeholder="placeholder"/>
    <span class=fixed_position_and_size>poor man's placeholder</span>
  )";

const char* kInvalidPlaceholderAndPoorMansPlaceholder =
    R"(
    <style>
      .fixed_position_and_size {
        position: fixed;
        top: 0;
        left: 0;
        width: 100px;
        height: 20px;
      }
    </style>
    <input id=target class=fixed_position_and_size placeholder="+- "/>
    <span class=fixed_position_and_size>poor man's placeholder</span>
)";

auto HasRendererIdOf(const WebFormElement& e) {
  return Property("FormData::renderer_id()", &FormData::renderer_id,
                  GetFormRendererId(e));
}

auto HasRendererIdOf(const WebFormControlElement& e) {
  return Property("FormFieldData::renderer_id()", &FormFieldData::renderer_id,
                  GetFieldRendererId(e));
}

auto FormControlTypesAre(auto&&... form_control_types) {
  return ElementsAre(Property("form_control_type",
                              &FormFieldData::form_control_type,
                              form_control_types)...);
}

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
        {features::kAutofillIgnoreCheckableElements},
        /*disabled_features=*/{});
  }
  ~FormAutofillUtilsTest() override = default;

  WebDocument GetDocument() { return GetMainFrame()->GetDocument(); }

  std::optional<FormData> ExtractFormData(WebFormElement form) {
    return form_util::ExtractFormData(GetDocument(), form, field_data_manager(),
                                      kCallTimerStateDummy,
                                      /*button_titles_cache=*/nullptr);
  }

  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
  FindFormAndFieldForFormControlElement(WebFormControlElement control) {
    return form_util::FindFormAndFieldForFormControlElement(
        control, field_data_manager(), kCallTimerStateDummy,
        /*button_titles_cache=*/nullptr,
        /*form_cache=*/{});
  }

  FieldDataManager& field_data_manager() { return *field_data_manager_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<FieldDataManager> field_data_manager_ =
      base::MakeRefCounted<FieldDataManager>();
};

// Tests that some form control types are extracted by ExtractFormData() and
// others are not.
TEST_F(FormAutofillUtilsTest, ExtractFormData_FormControlTypes) {
  LoadHTML(R"(
    <form id=form-id>
      <!-- These form controls are not extracted. -->
      <div contenteditable></div>
      <button type=kButtonButton>Foo</button>
      <button type=kButtonSubmit>Foo</button>
      <button type=kButtonReset>Foo</button>
      <button type=kButtonPopover>Foo</button>
      <fieldset></fieldset>
      <input type=button>
      <input type=checkbox>
      <input type=color>
      <input type=datetime-local>
      <input type=file>
      <input type=hidden>
      <input type=image>
      <input type=radio>
      <input type=range>
      <input type=reset>
      <input type=submit>
      <input type=time>
      <input type=week>
      <output>Foo</output>
      <select multiple><option>Foo</option><option>Bar</option></select>

      <!-- These form controls are extracted. -->
      <input>
      <input type=email>
      <input type=month>
      <input type=number>
      <input type=password>
      <input type=search>
      <input type=tel>
      <input type=text>
      <input type=url>
      <input type=date>
      <select><option>Foo</option><option>Bar</option></select>
      <textarea>Foo</textarea>
    </form>
  )");
  FormData form_data =
      *ExtractFormData(GetFormElementById(GetDocument(), "form-id"));
  using enum FormControlType;
  EXPECT_THAT(form_data.fields(),
              FormControlTypesAre(kInputText, kInputEmail, kInputMonth,
                                  kInputNumber, kInputPassword, kInputSearch,
                                  kInputTelephone, kInputText, kInputUrl,
                                  kInputDate, kSelectOne, kTextArea));
}

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
                              huge_option, huge_option));

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
    std::vector<WebElement> web_to_skip =
        GetDocument().QuerySelectorAll("div[class='skip']");
    std::set<WebNode> to_skip;
    for (const WebElement& element : web_to_skip) {
      to_skip.insert(element);
    }

    EXPECT_EQ(test_case.expected_label,
              FindChildTextWithIgnoreListForTesting(target, to_skip));
  }
}

// TODO(crbug.com/430258039) Simplify the parametrized fixture and adapt test
// cases once the feature is launched.
template <typename TestCaseStruct>
class FormAutofillUtilsParameterizedTest
    : public FormAutofillUtilsTest,
      public testing::WithParamInterface<std::tuple<bool, TestCaseStruct>> {
 public:
  FormAutofillUtilsParameterizedTest() {
    feature_override_.InitWithFeatureState(
        features::kAutofillBetterLocalHeuristicPlaceholderSupport,
        BetterPlaceholderSupportEnabled());
  }

 protected:
  bool BetterPlaceholderSupportEnabled() {
    return std::get<0>(this->GetParam());
  }

  TestCaseStruct TestCase() { return std::get<1>(this->GetParam()); }

 private:
  base::test::ScopedFeatureList feature_override_;
};

using InferLabelForElementParameterizedTest =
    FormAutofillUtilsParameterizedTest<AutofillFieldUtilCase>;

const AutofillFieldUtilCase kInferLabelForElementTestCases[] = {
    {"DIV_table_test_1", R"(
       <div>
         <div>label</div><div><input id=target></div>
       </div>)",
     u"label", u"label"},
    {"DIV_table_test_2", R"(
       <div>
         <div>label</div>
         <div>should be skipped<input></div>
         <div><input id=target></div>
       </div>)",
     u"label", u"label"},
    {"DIV_table_test_3", R"(
       <div>
         <div>should be skipped<input></div>
         <div>label</div>
         <div><input id=target></div>
       </div>)",
     u"label", u"label"},
    {"DIV_table_test_4", R"(
       <div>
         <div>should be skipped<input></div>
         label
         <div><input id=target></div>
       </div>)",
     u"label", u"label"},
    {"DIV_table_test_5",
     "<div>"
     "<div>label<div><input id='target'/></div>behind</div>"
     "</div>",
     u"label", u"label"},
    {"DIV_table_test_6", R"(
       <div>
         label
         <div>*</div>
         <div><input id='target'></div>
       </div>)",
     u"label", u"label"},
    {"Infer_from_next_sibling",
     "<input id='target' type='checkbox'>hello <b>world</b>", u"hello world",
     u"hello world"},
    // With better placeholder support, poor man's placeholder will no longer
    // be considered a label. The label will be instead based on the value
    // attribute that is available.
    {"Poor_mans_placeholder", kPoorMansPlaceholderFullOverlap,
     u"poor man's placeholder", u"value"},
    // Same as above, label will be based on value attribute.
    {"Poor_mans_placeholder_partial_overlap",
     kPoorMansPlaceholderPartialOverlap, u"placeholder", u"value"},
    {"Poor_mans_placeholder_no_overlap", kPoorMansPlaceholderNoOverlap, u"",
     u""},
    {"Poor_mans_placeholder_no_overlap_2", kPoorMansPlaceholderNoOverlap2, u"",
     u""},
    {"Poor_mans_placeholder_possibly_an_error_message",
     kPoorMansPlaceholderPossiblyErrorMessage, u"", u""},
    {"Poor_mans_placeholder_no_horizontal_containment",
     kPoorMansPlaceholderNoHorizontalContainment, u"", u""}};

TEST_P(InferLabelForElementParameterizedTest, InferLabelForElementTest) {
  const AutofillFieldUtilCase& test_case = TestCase();

  std::u16string_view expected_label;
  if (BetterPlaceholderSupportEnabled()) {
    expected_label = test_case.better_placeholder_support_expected_label;
  } else {
    expected_label = test_case.expected_label;
  }

  LoadHTML(test_case.html);
  WebFormControlElement form_target =
      GetFormControlElementById(GetDocument(), "target");
  std::vector<FormFieldData> fields(1);
  InferLabelForElementsForTesting(
      std::to_array<WebFormControlElement>({form_target}), fields);
  EXPECT_EQ(fields.front().label(), expected_label);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    InferLabelForElementParameterizedTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(kInferLabelForElementTestCases)),
    [](const testing::TestParamInfo<std::tuple<bool, AutofillFieldUtilCase>>&
           info) {
      const bool feature_enabled = std::get<0>(info.param);
      const AutofillFieldUtilCase& test_case = std::get<1>(info.param);
      return base::StrCat({test_case.description, feature_enabled
                                                      ? "_FeatureEnabled"
                                                      : "_FeatureDisabled"});
    });

struct AutofillFieldLabelSourceCase {
  std::string_view description;
  std::string_view html;
  const FormFieldData::LabelSource label_source;
  const FormFieldData::LabelSource better_placeholder_support_source;
};

using InferLabelSourceParameterizedTest =
    FormAutofillUtilsParameterizedTest<AutofillFieldLabelSourceCase>;

const AutofillFieldLabelSourceCase kInferLabelSourceTestCases[] = {
    {"DivTable", "<div><div>label</div><div><input id='target'/></div></div>",
     FormFieldData::LabelSource::kDivTable,
     FormFieldData::LabelSource::kDivTable},
    {"LabelTag", "<label>label</label><input id='target'/>",
     FormFieldData::LabelSource::kLabelTag,
     FormFieldData::LabelSource::kLabelTag},
    {"Combined", "<b>l</b><strong>a</strong>bel<input id='target'/>",
     FormFieldData::LabelSource::kCombined,
     FormFieldData::LabelSource::kCombined},
    {"PTag", "<p><b>l</b><strong>a</strong>bel</p><input id='target'/>",
     FormFieldData::LabelSource::kPTag, FormFieldData::LabelSource::kPTag},
    {"PlaceholderAndAriaLabel",
     "<input id='target' placeholder='label' aria-label='label'/>",
     FormFieldData::LabelSource::kPlaceHolder,
     FormFieldData::LabelSource::kAriaLabel},
    {"AriaLabel", "<input id='target' aria-label='label'/>",
     FormFieldData::LabelSource::kAriaLabel,
     FormFieldData::LabelSource::kAriaLabel},
    {"Value", "<input id='target' value='label'/>",
     FormFieldData::LabelSource::kValue, FormFieldData::LabelSource::kValue},
    // In the next test, the text node is picked up on the way up the DOM-tree
    // by the div extraction logic.
    {"LiTagWithDivTable", "<li>label<div><input id='target'/></div></li>",
     FormFieldData::LabelSource::kDivTable,
     FormFieldData::LabelSource::kDivTable},
    {"LiTag", "<li><span>label</span><div><input id='target'/></div></li>",
     FormFieldData::LabelSource::kLiTag, FormFieldData::LabelSource::kLiTag},
    {"TdTag",
     "<table><tr><td>label</td><td><input id='target'/></td></tr></table>",
     FormFieldData::LabelSource::kTdTag, FormFieldData::LabelSource::kTdTag},
    {"DdTag", "<dl><dt>label</dt><dd><input id='target'></dd></dl>",
     FormFieldData::LabelSource::kDdTag, FormFieldData::LabelSource::kDdTag},
    {"OverlayingLabel", kPoorMansPlaceholderFullOverlap,
     FormFieldData::LabelSource::kOverlayingLabel,
     FormFieldData::LabelSource::kValue}};

TEST_P(InferLabelSourceParameterizedTest, InferLabelSourceTest) {
  const AutofillFieldLabelSourceCase& test_case = TestCase();

  FormFieldData::LabelSource expected_label_source;
  if (BetterPlaceholderSupportEnabled()) {
    expected_label_source = test_case.better_placeholder_support_source;
  } else {
    expected_label_source = test_case.label_source;
  }

  LoadHTML(test_case.html);
  WebFormControlElement form_target =
      GetFormControlElementById(GetDocument(), "target");
  std::vector<FormFieldData> fields(1);
  InferLabelForElementsForTesting(
      std::to_array<WebFormControlElement>({form_target}), fields);
  EXPECT_EQ(fields.front().label_source(), expected_label_source);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    InferLabelSourceParameterizedTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(kInferLabelSourceTestCases)),
    [](const testing::TestParamInfo<
        std::tuple<bool, AutofillFieldLabelSourceCase>>& info) {
      const bool feature_enabled = std::get<0>(info.param);
      const AutofillFieldLabelSourceCase& test_case = std::get<1>(info.param);
      return base::StrCat({test_case.description, feature_enabled
                                                      ? "_FeatureEnabled"
                                                      : "_FeatureDisabled"});
    });

struct AutofillFieldPlaceholderCase {
  std::string_view description;
  std::string_view html;
  std::u16string_view placeholder_attribute_value;
  std::u16string_view better_placeholder_support_expected_placeholder;
};

using InferPlaceholderParameterizedTest =
    FormAutofillUtilsParameterizedTest<AutofillFieldPlaceholderCase>;

static const AutofillFieldPlaceholderCase kInferPlaceholderTestCases[] = {
    {"Placeholder_present", kPlaceholderAndPoorMansPlaceholder, u"placeholder",
     u"placeholder"},
    {"Invalid_placeholder", kInvalidPlaceholderAndPoorMansPlaceholder, u"+- ",
     u"poor man's placeholder"},
    {"Placeholder_missing", kPoorMansPlaceholderFullOverlap, u"",
     u"poor man's placeholder"},
};

// Tests that the `placeholder` is inferred correctly based on
// the enablement of `AutofillBetterLocalHeuristicPlaceholderSupport`.
// `placeholder_attribute` is expected to keep the value of HTML
// attribute without modification.
TEST_P(InferPlaceholderParameterizedTest, InferPlaceholderForElementTest) {
  const AutofillFieldPlaceholderCase& test_case = TestCase();

  std::u16string_view expected_placeholder;
  if (BetterPlaceholderSupportEnabled()) {
    expected_placeholder =
        test_case.better_placeholder_support_expected_placeholder;
  } else {
    expected_placeholder = test_case.placeholder_attribute_value;
  }

  LoadHTML(test_case.html);
  WebFormControlElement form_target =
      GetFormControlElementById(GetDocument(), "target");

  FormFieldData field;
  WebFormControlElementToFormFieldForTesting(blink::WebFormElement(),
                                             form_target, nullptr, &field);

  EXPECT_EQ(field.placeholder(), expected_placeholder);
  EXPECT_EQ(field.placeholder_attribute(),
            test_case.placeholder_attribute_value);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    InferPlaceholderParameterizedTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(kInferPlaceholderTestCases)),
    [](const testing::TestParamInfo<
        std::tuple<bool, AutofillFieldPlaceholderCase>>& info) {
      const bool feature_enabled = std::get<0>(info.param);
      const AutofillFieldPlaceholderCase& test_case = std::get<1>(info.param);
      return base::StrCat({test_case.description, feature_enabled
                                                      ? "_FeatureEnabled"
                                                      : "_FeatureDisabled"});
    });

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

  ButtonTitleList actual = GetButtonTitles(form_target, &cache);

  ButtonTitleList expected = {
      {u"Sign Up", ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE}};
  EXPECT_EQ(expected, actual);

  VerifyButtonTitleCache(form_target, expected, cache);
}

TEST_F(FormAutofillUtilsTest, GetButtonTitles_TooLongTitle) {
  std::string kFormHtml = "<form id='target'>";
  for (int i = 0; i < 10; ++i) {
    std::string kFieldHtml = "<input type='button' value='" +
                             base::NumberToString(i) + std::string(300, 'a') +
                             "'>";
    kFormHtml += kFieldHtml;
  }
  kFormHtml += "</form>";

  LoadHTML(kFormHtml);
  WebFormElement form_target = GetFormElementById(GetDocument(), "target");
  ButtonTitlesCache cache;

  ButtonTitleList actual = GetButtonTitles(form_target, &cache);

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

  ButtonTitleList expected = {
      {u"Sign Up", ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE}};
  ButtonTitleList actual =
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
  std::vector<WebFormElement> forms = GetDocument().Forms();

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
TEST_F(FormAutofillUtilsTest, GetAriaDescriptionBySingle) {
  LoadHTML(
      "<input id='input' type='text' aria-describedby='div1'/>"
      "<div id='div1'>aria description</div>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaDescriptionForTesting(doc, element), u"aria description");
}

// Tests that aria-describedby works: Complex case: multiple ids referenced.
TEST_F(FormAutofillUtilsTest, GetAriaDescriptionByMulti) {
  LoadHTML(
      "<input id='input' type='text' aria-describedby='div1 div2'/>"
      "<div id='div2'>description</div>"
      "<div id='div1'>aria</div>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaDescriptionForTesting(doc, element), u"aria description");
}

// Tests that invalid aria-describedby returns the empty string.
TEST_F(FormAutofillUtilsTest, GetAriaDescriptionByInvalid) {
  LoadHTML("<input id='input' type='text' aria-describedby='invalid'/>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaDescriptionForTesting(doc, element), u"");
}

// Tests that aria-describedby is prioritized over aria-description.
TEST_F(FormAutofillUtilsTest, GetAriaDescriptionPrioritization) {
  LoadHTML(
      "<input id='input' type='text' aria-describedby='div1'"
      "       aria-description='aria description'/>"
      "<div id='div1'>aria describedby</div>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaDescriptionForTesting(doc, element), u"aria describedby");
}

// Tests that aria-description is used as a fallback if aria-describedby is
// unspecified.
TEST_F(FormAutofillUtilsTest, GetAriaDescriptionFallback) {
  LoadHTML(
      "<input id='input' type='text' aria-description='aria description'/>");

  WebDocument doc = GetDocument();
  auto element = GetFormControlElementById(doc, "input");
  EXPECT_EQ(GetAriaDescriptionForTesting(doc, element), u"aria description");
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
      form_and_field = FindFormAndFieldForFormControlElement(web_control);

  ASSERT_TRUE(form_and_field);
  auto& [form, field] = *form_and_field;
  EXPECT_FALSE(form.fields().back().bounds().IsEmpty());
}

TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_ExtractUnownedBounds) {
  LoadHTML("<body><input id='i1'></body>");
  WebDocument doc = GetDocument();
  auto web_control = GetFormControlElementById(doc, "i1");
  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
      form_and_field = FindFormAndFieldForFormControlElement(web_control);

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
      form_and_field = FindFormAndFieldForFormControlElement(web_control);

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
       FindFormAndFieldForFormControlElement_Disconnected) {
  LoadHTML(R"(<input name=count id=t>)");
  WebDocument doc = GetDocument();
  auto form_control = GetElementById(doc, "t").To<WebInputElement>();
  ExecuteJavaScriptForTests(R"(document.getElementById('t').remove();)");
  EXPECT_EQ(FindFormAndFieldForFormControlElement(form_control), std::nullopt);
}

// Tests that Autofill form ownership follows Blink form's association, which,
// in compliance with the HTML standard, associates forms with an unclosed
// <form> element.
// Regression test for crbug.com/347059988#comment40.
TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_DramaticallyBadMarkup) {
  auto is_ancestor = [](const WebElement& ancestor, WebNode descendant) {
    do {
      if (ancestor == descendant) {
        return true;
      }
    } while ((descendant = descendant.ParentNode()));
    return false;
  };

  // The following markup is intentionally bad!
  LoadHTML(R"(
    <!DOCTYPE html>
    <div>
      <form id=f1>
        <div>
          </form>
          <form id=f2>
        </div>
    </div>
    <input id=t>
  )");
  // This leads to the same DOM as
  //   <div>
  //     <form id=f1>
  //       <div>
  //         <form id=f2>
  //         </form>
  //       </div>
  //     </form>
  //   </div>
  //   <input id=t>
  // but it associates `t` with `f2`.

  WebDocument doc = GetDocument();
  auto f1 = GetElementById(doc, "f1").To<WebFormElement>();
  auto f2 = GetElementById(doc, "f2").To<WebFormElement>();
  auto t = GetElementById(doc, "t").To<WebInputElement>();

  ASSERT_TRUE(is_ancestor(f1, f2));
  ASSERT_FALSE(is_ancestor(f1, t));
  ASSERT_EQ(t.Form(), f2);  // nocheck

  EXPECT_THAT(FindFormAndFieldForFormControlElement(t),
              Optional(Pair(AllOf(HasRendererIdOf(f1),
                                  Property(&FormData::fields,
                                           ElementsAre(HasRendererIdOf(t)))),
                            _)));
}

// Tests the fallback mechanism where FindFormAndFieldForFormControlElement()
// constructs a form with a single field if it is unable to extract the form
// containing a control element.
TEST_F(FormAutofillUtilsTest,
       FindFormAndFieldForFormControlElement_FallbackOnFailure) {
  auto AddElementToForm = [this](const char* element) {
    std::string js = base::StringPrintf(
        "document.forms[0].appendChild(document.createElement('%s'))", element);
    ExecuteJavaScriptForTests(js);
  };

  // Create a form with too many fields so that extraction fails.
  LoadHTML(R"(<html><body><form id='f'><input id='i0'/></form>)");
  for (size_t i = 0; i < kMaxExtractableFields; ++i) {
    AddElementToForm("input");
  }

  WebDocument doc = GetDocument();
  WebFormElement form_element = GetFormElementById(doc, "f");
  WebFormControlElement control_element = GetFormControlElementById(doc, "i0");

  std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
      form_and_field = form_util::FindFormAndFieldForFormControlElement(
          control_element, field_data_manager(), kCallTimerStateDummy,
          /*button_titles_cache=*/nullptr,
          /*form_cache=*/{});

  ASSERT_TRUE(form_and_field);
  const FormData& fallback_form = form_and_field->first;

  // The fallback form should represent the owning form, so its `FormRendererId`
  // should match `form_element`'s renderer ID, and the form's only field's
  // `FieldRendererId` should match `control_element`'s renderer ID.
  EXPECT_EQ(fallback_form.renderer_id(),
            form_util::GetFormRendererId(form_element));
  EXPECT_NE(fallback_form.renderer_id(), FormRendererId());
  EXPECT_EQ(fallback_form.fields().size(), 1u);
  EXPECT_EQ(fallback_form.fields()[0].renderer_id(),
            form_util::GetFieldRendererId(control_element));
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
  LoadHTML(test_case.html);

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
    EXPECT_FALSE(std::visit(is_empty, form_data->child_frames()[i].token));
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
          html = base::StringPrintf("<form id='%s'>%s</form>", form_id, html);
          const char* other_forms =
              "<input><iframe></iframe> <form><input><iframe></iframe></form>";
          html = base::StrCat({other_forms, html, other_forms});
        } else {
          const char* other_form = "<form><input><iframe></iframe></form>";
          html = base::StrCat({other_form, html, other_form});
        }
        html = base::StringPrintf("<body>%s</body>", html);

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
  auto AddElementToForm = [this](const char* element) {
    std::string js = base::StringPrintf(
        "document.forms[0].appendChild(document.createElement('%s'))", element);
    ExecuteJavaScriptForTests(js);
  };

  LoadHTML(R"(<html><body><form id='f'></form>)");
  for (size_t i = 0; i < kMaxExtractableFields; ++i) {
    AddElementToForm("input");
  }
  for (size_t i = 0; i < kMaxExtractableChildFrames; ++i) {
    AddElementToForm("iframe");
  }

  // Ensure that Android runs at default page scale.
  web_view_->SetPageScaleFactor(1.0);

  WebDocument doc = GetDocument();
  WebFormElement form_element = GetFormElementById(doc, "f");
  FormData form_data = *ExtractFormData(form_element);
  EXPECT_EQ(form_data.fields().size(), kMaxExtractableFields);
  EXPECT_EQ(form_data.child_frames().size(), kMaxExtractableChildFrames);

  // Upon adding one more frame, this exceeds the limit and therefore we start
  // returning a form with no iframes.
  AddElementToForm("iframe");
  form_data = *ExtractFormData(form_element);
  EXPECT_EQ(form_data.fields().size(), kMaxExtractableFields);
  EXPECT_TRUE(form_data.child_frames().empty());
}

// Tests that if the number of fields exceeds |kMaxExtractableFields|, neither
// fields nor child frames of that form are extracted.
TEST_F(FormAutofillUtilsTest, ExtractNoFieldsOrFramesIfTooManyFields) {
  auto AddElementToForm = [this](const char* element) {
    std::string js = base::StringPrintf(
        "document.forms[0].appendChild(document.createElement('%s'))", element);
    ExecuteJavaScriptForTests(js);
  };

  LoadHTML(R"(<html><body><form id='f'></form>)");
  for (size_t i = 0; i < kMaxExtractableFields; ++i) {
    AddElementToForm("input");
  }
  for (size_t i = 0; i < kMaxExtractableChildFrames; ++i) {
    AddElementToForm("iframe");
  }

  // Ensure that Android runs at default page scale.
  web_view_->SetPageScaleFactor(1.0);

  WebDocument doc = GetDocument();
  WebFormElement form_element = GetFormElementById(doc, "f");
  FormData form_data = *ExtractFormData(form_element);
  EXPECT_EQ(form_data.fields().size(), kMaxExtractableFields);
  EXPECT_EQ(form_data.child_frames().size(), kMaxExtractableChildFrames);

  // Upon adding one more field, this exceeds the limit and therefore we start
  // returning a null form.
  AddElementToForm("input");
  ASSERT_FALSE(ExtractFormData(form_element));
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
  std::ranges::reverse(expected_elements);

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
                      Property(&FormFieldData::name, u"number_input"),
                      Property(&FormFieldData::name, u"select_input")))));
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
                      Property(&FormFieldData::name, u"number_input"),
                      Property(&FormFieldData::name, u"select_input")))));
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

// Tests that final-checkout-amount extraction extracts the
// final-checkout-amount if the label node is in the subtree that is only one
// ancestor up.
TEST_F(FormAutofillUtilsTest, ExtractFinalCheckoutAmountFromDom_OneAncestorUp) {
  std::vector<std::string> matches;
  LoadHTML(R"(
    <body>
      <div>
        <span>Total</span>
        <div>$448.60</div>
      </div>
    </body>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^.448.60$";
  std::string_view label_regex = "^Total$";

  EXPECT_EQ(ExtractFinalCheckoutAmountFromDom(
                document, price_regex, label_regex,
                /*number_of_ancestor_levels_to_search=*/6),
            "$448.60");
}

// Tests that final-checkout-amount extraction extracts the
// final-checkout-amount if the label node is in the subtree that is many
// ancestors up.
TEST_F(FormAutofillUtilsTest,
       ExtractFinalCheckoutAmountFromDom_ManyAncestorsUp) {
  LoadHTML(R"(
  <div>
    <div>
      <div>Total</div>
      <div>
        <div>
          <span>
            <span>
              <span>$56.70</span>
            </span>
          </span>
        </div>
      </div>
    </div>
  </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^.56.70$";
  std::string_view label_regex = "^Total$";

  EXPECT_EQ(ExtractFinalCheckoutAmountFromDom(
                document, price_regex, label_regex,
                /*number_of_ancestor_levels_to_search=*/6),
            "$56.70");
}

// Tests that final-checkout-amount extraction extracts the
// final-checkout-amount if the label node is in the subtree that is many
// ancestors down.
TEST_F(FormAutofillUtilsTest,
       ExtractFinalCheckoutAmountFromDom_ManyAncestorsDown) {
  LoadHTML(R"(
  <div>
    <div>
      <div>$56.70</div>
      <div>
        <div>
          <span>
            <span>
              <span>
                <div>Total</div>
              </span>
            </span>
          </span>
        </div>
      </div>
    </div>
  </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^.56.70$";
  std::string_view label_regex = "^Total$";

  EXPECT_EQ(ExtractFinalCheckoutAmountFromDom(
                document, price_regex, label_regex,
                /*number_of_ancestor_levels_to_search=*/2),
            "$56.70");
}

// Tests that final-checkout-amount extraction does not extract a
// final-checkout-amount if the label node is more than
// `number_of_ancestor_levels_to_search` up from the final-checkout-amount node.
TEST_F(FormAutofillUtilsTest,
       ExtractFinalCheckoutAmountFromDom_TooManyAncestorsUp_DoesNotMatch) {
  LoadHTML(R"(
  <div>
    <div>
      <div>Total</div>
      <div>
        <div>
          <span>
            <span>
              <span>$56.70</span>
            </span>
          </span>
        </div>
      </div>
    </div>
  </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^.56.70$";
  std::string_view label_regex = "^Total$";

  EXPECT_TRUE(ExtractFinalCheckoutAmountFromDom(
                  document, price_regex, label_regex,
                  /*number_of_ancestor_levels_to_search=*/3)
                  .empty());
}

// Tests that final-checkout-amount extraction returns the first
// final-checkout-amount match if there are multiple possible matches.
TEST_F(FormAutofillUtilsTest,
       ExtractFinalCheckoutAmountFromDom_MultiplePriceNodes_MatchesFirstOne) {
  LoadHTML(R"(
  <div>
    <div>
      <div>
        <div>
          <span>
            <span>
              <span>$56.70</span>
              <span>Total</span>
            </span>
            <span>
              <span>$56.71</span>
              <span>Total</span>
            </span>
          </span>
        </div>
      </div>
    </div>
  </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^(.56.70|.56.71)$";
  std::string_view label_regex = "^Total$";

  std::string final_checkout_amount = ExtractFinalCheckoutAmountFromDom(
      document, price_regex, label_regex,
      /*number_of_ancestor_levels_to_search=*/6);
  EXPECT_TRUE(final_checkout_amount == "$56.70" ||
              final_checkout_amount == "$56.71");
}

// Tests that final-checkout-amount extraction returns the closest final
// checkout amount match if there are multiple possible matches. The closest
// match is based on the lowest common ancestor between price node and label
// node.
TEST_F(FormAutofillUtilsTest,
       ExtractFinalCheckoutAmountFromDom_MultiplePriceNodes_MatchesClosestOne) {
  LoadHTML(R"(
  <div>
    <div>
      <div>
        <div>
          <span>
            <span>
              <span>
                <span>$56.70</span>
              </span>
              <span>Total</span>
            </span>
            <span>
              <span>$56.71</span>
              <span>Total</span>
            </span>
          </span>
        </div>
      </div>
    </div>
  </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^(.56.70|.56.71)$";
  std::string_view label_regex = "^Total$";

  EXPECT_EQ(ExtractFinalCheckoutAmountFromDom(
                document, price_regex, label_regex,
                /*number_of_ancestor_levels_to_search=*/6),
            "$56.71");
}

// Tests that final-checkout-amount extraction does not extract a
// final-checkout-amount if there are price nodes in the ancestor searches
// containing the label node.
TEST_F(FormAutofillUtilsTest,
       ExtractFinalCheckoutAmountFromDom_MultiplePriceNodes_DoesNotMatch) {
  LoadHTML(R"(
  <div>
    <div>
      <div>Total</div>
      <div>
        <div>
          <span>
            <span>
              <span>$56.70</span>
            </span>
            <span>
              <span>$56.71</span>
            </span>
          </span>
        </div>
      </div>
    </div>
  </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^(.56.70|.56.71)$";
  std::string_view label_regex = "^Total$";

  EXPECT_TRUE(ExtractFinalCheckoutAmountFromDom(
                  document, price_regex, label_regex,
                  /*number_of_ancestor_levels_to_search=*/6)
                  .empty());
}

// Tests that final-checkout-amount extraction does not extract a
// final-checkout-amount if the ancestor search of one price node contains
// multiple price nodes, and the ancestor search of the other one does not
// contain the label node.
TEST_F(
    FormAutofillUtilsTest,
    ExtractFinalCheckoutAmountFromDom_MultiplePriceNodesInAncestorSearchOfOne_DoesNotMatch) {
  LoadHTML(R"(
  <div>
    <div>
      <span>$56.71</span>
      <span>
        <div>Total</div>
        <span>
          <div>
            <div>
              <span>$56.70</span>
            </div>
          </div>
        </span>
      </span>
    </div>
  </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^(.56.70|.56.71)$";
  std::string_view label_regex = "^Total$";

  EXPECT_TRUE(ExtractFinalCheckoutAmountFromDom(
                  document, price_regex, label_regex,
                  /*number_of_ancestor_levels_to_search=*/3)
                  .empty());
}

// Tests that final-checkout-amount extraction matches a final-checkout-amount
// if the ancestor search of one price node contains multiple price nodes, and
// the ancestor search of the other one contains the label node and only one
// price node.
TEST_F(
    FormAutofillUtilsTest,
    ExtractFinalCheckoutAmountFromDom_MultiplePriceNodesInAncestorSearchOfOne_OtherAncestorPathOnlyHasOne_Matches) {
  LoadHTML(R"(
  <div>
    <div>
      <span>$56.71</span>
      <div>
        <div>
          <span>
            <div>Total</div>
            <span>
              <span>$56.70</span>
            </span>
          </span>
        </div>
      </div>
    </div>
  </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^(.56.70|.56.71)$";
  std::string_view label_regex = "^Total$";

  EXPECT_EQ(ExtractFinalCheckoutAmountFromDom(
                document, price_regex, label_regex,
                /*number_of_ancestor_levels_to_search=*/6),
            "$56.70");
}

// Tests that the final-checkout-amount extraction does not extract a final
// checkout amount if there is no label node.
TEST_F(FormAutofillUtilsTest,
       ExtractFinalCheckoutAmountFromDom_NoLabelNode_DoesNotMatch) {
  LoadHTML(R"(
    <div>
      <div>Not a label</div>
      <span>$56.70</span>
    </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^.56.70$";
  std::string_view label_regex = "^Total$";

  EXPECT_TRUE(ExtractFinalCheckoutAmountFromDom(
                  document, price_regex, label_regex,
                  /*number_of_ancestor_levels_to_search=*/6)
                  .empty());
}

// Tests that final-checkout-amount extraction does not extract a
// final-checkout-amount if there are no price nodes.
TEST_F(FormAutofillUtilsTest,
       ExtractFinalCheckoutAmountFromDom_NoPriceNodes_DoesNotMatch) {
  LoadHTML(R"(
  <div>
      <div>Total</div>
      <div>Not a final-checkout-amount</div>
    </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^.56.70$";
  std::string_view label_regex = "^Total$";

  EXPECT_TRUE(ExtractFinalCheckoutAmountFromDom(
                  document, price_regex, label_regex,
                  /*number_of_ancestor_levels_to_search=*/6)
                  .empty());
}

// Tests that final-checkout-amount extraction does not extract a
// final-checkout-amount if there are no label nodes and no price nodes.
TEST_F(
    FormAutofillUtilsTest,
    ExtractFinalCheckoutAmountFromDom_NoPriceNodesAndNoLabelNodes_DoesNotMatch) {
  LoadHTML(R"(
    <div>
      <div>Not a total node</div>
      <div>Not a final-checkout-amount</div>
    </div>)");
  WebDocument document = GetDocument();
  std::string_view price_regex = "^.56.70$";
  std::string_view label_regex = "^Total$";

  EXPECT_TRUE(ExtractFinalCheckoutAmountFromDom(
                  document, price_regex, label_regex,
                  /*number_of_ancestor_levels_to_search=*/6)
                  .empty());
}

// Fixture for testing that forms can[not] be extracted on certain URLs.
class FormAutofillUtilsTest_AdmissibleUrls
    : public FormAutofillUtilsTest,
      public testing::WithParamInterface<std::pair<std::string_view, bool>> {
 public:
  std::string_view Url() const { return GetParam().first; }
  bool extractable() const { return GetParam().second; }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormAutofillUtilsTest_AdmissibleUrls,
    testing::Values(std::pair("https://foo.com", true),
                    std::pair("http://foo.com", true),
                    std::pair("about:srcdoc", true),
                    std::pair("data:text/html,blabla", true),
                    std::pair("about:blank", false),
                    std::pair("chrome:new-tab-page", false),
                    std::pair("chrome://autofill-internals", false)));

// Tests that <div contenteditable> can be extracted from admissible URLs.
TEST_P(FormAutofillUtilsTest_AdmissibleUrls, ExtractFormData) {
  LoadHTMLWithUrlOverride(R"(
    "<form id=f><input></form>"
  )",
                          Url());
  std::optional<FormData> form =
      ExtractFormData(GetFormElementById(GetDocument(), "f"));
  if (extractable()) {
    EXPECT_NE(form, std::nullopt);
  } else {
    EXPECT_EQ(form, std::nullopt);
  }
}

// Tests that <div contenteditable> can be extracted from admissible URLs.
TEST_P(FormAutofillUtilsTest_AdmissibleUrls, FindFormForContentEditable) {
  LoadHTMLWithUrlOverride(R"(
    "<div id=ce contenteditable></div>"
  )",
                          Url());
  std::optional<FormData> form =
      FindFormForContentEditable(GetDocument().GetElementById("ce"));
  if (extractable()) {
    EXPECT_NE(form, std::nullopt);
  } else {
    EXPECT_EQ(form, std::nullopt);
  }
}

// This constant defines a regular form in HTML with multiple fields of
// different types.
constexpr std::string_view kFormHtml =
    R"(<form id=TestForm name=TestForm action='http://abc.com'>
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
         <input type=date id='date'>
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
constexpr std::string_view kUnownedFormHtml =
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
       <input type=date id='date'>
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
constexpr std::string_view kUnownedUntitledFormHtml =
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
       <input type=date id='date'>
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
constexpr std::string_view kUnownedNonEnglishFormHtml =
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
         <input type=date id='date'>
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

constexpr std::string_view kNonAsciiHeaderHtml =
    R"(<head><title>accented latin: \xC3\xA0, )"
    R"(thai: \xE0\xB8\x81, control: \x04, )"
    R"(nbsp: \xEF\xBB\xBF, non-BMP: \xF0\x9F\x8C\x80; )"
    R"(This should match a CHECKOUT flow )"
    R"(despite the non-ASCII chars</title></head>)";

FormCache::UpdateFormCacheResult UpdateFormCache(FormCache& form_cache) {
  static constexpr CallTimerState kUpdateFormCacheCallTimerStateDummy = {
      .call_site = CallTimerState::CallSite::kExtractForms,
      .last_autofill_agent_reset = {},
      .last_dom_content_loaded = {},
  };

  return form_cache.UpdateFormCache(*base::MakeRefCounted<FieldDataManager>(),
                                    kUpdateFormCacheCallTimerStateDummy);
}

FormData FindForm(const blink::WebFormControlElement& element) {
  constexpr CallTimerState kExtractFormDataCallTimerStateDummy = {
      .call_site = CallTimerState::CallSite::kUpdateFormCache,
      .last_autofill_agent_reset = {},
      .last_dom_content_loaded = {},
  };

  if (auto p = form_util::FindFormAndFieldForFormControlElement(
          element, *base::MakeRefCounted<FieldDataManager>(),
          kExtractFormDataCallTimerStateDummy,
          /*button_titles_cache=*/nullptr,
          /*form_cache=*/{})) {
    return p->first;
  }
  return FormData();
}

void ApplyFieldsAction(
    const blink::WebDocument& document,
    base::span<const FormFieldData> fields,
    mojom::ActionPersistence action_persistence,
    mojom::FormActionType action_type = mojom::FormActionType::kFill) {
  form_util::ApplyFieldsAction(
      document,
      base::ToVector(fields,
                     [](const FormFieldData& field) {
                       return FormFieldData::FillData(field);
                     }),
      action_type, action_persistence,
      *base::MakeRefCounted<FieldDataManager>());
}

// Helper to retrieve a value from a map. Return default-constructed value if
// the key does not exist.
template <typename MapType, typename KeyType>
auto Get(const MapType& map, const KeyType& key) {
  return base::OptionalFromPtr(base::FindOrNull(map, key)).value_or({});
}

class UnownedFormToFormDataTest : public test::AutofillRendererTest {};

// Tests that the extraction of an unowned form fails if only an owned form
// exists.
TEST_F(UnownedFormToFormDataTest, UnownedFormElementsToFormDataWithForm) {
  LoadHTML(kFormHtml);
  EXPECT_FALSE(ExtractFormData(WebFormElement()));
}

// Tests that the extraction of an unowned form succeeds.
TEST_F(UnownedFormToFormDataTest, FormlessForms) {
  LoadHTML(kUnownedUntitledFormHtml);
  EXPECT_TRUE(ExtractFormData(WebFormElement()));
}

struct FormFillPreviewTestParam {
  std::string html;
  std::optional<std::string> url_override;
  bool unowned = false;
};

class FormFillAndPreviewTest
    : public test::AutofillRendererTest,
      public WithParamInterface<FormFillPreviewTestParam> {
 public:
  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    form_cache_.emplace(&autofill_agent());

#if BUILDFLAG(IS_WIN)
    // Autofill uses the system font to render suggestion previews. On
    // Windows an extra step is required to ensure that the system font is
    // configured.
    blink::WebFontRendering::SetMenuFontMetrics(WebString::FromAscii("Arial"),
                                                12);
#endif
  }

  void TearDown() override {
    form_cache_.reset();
    test::AutofillRendererTest::TearDown();
  }

 protected:
  // Contains the initial value for non-empty fields by their HTML id. Used in
  // combination with `kPreviewValuesById`/`kFillValuesById`.
  static constexpr auto kInitialValuesById =
      base::MakeFixedFlatMap<std::string_view, std::string_view>({
          {"notempty", "Hi"},
          {"month-nonempty", "2011-12"},
          {"select-nonempty", "CA"},
          {"select-unchanged", "CA"},
          {"select-displaynone", "CA"},
          {"textarea-nonempty", "Go\naway!"},
      });
  // Contains HTML ids of fields that should be autofilled. Used in combination
  // with `kPreviewValuesById`/`kFillValuesById`.
  static constexpr auto kActiveFieldIds = std::to_array<std::string_view>({
      "firstname",
      "lastname",
      "notempty",
      "noautocomplete",
      "month",
      "month-nonempty",
      "date",
      "select",
      "select-nonempty",
      "select-unchanged",
      "select-displaynone",
      "textarea",
      "textarea-nonempty",
  });

  FormCache::UpdateFormCacheResult UpdateFormCache() {
    return form_util::UpdateFormCache(*form_cache_);
  }

  template <size_t FieldsCount>
  AssertionResult ExecuteAutofillAction(
      std::string_view initial_focus_element_id,
      mojom::ActionPersistence action_persistence,
      const base::fixed_flat_map<std::string_view,
                                 std::string_view,
                                 FieldsCount>& suggestions_by_id) {
    std::vector<FormData> forms = UpdateFormCache().updated_forms;
    if (forms.size() != 1U) {
      return AssertionFailure() << "Found incorrect number of forms!\n"
                                << "Expected: 1, Actual: " << forms.size();
    }
    FormData& form = forms.front();

    // Setup the suggestions in the FormFields
    for (const auto& [id, suggestion] : suggestions_by_id) {
      FormFieldData* field =
          test_api(form).FindFieldByNameForTest(UTF8ToUTF16(id));
      if (!field) {
        return AssertionFailure()
               << "Unable to find field with ID '" << id << "'";
      }
      field->set_value(UTF8ToUTF16(suggestion));
      field->set_is_autofilled_according_to_renderer(true);
    }

    // Trigger the fill/preview action
    GetDocument()
        .GetElementById(WebString::FromUtf8(initial_focus_element_id))
        .Focus();
    ApplyFieldsAction(GetDocument(), form.fields(), action_persistence);

    return AssertionSuccess();
  }

 private:
  // We use a fresh `FormCache` in this fixture because the
  // `AutofillAgent`'s cache is used and populated by `AutofillAgent`.
  std::optional<FormCache> form_cache_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormFillAndPreviewTest,
    ValuesIn(std::to_array<FormFillPreviewTestParam>({
        // Tests filling a regular, owned form.
        {.html = std::string(kFormHtml), .unowned = false},
        // Tests filling an unowned form with a title tag in the HTML header.
        {.html = std::string(kUnownedFormHtml), .unowned = true},
        // Tests filling an unowned and untitled form. Use URL override to
        // indicate that the unowned fields are a form.
        {.html = std::string(kUnownedUntitledFormHtml),
         .url_override = "http://example.test/checkout_flow",
         .unowned = true},
        {.html = std::string(kUnownedUntitledFormHtml),
         .url_override = "http://example.test/Enter_Shipping_Address/",
         .unowned = true},
        // Tests filling an unowned form with a non-English language
        // configured.
        {.html = std::string(kUnownedNonEnglishFormHtml), .unowned = true},
        // Tests filling an unowned form with non-ASCII characters in the HTML.
        {.html = base::StrCat({kNonAsciiHeaderHtml, kUnownedUntitledFormHtml}),
         .unowned = true},
    })),
    [](const TestParamInfo<FormFillAndPreviewTest::ParamType>& info) {
      constexpr auto kNames = std::to_array<std::string_view>(
          {"OwnedForm", "UnownedForm", "UnownedUntitledFormCheckoutUrl",
           "UnownedUntitledFormAddressUrl", "UnownedNonEnglishForm",
           "NonAsciiForm"});
      return std::string{kNames[info.index]};
    });

// Tests filling different form configurations (owned, unowned, ...).
TEST_P(FormFillAndPreviewTest, FillForm) {
  constexpr auto kFillValuesById =
      base::MakeFixedFlatMap<std::string_view, std::string_view>({
          // Regular empty fields (firstname & lastname) should be autofilled.
          {"firstname", "filled firstname"},
          {"lastname", "filled lastname"},
          // Already filled fields should be previewed.
          {"notempty", "filled notempty"},
          {"noautocomplete", "filled noautocomplete"},
          // Disabled fields should not be autofilled.
          {"notenabled", "filled notenabled"},
          // Readonly fields should not be autofilled.
          {"readonly", "filled readonly"},
          // Fields with "visibility: hidden" should not be autofilled.
          {"invisible", "filled invisible"},
          // Fields with "display:none" should not be autofilled.
          {"displaynone", "filled displaynone"},
          // Regular <input type=month> should be autofilled.
          {"month", "2017-11"},
          {"month-nonempty", "2017-11"},
          // Regular <input type=date> should be be autofilled.
          {"date", "2017-11-12"},
          // Regular select fields should be autofilled.
          {"select", "TX"},
          // Select fields should be autofilled even if they already have a
          // non-empty value.
          {"select-nonempty", "TX"},
          {"select-unchanged", "CA"},
          // Select fields that are not focusable should be filled.
          {"select-displaynone", "TX"},
          // Regular textarea elements should be autofilled.
          {"textarea", "some multi-\nline value"},
          {"textarea-nonempty", "some multi-\nline value"},
      });

  if (GetParam().url_override) {
    LoadHTMLWithUrlOverride(GetParam().html, *GetParam().url_override);
  } else {
    LoadHTML(GetParam().html);
  }

  // Verify initial state of form.
  for (const auto& [id, fill_value] : kFillValuesById) {
    EXPECT_THAT(GetFormControlElementById(id),
                test::WebFormControlElementEq({
                    .value = std::string(Get(kInitialValuesById, id)),
                    .suggested_value = "",
                    .is_autofilled = false,
                    .is_previewed = false,
                }));
  }

  ASSERT_TRUE(ExecuteAutofillAction(
      "firstname", mojom::ActionPersistence::kFill, kFillValuesById));

  // Verify state of form after filling.
  for (const auto& [id, fill_value] : kFillValuesById) {
    bool is_active = std::ranges::contains(kActiveFieldIds, id);
    EXPECT_THAT(GetFormControlElementById(id),
                test::WebFormControlElementEq({
                    // Use `fill_value` for filled fields and the initial value
                    // to verify that non-filled fields were not modified.
                    .value = std::string(
                        is_active ? fill_value : Get(kInitialValuesById, id)),
                    .suggested_value = "",
                    .is_autofilled = is_active,
                    .is_previewed = false,
                }));
  }
  WebInputElement first_input = GetInputElementById("firstname");
  ASSERT_FALSE(first_input.IsNull());
  EXPECT_EQ(first_input.Value().length(), first_input.SelectionStart());
  EXPECT_EQ(first_input.Value().length(), first_input.SelectionEnd());
}

// Tests previewing different form configurations (owned, unowned, ...).
TEST_P(FormFillAndPreviewTest, PreviewForm) {
  constexpr auto kPreviewValuesById =
      base::MakeFixedFlatMap<std::string_view, std::string_view>({
          // Normal empty fields should be previewed.
          {"firstname", "suggested firstname"},
          {"lastname", "suggested lastname"},
          // Already filled fields should be previewed.
          {"notempty", "suggested notempty"},
          {"noautocomplete", "filled noautocomplete"},
          // Disabled fields should not be previewed.
          {"notenabled", "suggested notenabled"},
          // Readonly fields should not be previewed.
          {"readonly", "suggested readonly"},
          // Fields with "visibility: hidden" should not be previewed.
          {"invisible", "suggested invisible"},
          // Fields with "display:none" should not previewed.
          {"displaynone", "suggested displaynone"},
          // Regular <input type=month> should be previewed.
          {"month", "2017-11"},
          {"month-nonempty", "2017-11"},
          // Regular <input type=date> should be previewed.
          {"date", "2017-11-12"},
          // Regular select fields should be previewed.
          {"select", "TX"},
          // Select fields should be previewed even if they already have a
          // non-empty value.
          {"select-nonempty", "TX"},
          // Select fields should be previewed even if no suggestion is passed.
          {"select-unchanged", ""},
          // Select fields that are not focusable should always be filled.
          {"select-displaynone", "CA"},
          // Normal textarea elements should be previewed.
          {"textarea", "suggested multi-\nline value"},
          // Nonempty textarea elements should not be previewed.
          {"textarea-nonempty", "suggested multi-\nline value"},
      });

  if (GetParam().url_override) {
    LoadHTMLWithUrlOverride(GetParam().html, *GetParam().url_override);
  } else {
    LoadHTML(GetParam().html);
  }

  // Verify initial state of form.
  for (const auto& [id, preview_value] : kPreviewValuesById) {
    EXPECT_THAT(GetFormControlElementById(id),
                test::WebFormControlElementEq({
                    .value = std::string(Get(kInitialValuesById, id)),
                    .suggested_value = "",
                    .is_autofilled = false,
                    .is_previewed = false,
                }));
  }

  ASSERT_TRUE(ExecuteAutofillAction(
      "firstname", mojom::ActionPersistence::kPreview, kPreviewValuesById));

  // Verify state of form after previewing.
  for (const auto& [id, preview_value] : kPreviewValuesById) {
    bool is_active = std::ranges::contains(kActiveFieldIds, id);
    EXPECT_THAT(
        GetFormControlElementById(id),
        test::WebFormControlElementEq({
            // Ensure that no fields were actually modified.
            .value = std::string(Get(kInitialValuesById, id)),
            .suggested_value = std::string(is_active ? preview_value : ""),
            .is_autofilled = false,
            .is_previewed = is_active,
        }));
  }
  WebInputElement first_input = GetInputElementById("firstname");
  ASSERT_FALSE(first_input.IsNull());
  // Since the suggestion is previewed as a placeholder, there should be no
  // selected text.
  EXPECT_EQ(0u, first_input.SelectionStart());
  EXPECT_EQ(0u, first_input.SelectionEnd());
}

class FormClearPreviewTest : public FormFillAndPreviewTest {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormClearPreviewTest,
    ValuesIn(std::to_array<FormFillPreviewTestParam>({
        {.html = R"(<form name=TestForm action='http://abc.com'>
                      <input id=firstname value=Wyatt>
                      <input id=lastname>
                      <input id=email>
                      <input type=email id=email2>
                      <input type=tel id=phone>
                      <input type=submit value=Send>
                    </form>)",
         .unowned = false},
        {.html = R"(<head><title>store checkout</title></head>
                    <input id=firstname value=Wyatt>
                    <input id=lastname>
                    <input id=email>
                    <input type=email id=email2>
                    <input type=tel id=phone>
                    <input type=submit value=Send>)",
         .unowned = true},
    })),
    [](const TestParamInfo<FormFillAndPreviewTest::ParamType>& param_info) {
      return param_info.param.unowned ? "Unowned" : "Owned";
    });

// Tests that previewed elements are reverted to their original state after
// calling `form_util::ClearPreviewedElements`.
TEST_P(FormClearPreviewTest, ClearPreviewedElements) {
  LoadHTML(GetParam().html);

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
  firstname.SetSuggestedValue(WebString::FromAscii("Wyatt"));
  lastname.SetSuggestedValue(WebString::FromAscii("Earp"));
  elements[2].first.SetSuggestedValue(WebString::FromAscii("wyatt@earp.com"));
  elements[3].first.SetSuggestedValue(WebString::FromAscii("wyatt@earp.com"));
  elements[4].first.SetSuggestedValue(WebString::FromAscii("650-777-9999"));

  std::vector<bool> is_value_empty(elements.size());
  for (size_t i = 0; i < elements.size(); ++i) {
    is_value_empty[i] = elements[i].first.Value().IsEmpty();
  }

  // Clear the previewed fields.
  form_util::ClearPreviewedElements(elements);

  // Verify the previewed fields are cleared.
  for (size_t i = 0; i < elements.size(); ++i) {
    WebFormControlElement& element = elements[i].first;
    SCOPED_TRACE(Message() << "Element " << i);
    EXPECT_EQ(element.Value().IsEmpty(), is_value_empty[i]);
    EXPECT_TRUE(element.SuggestedValue().IsEmpty());
    EXPECT_FALSE(element.IsAutofilled());
  }
}

// Tests that previously non-empty but non-autofilled elements restore their
// original value after calling `form_util::ClearPreviewedElements`.
TEST_P(FormClearPreviewTest, ClearPreviewedFormWithNonEmptyInitiatingNode) {
  LoadHTML(GetParam().html);

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
  firstname.SetSuggestedValue(WebString::FromAscii("Wyatt X."));
  lastname.SetSuggestedValue(WebString::FromAscii("Earp"));
  elements[2].first.SetSuggestedValue(WebString::FromAscii("wyatt@earp.com"));
  elements[3].first.SetSuggestedValue(WebString::FromAscii("wyatt@earp.com"));
  elements[4].first.SetSuggestedValue(WebString::FromAscii("650-777-9999"));

  // Clear the previewed fields.
  form_util::ClearPreviewedElements(elements);

  // Fields with non-empty values are restored.
  EXPECT_EQ(u"Wyatt", firstname.Value().Utf16());
  EXPECT_TRUE(firstname.SuggestedValue().IsEmpty());
  EXPECT_FALSE(firstname.IsAutofilled());

  // Verify the previewed fields are cleared.
  for (size_t i = 1; i < elements.size(); ++i) {
    WebFormControlElement& element = elements[i].first;
    SCOPED_TRACE(Message() << "Element " << i);
    EXPECT_TRUE(element.Value().IsEmpty());
    EXPECT_TRUE(element.SuggestedValue().IsEmpty());
    EXPECT_FALSE(element.IsAutofilled());
  }
}

// Tests that previously autofilled elements restore their original autofilled
// value after calling `form_util::ClearPreviewedElements`.
TEST_P(FormClearPreviewTest, ClearPreviewedFormWithAutofilledInitiatingNode) {
  LoadHTML(GetParam().html);

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
  firstname.SetSuggestedValue(WebString::FromAscii("Wyatt X."));
  lastname.SetSuggestedValue(WebString::FromAscii("Earp"));
  elements[2].first.SetSuggestedValue(WebString::FromAscii("wyatt@earp.com"));
  elements[3].first.SetSuggestedValue(WebString::FromAscii("wyatt@earp.com"));
  elements[4].first.SetSuggestedValue(WebString::FromAscii("650-777-9999"));

  // Clear the previewed fields.
  form_util::ClearPreviewedElements(elements);

  // Fields with non-empty values are restored.
  EXPECT_EQ(u"Wyatt", firstname.Value().Utf16());
  EXPECT_TRUE(firstname.SuggestedValue().IsEmpty());
  EXPECT_TRUE(firstname.IsAutofilled());

  // Verify the previewed fields are cleared.
  for (size_t i = 1; i < elements.size(); ++i) {
    WebFormControlElement& element = elements[i].first;
    SCOPED_TRACE(Message() << "Element " << i);
    EXPECT_TRUE(element.Value().IsEmpty());
    EXPECT_TRUE(element.SuggestedValue().IsEmpty());
    EXPECT_FALSE(element.IsAutofilled());
  }
}

class FieldLabelInferenceTest : public test::AutofillRendererTest {
 protected:
  static std::vector<Matcher<FormFieldData>> JohnSmithIdFieldsMatchers() {
    return {test::FormFieldDescriptionEq({.label = u"First name:",
                                          .name = u"firstname",
                                          .name_attribute = u"",
                                          .id_attribute = u"firstname",
                                          .value = u"John"}),
            test::FormFieldDescriptionEq({.label = u"Last name:",
                                          .name = u"lastname",
                                          .name_attribute = u"",
                                          .id_attribute = u"lastname",
                                          .value = u"Smith"}),
            test::FormFieldDescriptionEq({.label = u"Email:",
                                          .name = u"email",
                                          .name_attribute = u"",
                                          .id_attribute = u"email",
                                          .value = u"john@example.com"})};
  }

  static std::vector<Matcher<FormFieldData>> JohnSmithNameFieldsMatchers() {
    return {test::FormFieldDescriptionEq({.label = u"First name:",
                                          .name = u"firstname",
                                          .name_attribute = u"firstname",
                                          .value = u"John"}),
            test::FormFieldDescriptionEq({.label = u"Last name:",
                                          .name = u"lastname",
                                          .name_attribute = u"lastname",
                                          .value = u"Smith"}),
            test::FormFieldDescriptionEq({.label = u"Email:",
                                          .name = u"email",
                                          .name_attribute = u"email",
                                          .value = u"john@example.com"})};
  }

  static std::vector<Matcher<FormFieldData>>
  StarredFirstLastEmailFieldsMatchers() {
    return {test::FormFieldDescriptionEq({.label = u"*First Name",
                                          .name = u"firstname",
                                          .name_attribute = u"",
                                          .id_attribute = u"firstname",
                                          .value = u"John"}),
            test::FormFieldDescriptionEq({.label = u"*Last Name",
                                          .name = u"lastname",
                                          .name_attribute = u"",
                                          .id_attribute = u"lastname",
                                          .value = u"Smith"}),
            test::FormFieldDescriptionEq({.label = u"*Email",
                                          .name = u"email",
                                          .name_attribute = u"",
                                          .id_attribute = u"email",
                                          .value = u"john@example.com"})};
  }

  static std::vector<Matcher<FormFieldData>>
  AriaLabelAndDescriptionFieldsMatchers() {
    return {test::FormFieldDescriptionEq(
                {.aria_label = u"inline aria label", .aria_description = u""}),
            test::FormFieldDescriptionEq(
                {.aria_label = u"aria label", .aria_description = u""}),
            test::FormFieldDescriptionEq(
                {.aria_label = u"", .aria_description = u"aria description"})};
  }
};

TEST_F(FieldLabelInferenceTest, Labels) {
  LoadHTML(R"(<form id=TestForm>
           <label for=firstname> First name: </label>
             <input id=firstname value=John>
           <label for=lastname> Last name: </label>
             <input id=lastname value=Smith>
           <label for=email> Email: </label>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

// <label for=fieldId> elements are correctly assigned to their inputs. Multiple
// labels are separated with a space.
TEST_F(FieldLabelInferenceTest, LabelForAttribute) {
  LoadHTML(R"(<label for=fieldId>foo</label>
              <label for=fieldId>bar</label>
              <input id=fieldId>)");
  ASSERT_NE(GetMainFrame(), nullptr);

  base::HistogramTester histogram_tester;
  // Simulate seeing an unowned form containing just the input "fieldID".
  std::optional<FormData> form = ExtractFormData(WebFormElement());
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(),
              ElementsAre(test::FormFieldDescriptionEq({
                  .label = u"foo bar",
                  .label_source = FormFieldData::LabelSource::kForId,
              })));
}

// Tests that when a label is assigned to an input, text behind it is considered
// as a fallback.
// The label is assigned to the input without the for-attribute, by declaring it
// it inside the label.
TEST_F(FieldLabelInferenceTest, LabelTextBehindInput) {
  LoadHTML(R"(
    <form id=TestForm>
      <label>
        <input>
        label
      </label>
    </form>
  )");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(),
              ElementsAre(test::FormFieldDescriptionEq({.label = u"label"})));
}

TEST_F(FieldLabelInferenceTest, LabelsWithSpans) {
  LoadHTML(R"(<form id=TestForm>
           <label for=firstname><span>First name: </span></label>
             <input id=firstname value=John>
           <label for=lastname><span>Last name: </span></label>
             <input id=lastname value=Smith>
           <label for=email><span>Email: </span></label>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

// This test is different from FormLabelsTest.Labels in that the label
// elements for= attribute is set to the name of the form control element it is
// a label for instead of the id of the form control element.  This is invalid
// because the for= attribute must be set to the id of the form control element;
// however, current label parsing code will extract the text from the previous
// label element and apply it to the following input field.
TEST_F(FieldLabelInferenceTest, InvalidLabels) {
  LoadHTML(
      R"(<form id=TestForm>
           <label for=firstname> First name: </label>
             <input name=firstname value=John>
           <label for=lastname> Last name: </label>
             <input name=lastname value=Smith>
           <label for=email> Email: </label>
             <input name=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithNameFieldsMatchers()));
}

// This test has three form control elements, only one of which has a label
// element associated with it.
TEST_F(FieldLabelInferenceTest, OneLabelElement) {
  LoadHTML(R"(<form id=TestForm>
           First name:
             <input id=firstname value=John>
           <label for=lastname>Last name: </label>
             <input id=lastname value=Smith>
           Email:
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromText) {
  LoadHTML(R"(<form id=TestForm>
           First name:
             <input id=firstname value=John>
           Last name:
             <input id=lastname value=Smith>
           Email:
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromParagraph) {
  LoadHTML(R"(<form id=TestForm>
           <p>First name:</p><input id=firstname value=John>
           <p>Last name:</p>
             <input id=lastname value=Smith>
           <p>Email:</p>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromBold) {
  LoadHTML(R"(<form id=TestForm>
           <b>First name:</b><input id=firstname value=John>
           <b>Last name:</b>
             <input id=lastname value=Smith>
           <b>Email:</b>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredPriorToImgOrBr) {
  LoadHTML(R"(<form id=TestForm>
           First name:<img><input id=firstname value=John>
           Last name:<img>
             <input id=lastname value=Smith>
           Email:<br>
             <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromTableCell) {
  LoadHTML(R"(<form id=TestForm>
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
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromTableCellTH) {
  LoadHTML(R"(<form id=TestForm>
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
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromTableCellNested) {
  LoadHTML(R"(<form id=TestForm>
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
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(
          test::FormFieldDescriptionEq({.label = u"First name: Bogus",
                                        .name = u"firstname",
                                        .name_attribute = u"",
                                        .id_attribute = u"firstname",
                                        .value = u"John"}),
          test::FormFieldDescriptionEq({.label = u"Last name:",
                                        .name = u"lastname",
                                        .name_attribute = u"",
                                        .id_attribute = u"lastname",
                                        .value = u"Smith"}),
          test::FormFieldDescriptionEq({.label = u"Email:",
                                        .name = u"email",
                                        .name_attribute = u"",
                                        .id_attribute = u"email",
                                        .value = u"john@example.com"})));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromTableEmptyTDs) {
  LoadHTML(R"(<form id=TestForm>
         <table>
                       <tr>
             <td><span>*</span><b>First Name</b></td>
             <td></td>
             <td>
               <input id=firstname value=John>
             </td>
           </tr>
           <tr>
             <td><span>*</span><b>Last Name</b></td>
             <td></td>
             <td>
               <input id=lastname value=Smith>
             </td>
           </tr>
           <tr>
             <td><span>*</span><b>Email</b></td>
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
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(),
              ElementsAreArray(StarredFirstLastEmailFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromPreviousTD) {
  LoadHTML(R"(<form id=TestForm>
         <table>
           <tr>
             <td>*First Name</td>
             <td>
               Bogus
               <input type=hidden>
               <input id=firstname value=John>
             </td>
           </tr>
           <tr>
             <td>*Last Name</td>
             <td>
               <input id=lastname value=Smith>
             </td>
           </tr>
           <tr>
             <td>*Email</td>
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
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(),
              ElementsAreArray(StarredFirstLastEmailFieldsMatchers()));
}

// <script>, <noscript> and <option> tags are excluded when the labels are
// inferred.
// Also <!-- comment --> is excluded.
TEST_F(FieldLabelInferenceTest, LabelsInferredFromTableWithSpecialElements) {
  LoadHTML(R"(<form id=TestForm>
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
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(test::FormFieldDescriptionEq(
                      {.label = u"* First Name",
                       .name = u"firstname",
                       .name_attribute = u"",
                       .id_attribute = u"firstname",
                       .value = u"John",
                       .form_control_type = FormControlType::kInputText}),
                  test::FormFieldDescriptionEq(
                      {.label = u"* Middle Name",
                       .name = u"middlename",
                       .name_attribute = u"",
                       .id_attribute = u"middlename",
                       .value = u"Joe",
                       .form_control_type = FormControlType::kInputText}),
                  test::FormFieldDescriptionEq(
                      {.label = u"* Last Name",
                       .name = u"lastname",
                       .name_attribute = u"",
                       .id_attribute = u"lastname",
                       .value = u"Smith",
                       .form_control_type = FormControlType::kInputText}),
                  test::FormFieldDescriptionEq(
                      {.label = u"* Country",
                       .name = u"country",
                       .name_attribute = u"",
                       .id_attribute = u"country",
                       .value = u"US",
                       .max_length = 0,
                       .form_control_type = FormControlType::kSelectOne}),
                  test::FormFieldDescriptionEq(
                      {.label = u"* Email",
                       .name = u"email",
                       .name_attribute = u"",
                       .id_attribute = u"email",
                       .value = u"john@example.com",
                       .form_control_type = FormControlType::kInputText})));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromTableLabels) {
  LoadHTML(R"(<form id=TestForm>
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
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromTableTDInterveningElements) {
  LoadHTML(R"(<form id=TestForm>
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
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

// Verify that we correctly infer labels when the label text spans multiple
// adjacent HTML elements, not separated by whitespace.
TEST_F(FieldLabelInferenceTest, LabelsInferredFromTableAdjacentElements) {
  LoadHTML(R"(<form id=TestForm>
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
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(),
              ElementsAreArray(StarredFirstLastEmailFieldsMatchers()));
}

// Verify that we correctly infer labels when the label text resides in the
// previous row.
TEST_F(FieldLabelInferenceTest, LabelsInferredFromTableRow) {
  LoadHTML(R"(<form id=TestForm>
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
         </table>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  std::vector<Matcher<FormFieldData>> matchers =
      StarredFirstLastEmailFieldsMatchers();
  base::Extend(
      matchers,
      std::array{test::FormFieldDescriptionEq({.label = u"NAME",
                                               .name = u"name2",
                                               .name_attribute = u"",
                                               .id_attribute = u"name2",
                                               .value = u"John Smith"}),
                 test::FormFieldDescriptionEq({.label = u"EMAIL",
                                               .name = u"email2",
                                               .name_attribute = u"",
                                               .id_attribute = u"email2",
                                               .value = u"john@example2.com"}),
                 test::FormFieldDescriptionEq({.label = u"Phone",
                                               .name = u"phone1",
                                               .name_attribute = u"",
                                               .id_attribute = u"phone1",
                                               .value = u"123"}),
                 test::FormFieldDescriptionEq({.label = u"Phone",
                                               .name = u"phone2",
                                               .name_attribute = u"",
                                               .id_attribute = u"phone2",
                                               .value = u"456"}),
                 test::FormFieldDescriptionEq({.label = u"Phone",
                                               .name = u"phone3",
                                               .name_attribute = u"",
                                               .id_attribute = u"phone3",
                                               .value = u"7890"}),
                 test::FormFieldDescriptionEq({.label = u"Credit Card Number",
                                               .name = u"ccnumber",
                                               .name_attribute = u"ccnumber",
                                               .value = u"4444555544445555"})});
  EXPECT_THAT(form->fields(), ElementsAreArray(matchers));
}

// Verify that we correctly infer labels when enclosed within a list item.
TEST_F(FieldLabelInferenceTest, LabelsInferredFromListItem) {
  LoadHTML(R"(<form id=TestForm name=TestForm action='http://cnn.com'>
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
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(test::FormFieldDescriptionEq({.label = u"* Home Phone",
                                                .name = u"areacode",
                                                .name_attribute = u"",
                                                .id_attribute = u"areacode",
                                                .value = u"415"}),
                  test::FormFieldDescriptionEq({.label = u"* Home Phone",
                                                .name = u"prefix",
                                                .name_attribute = u"",
                                                .id_attribute = u"prefix",
                                                .value = u"555"}),
                  test::FormFieldDescriptionEq({.label = u"* Home Phone",
                                                .name = u"suffix",
                                                .name_attribute = u"",
                                                .id_attribute = u"suffix",
                                                .value = u"1212"})));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromDefinitionList) {
  LoadHTML(R"(<form id=TestForm>
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
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(
          test::FormFieldDescriptionEq({.label = u"* First name: Bogus",
                                        .name = u"firstname",
                                        .name_attribute = u"",
                                        .id_attribute = u"firstname",
                                        .value = u"John"}),
          test::FormFieldDescriptionEq({.label = u"Last name:",
                                        .name = u"lastname",
                                        .name_attribute = u"",
                                        .id_attribute = u"lastname",
                                        .value = u"Smith"}),
          test::FormFieldDescriptionEq({.label = u"Email:",
                                        .name = u"email",
                                        .name_attribute = u"",
                                        .id_attribute = u"email",
                                        .value = u"john@example.com"})));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredWithSameName) {
  LoadHTML(R"(<form id=TestForm>
           Address Line 1:
             <input name=Address>
           Address Line 2:
             <input name=Address>
           Address Line 3:
             <input name=Address>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(
          test::FormFieldDescriptionEq({.label = u"Address Line 1:",
                                        .name = u"Address",
                                        .name_attribute = u"Address"}),
          test::FormFieldDescriptionEq({.label = u"Address Line 2:",
                                        .name = u"Address",
                                        .name_attribute = u"Address"}),
          test::FormFieldDescriptionEq({.label = u"Address Line 3:",
                                        .name = u"Address",
                                        .name_attribute = u"Address"})));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredWithImageTags) {
  LoadHTML(R"(<form id=TestForm name=TestForm action='http://cnn.com'>
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
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

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
                                        .name_attribute = u"dayphone4"}),
          test::FormFieldDescriptionEq(
              {.label = u"", .name = u"dummy", .name_attribute = u"dummy"})));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromDivTable) {
  LoadHTML(R"(<form id=TestForm>
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
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithNameFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromDivSiblingTable) {
  LoadHTML(R"(<form id=TestForm>
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
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithNameFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, LabelsInferredFromLabelInDivTable) {
  LoadHTML(R"(<form id=TestForm>
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
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest,
       LabelsInferredFromDefinitionListRatherThanDivTable) {
  LoadHTML(R"(<form id=TestForm name=TestForm action='http://cnn.com'>
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
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);
  EXPECT_EQ(u"TestForm", form->name());
  EXPECT_EQ(GURL("http://cnn.com"), form->action());
  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithIdFieldsMatchers()));
}

// If we have multiple labels per id, the labels concatenated into label string.
TEST_F(FieldLabelInferenceTest, MultipleLabelsPerElement) {
  LoadHTML(R"(<form id=TestForm>
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
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(
          test::FormFieldDescriptionEq({.label = u"First Name:",
                                        .name = u"firstname",
                                        .name_attribute = u"",
                                        .id_attribute = u"firstname",
                                        .value = u"John"}),
          test::FormFieldDescriptionEq({.label = u"Last Name:",
                                        .name = u"lastname",
                                        .name_attribute = u"",
                                        .id_attribute = u"lastname",
                                        .value = u"Smith"}),
          test::FormFieldDescriptionEq({.label = u"Email: xxx@yyy.com",
                                        .name = u"email",
                                        .name_attribute = u"",
                                        .id_attribute = u"email",
                                        .value = u"john@example.com"})));
}

TEST_F(FieldLabelInferenceTest, AriaLabelAndDescription) {
  LoadHTML(
      R"(<form id=form>
           <div id=label>aria label</div>
           <div id=description>aria description</div>
           <input id=field0 aria-label='inline aria label'>
           <input id=field1 aria-labelledby='label'>
           <input id=field2 aria-describedby='description'>
         </form>)");

  WebFormElement web_form = GetWebElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  WebFormControlElement control_element = GetFormControlElementById("field0");
  ASSERT_TRUE(control_element);
  FormData form = FindForm(control_element);

  EXPECT_THAT(form.fields(),
              ElementsAreArray(AriaLabelAndDescriptionFieldsMatchers()));
}

TEST_F(FieldLabelInferenceTest, AriaLabelAndDescription2) {
  LoadHTML(
      R"(<form id=form>
           <input id=field0 aria-label='inline aria label'>
           <input id=field1 aria-labelledby='label'>
           <input id=field2 aria-describedby='description'>
         </form>
         <div id=label>aria label</div>
         <div id=description>aria description</div>)");

  WebFormElement web_form = GetWebElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  WebFormControlElement control_element = GetFormControlElementById("field0");
  ASSERT_TRUE(control_element);
  FormData form = FindForm(control_element);

  EXPECT_THAT(form.fields(),
              ElementsAreArray(AriaLabelAndDescriptionFieldsMatchers()));
}

class FormDataConversionTest : public test::AutofillRendererTest {
 public:
  FormDataConversionTest() = default;

  FormDataConversionTest(const FormDataConversionTest&) = delete;
  FormDataConversionTest& operator=(const FormDataConversionTest&) = delete;

  ~FormDataConversionTest() override = default;

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
    return form_util::UpdateFormCache(*form_cache_);
  }

 protected:
  static std::vector<Matcher<FormFieldData>> JohnSmithFieldsMatchers() {
    return {test::FormFieldDescriptionEq({.label = u"First name:",
                                          .name = u"firstname",
                                          .name_attribute = u"",
                                          .id_attribute = u"firstname",
                                          .value = u"John"}),
            test::FormFieldDescriptionEq({.label = u"Last name:",
                                          .name = u"lastname",
                                          .name_attribute = u"",
                                          .id_attribute = u"lastname",
                                          .value = u"Smith"}),
            test::FormFieldDescriptionEq({.label = u"Email:",
                                          .name = u"email",
                                          .name_attribute = u"",
                                          .id_attribute = u"email",
                                          .value = u"john@example.com"})};
  }

  static std::vector<Matcher<FormFieldData>>
  JohnSmithWithLabelsFieldsMatchers() {
    return {test::FormFieldDescriptionEq({.label = u"John",
                                          .name = u"firstname",
                                          .id_attribute = u"firstname",
                                          .value = u"John"}),
            test::FormFieldDescriptionEq({.label = u"Smith",
                                          .name = u"lastname",
                                          .id_attribute = u"lastname",
                                          .value = u"Smith"}),
            test::FormFieldDescriptionEq({.label = u"john@example.com",
                                          .name = u"email",
                                          .id_attribute = u"email",
                                          .value = u"john@example.com"})};
  }

  static std::vector<Matcher<FormFieldData>>
  JohnSmithWithPhoneAndAddressFieldsMatchers() {
    std::vector<Matcher<FormFieldData>> matchers =
        JohnSmithWithLabelsFieldsMatchers();
    matchers.push_back(test::FormFieldDescriptionEq(
        {.label = u"1.800.555.1234",
         .name = u"phone",
         .id_attribute = u"phone",
         .value = u"1.800.555.1234",
         .form_control_type = FormControlType::kInputText}));
    matchers.push_back(test::FormFieldDescriptionEq(
        {.label = u"",
         .name = u"street-address",
         .id_attribute = u"street-address",
         .value = u"123 Fantasy Ln.\nApt. 42",
         .form_control_type = FormControlType::kTextArea}));
    return matchers;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<FormCache> form_cache_;
};

TEST_F(FormDataConversionTest, WebFormElementToFormData) {
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

  std::vector<WebFormElement> forms = GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());

  WebInputElement input_element = GetInputElementById("firstname");

  FormData form = FindForm(input_element);

  EXPECT_EQ(u"TestForm", form.name());
  EXPECT_EQ(form_util::GetFormRendererId(forms[0]), form.renderer_id());
  EXPECT_EQ(GURL("http://cnn.com/submit/"), form.action());

  EXPECT_THAT(
      form.fields(),
      ElementsAre(test::FormFieldDescriptionEq(
                      {.label = u"First name:",
                       .name = u"firstname",
                       .id_attribute = u"firstname",
                       .value = u"John",
                       .max_length = FormFieldData::kDefaultMaxLength,
                       .form_control_type = FormControlType::kInputText}),
                  test::FormFieldDescriptionEq(
                      {.label = u"Last name:",
                       .name = u"lastname",
                       .id_attribute = u"lastname",
                       .value = u"Smith",
                       .max_length = FormFieldData::kDefaultMaxLength,
                       .form_control_type = FormControlType::kInputText}),
                  test::FormFieldDescriptionEq(
                      {.label = u"Address:",
                       .name = u"street-address",
                       .id_attribute = u"street-address",
                       .value = u"123 Fantasy Ln.\nApt. 42",
                       .max_length = FormFieldData::kDefaultMaxLength,
                       .form_control_type = FormControlType::kTextArea}),
                  test::FormFieldDescriptionEq(
                      {.label = u"State:",
                       .name = u"state",
                       .id_attribute = u"state",
                       .value = u"CA",
                       .max_length = 0,
                       .form_control_type = FormControlType::kSelectOne}),
                  test::FormFieldDescriptionEq(
                      {.label = u"Password:",
                       .name = u"password",
                       .id_attribute = u"password",
                       .value = u"secret",
                       .max_length = FormFieldData::kDefaultMaxLength,
                       .form_control_type = FormControlType::kInputPassword}),
                  test::FormFieldDescriptionEq(
                      {.label = u"Card expiration:",
                       .name = u"month",
                       .id_attribute = u"month",
                       .value = u"2011-12",
                       .max_length = 0,
                       .form_control_type = FormControlType::kInputMonth})));

  // Check that the `renderer_id`s of the extracted `form.fields()` match the
  // IDs of the form control elements.
  EXPECT_EQ(base::ToVector(form.fields(), &FormFieldData::renderer_id),
            base::ToVector(form_util::GetOwnedAutofillableFormControls(
                               forms[0].GetDocument(), forms[0]),
                           &form_util::GetFieldRendererId));
}

TEST_F(FormDataConversionTest,
       WebFormElementConsiderNonControlLabelableElements) {
  LoadHTML(R"(<form id=form>
                <label for=progress>Progress:</label>
                <progress id=progress></progress>
                <label for=firstname>First name:</label>
                <input id=firstname value=John>
              </form>)");

  WebFormElement web_form =
      GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  std::optional<FormData> form = ExtractFormData(web_form);
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(test::FormFieldDescriptionEq({.name = u"firstname"})));
}

// We should not be able to serialize a form with too many fillable fields.
TEST_F(FormDataConversionTest, WebFormElementToFormData_TooManyFields) {
  std::string html = "<form name=TestForm action='http://cnn.com'>";
  for (size_t i = 0; i < (kMaxExtractableFields + 1); ++i) {
    html += "<input>";
  }
  html += "</form>";
  LoadHTML(html.c_str());

  std::vector<WebFormElement> forms = GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());
  std::vector<WebFormControlElement> form_controls =
      form_util::GetOwnedAutofillableFormControls(GetDocument(), forms.front());
  ASSERT_FALSE(form_controls.empty());

  WebInputElement input_element =
      form_controls.front().DynamicTo<WebInputElement>();

  EXPECT_THAT(FindForm(input_element),
              Property(&FormData::fields,
                       ElementsAre(Property(
                           &FormFieldData::renderer_id,
                           form_util::GetFieldRendererId(input_element)))));
}

// Tests that the `should_autocomplete` is set to false for all the fields when
// an autocomplete='off' attribute is set for the form in HTML.
TEST_F(FormDataConversionTest,
       WebFormElementToFormData_AutocompleteOff_OnForm) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com' autocomplete=off>
           <label for=firstname>First name:</label>
             <input id=firstname value=John>
           <label for=lastname>Last name:</label>
             <input id=lastname value=Smith>
           <label for='street-address'>Address:</label>
             <input id=addressline1 value='123 Test st.'>
         </form>)");

  WebFormElement web_form =
      GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  std::optional<FormData> form = ExtractFormData(web_form);
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      Each(test::FormFieldDescriptionEq({.should_autocomplete = false})));
}

// Tests that the `should_autocomplete` is set to false only for the field
// which has an autocomplete='off' attribute set for it in HTML.
TEST_F(FormDataConversionTest,
       WebFormElementToFormData_AutocompleteOff_OnField) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com'>
           <label for=firstname>First name:</label>
             <input id=firstname value=John autocomplete=off>
           <label for=lastname>Last name:</label>
             <input id=lastname value=Smith>
           <label for='street-address'>Address:</label>
             <input id=addressline1 value='123 Test st.'>
         </form>)");

  WebFormElement web_form =
      GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  std::optional<FormData> form = ExtractFormData(web_form);
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(test::FormFieldDescriptionEq({.should_autocomplete = false}),
                  test::FormFieldDescriptionEq({.should_autocomplete = true}),
                  test::FormFieldDescriptionEq({.should_autocomplete = true})));
}

// `should_autocomplete` must be set to false for the field with
// autocomplete='one-time-code' attribute set in HTML.
TEST_F(FormDataConversionTest,
       WebFormElementToFormData_AutocompleteOff_OneTimeCode) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com'>
           <input value=123 autocomplete='one-time-code'>
         </form>)");

  WebFormElement web_form =
      GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  std::optional<FormData> form = ExtractFormData(web_form);
  ASSERT_TRUE(form);

  EXPECT_THAT(form->fields(), ElementsAre(test::FormFieldDescriptionEq(
                                  {.should_autocomplete = false})));
}

// Tests CSS classes are set.
TEST_F(FormDataConversionTest, WebFormElementToFormData_CssClasses) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com' autocomplete=off>
           <input id=firstname class='firstname_field'>
           <input id=lastname class='lastname_field'>
           <input id=addressline1>
         </form>)");

  WebFormElement web_form =
      GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  std::optional<FormData> form = ExtractFormData(web_form);
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(
          test::FormFieldDescriptionEq({.css_classes = u"firstname_field"}),
          test::FormFieldDescriptionEq({.css_classes = u"lastname_field"}),
          test::FormFieldDescriptionEq({.css_classes = u""})));
}

// Tests id attributes are set.
TEST_F(FormDataConversionTest, WebFormElementToFormData_IdAttributes) {
  LoadHTML(
      R"(<form name=TestForm id=form action='http://cnn.com' autocomplete=off>
           <input name=name1 id=firstname>
           <input name=name2 id=lastname>
           <input name=same id=same>
           <input id=addressline1>
         </form>)");

  WebFormElement web_form =
      GetDocument().GetElementById("form").To<WebFormElement>();
  ASSERT_TRUE(web_form);

  std::optional<FormData> form = ExtractFormData(web_form);
  ASSERT_TRUE(form);

  EXPECT_THAT(
      form->fields(),
      ElementsAre(
          test::FormFieldDescriptionEq({.name = u"name1",
                                        .name_attribute = u"name1",
                                        .id_attribute = u"firstname"}),
          test::FormFieldDescriptionEq({.name = u"name2",
                                        .name_attribute = u"name2",
                                        .id_attribute = u"lastname"}),
          test::FormFieldDescriptionEq({.name = u"same",
                                        .name_attribute = u"same",
                                        .id_attribute = u"same"}),
          test::FormFieldDescriptionEq({.name = u"addressline1",
                                        .name_attribute = u"",
                                        .id_attribute = u"addressline1"})));
}

TEST_F(FormDataConversionTest, ExtractForms) {
  LoadHTML(R"(<form id=TestForm name=TestForm action='http://cnn.com'>
           First name: <input id=firstname value=John>
           Last name: <input id=lastname value=Smith>
           Email: <input id=email value='john@example.com'>
           <input type=submit name='reply-send' value=Send>
         </form>)");
  std::optional<FormData> form = ExtractFormData("TestForm");
  ASSERT_TRUE(form);
  EXPECT_EQ(u"TestForm", form->name());
  EXPECT_EQ(GURL("http://cnn.com"), form->action());
  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithFieldsMatchers()));
}

TEST_F(FormDataConversionTest, ExtractMultipleForms) {
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

  EXPECT_THAT(form.fields(),
              ElementsAreArray(JohnSmithWithLabelsFieldsMatchers()));

  // Second form.
  const FormData& form2 = forms[1];
  EXPECT_EQ(u"TestForm2", form2.name());
  EXPECT_EQ(GURL("http://zoo.com"), form2.action());

  EXPECT_THAT(
      form2.fields(),
      ElementsAre(
          test::FormFieldDescriptionEq({.label = u"Jack",
                                        .name = u"firstname",
                                        .id_attribute = u"firstname",
                                        .value = u"Jack"}),
          test::FormFieldDescriptionEq({.label = u"Adams",
                                        .name = u"lastname",
                                        .id_attribute = u"lastname",
                                        .value = u"Adams"}),
          test::FormFieldDescriptionEq({.label = u"jack@example.com",
                                        .name = u"email",
                                        .id_attribute = u"email",
                                        .value = u"jack@example.com"})));
}

TEST_F(FormDataConversionTest, OnlyExtractNewForms) {
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

  forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  std::vector<Matcher<FormFieldData>> expected_fields =
      JohnSmithWithLabelsFieldsMatchers();
  expected_fields.push_back(
      test::FormFieldDescriptionEq({.label = u"",
                                    .name = u"telephone",
                                    .id_attribute = u"telephone",
                                    .value = u"12345"}));
  EXPECT_THAT(forms[0].fields(), ElementsAreArray(expected_fields));

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

  forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  EXPECT_THAT(
      forms[0].fields(),
      ElementsAre(
          test::FormFieldDescriptionEq({.label = u"",
                                        .name = u"second_firstname",
                                        .id_attribute = u"second_firstname",
                                        .value = u"Bob"}),
          test::FormFieldDescriptionEq({.label = u"",
                                        .name = u"second_lastname",
                                        .id_attribute = u"second_lastname",
                                        .value = u"Hope"}),
          test::FormFieldDescriptionEq({.label = u"",
                                        .name = u"second_email",
                                        .id_attribute = u"second_email",
                                        .value = u"bobhope@example.com"})));
}

// We should not report additional forms for empty forms.
TEST_F(FormDataConversionTest, ExtractFormsNoFields) {
  LoadHTML(R"(<form name=TestForm action='http://cnn.com'>
              </form>)");

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_TRUE(forms.empty());
}

TEST_F(FormDataConversionTest, WebFormElementToFormData_Autocomplete) {
  // Form is still Autofill-able despite autocomplete=off.
  LoadHTML(
      R"(<form name=TestForm action='http://cnn.com' autocomplete=off>
             <input id=firstname value=John>
             <input id=lastname value=Smith>
             <input id=email value='john@example.com'>
             <input type=submit name='reply-send' value=Send>
           </form>)");

  std::vector<WebFormElement> web_forms = GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, web_forms.size());
  WebFormElement web_form = web_forms[0];

  EXPECT_TRUE(ExtractFormData(web_form));
}

TEST_F(FormDataConversionTest, SelectOneAsText) {
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

  // Set the value of the select-one.
  WebSelectElement select_element =
      GetDocument().GetElementById("country").To<WebSelectElement>();
  select_element.SetValue(WebString("AL"));

  std::vector<WebFormElement> forms = GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());

  std::optional<FormData> form = ExtractFormData(forms.front());
  ASSERT_TRUE(form);
  EXPECT_EQ(form->name(), u"TestForm");
  EXPECT_EQ(form->action(), GURL("http://cnn.com"));

  EXPECT_THAT(
      form->fields(),
      ElementsAre(test::FormFieldDescriptionEq({.label = u"John",
                                                .name = u"firstname",
                                                .id_attribute = u"firstname",
                                                .value = u"John"}),
                  test::FormFieldDescriptionEq({.label = u"Smith",
                                                .name = u"lastname",
                                                .id_attribute = u"lastname",
                                                .value = u"Smith"}),
                  test::FormFieldDescriptionEq({.label = u"",
                                                .name = u"country",
                                                .id_attribute = u"country",
                                                .value = u"AL"})));
}

TEST_F(FormDataConversionTest, UnownedFormElementsToFormDataWithoutForm) {
  LoadHTML(R"(<head><title>delivery info</title></head>
              <div>
                <label for=firstname>First name:</label>
                <label for=lastname>Last name:</label>
                <input id=firstname value=John>
                <input id=lastname value=Smith>
                <label for=email>Email:</label>
                <input id=email value='john@example.com'>
              </div>)");
  std::optional<FormData> form = ExtractFormData(WebFormElement());
  ASSERT_TRUE(form);

  EXPECT_TRUE(form->name().empty());
  EXPECT_FALSE(form->action().is_valid());

  EXPECT_THAT(form->fields(), ElementsAreArray(JohnSmithFieldsMatchers()));
}

struct FormAutofillTestParam {
  std::string html;
  bool unowned;
};

class FormAutofillFindFormTest
    : public FormDataConversionTest,
      public WithParamInterface<FormAutofillTestParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormAutofillFindFormTest,
    ValuesIn(std::to_array<FormAutofillTestParam>(
        {{.html =
              R"(<form name=TestForm action='http://abc.com'>
              <input id=firstname value=John>
              <input id=lastname value=Smith>
              <input id=email value='john@example.com'>
              <input id=phone value=1.800.555.1234>
              <textarea id=street-address>123 Fantasy Ln.&#10;Apt. 42</textarea>
              <input type=submit value=Send>
              </form>)",
          .unowned = false},
         {.html =
              R"(<head><title>delivery recipient info</title></head>
              <input id=firstname value=John>
              <input id=lastname value=Smith>
              <input id=email value='john@example.com'>
              <input id=phone value=1.800.555.1234>
              <textarea id=street-address>123 Fantasy Ln.&#10;Apt. 42</textarea>
              <input type=submit value=Send>)",
          .unowned = true}})),
    [](const TestParamInfo<FormAutofillFindFormTest::ParamType>& param_info) {
      return param_info.param.unowned ? "Unowned" : "Owned";
    });

TEST_P(FormAutofillFindFormTest, FindFormForInputElement) {
  LoadHTML(GetParam().html);

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebInputElement input_element = GetInputElementById("phone");

  // Find the form and verify it's the correct form.
  FormData form = FindForm(input_element);
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form.name());
    EXPECT_EQ(GURL("http://abc.com"), form.action());
  }

  EXPECT_THAT(form.fields(),
              ElementsAreArray(JohnSmithWithPhoneAndAddressFieldsMatchers()));
}

TEST_P(FormAutofillFindFormTest, FindFormForTextAreaElement) {
  LoadHTML(GetParam().html);

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Get the textarea element we want to find.
  WebElement element = GetDocument().GetElementById("street-address");
  WebFormControlElement textarea_element = element.To<WebFormControlElement>();

  // Find the form and verify it's the correct form.
  FormData form = FindForm(textarea_element);
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form.name());
    EXPECT_EQ(GURL("http://abc.com"), form.action());
  }

  EXPECT_THAT(form.fields(),
              ElementsAreArray(JohnSmithWithPhoneAndAddressFieldsMatchers()));
}

struct FormCacheExtractNewFormsTestParam {
  std::string html;
  size_t expected_number_of_extracted_forms;
  bool expected_is_form_tag;
};

class FormCacheExtractNewFormsTest
    : public FormDataConversionTest,
      public WithParamInterface<FormCacheExtractNewFormsTestParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormCacheExtractNewFormsTest,
    ValuesIn(std::to_array<FormCacheExtractNewFormsTestParam>({
        // An empty form should not be extracted.
        {.html = R"(<form name=TestForm action='http://abc.com'></form>)",
         .expected_number_of_extracted_forms = 0u,
         .expected_is_form_tag = true},
        // A form with less than three fields with no autocomplete type(s)
        // should be extracted because no minimum is being enforced for upload.
        {.html = R"(<form name=TestForm action='http://abc.com'>
                         <input id=firstname>
                       </form>)",
         .expected_number_of_extracted_forms = 1u,
         .expected_is_form_tag = true},
        // A form with less than three fields with at least one autocomplete
        // type should be extracted.
        {.html = R"(<form name=TestForm action='http://abc.com'>
                         <input id=firstname autocomplete='given-name'>
                       </form>)",
         .expected_number_of_extracted_forms = 1u,
         .expected_is_form_tag = true},
        // A form with three or more fields should be extracted.
        {.html = R"(<form name=TestForm action='http://abc.com'>
                         <input id=firstname>
                         <input id=lastname>
                         <input id=email>
                         <input type=submit value=Send>
                       </form>)",
         .expected_number_of_extracted_forms = 1u,
         .expected_is_form_tag = true},
        // An input field with an autocomplete attribute outside of a form
        // should be extracted.
        {.html = R"(<input id=firstname autocomplete='given-name'>
                       <input type=submit value=Send>)",
         .expected_number_of_extracted_forms = 1u,
         .expected_is_form_tag = false},
        // An input field without an autocomplete attribute outside of a form,
        // with no checkout hints, should not be extracted.
        {.html = R"(<input id=firstname>
                       <input type=submit value=Send>)",
         .expected_number_of_extracted_forms = 1u,
         .expected_is_form_tag = false},
        // A form with one field which is password gets extracted.
        {.html = R"(<form name=TestForm action='http://abc.com'>
                         <input type=password id=pw>
                       </form>)",
         .expected_number_of_extracted_forms = 1u,
         .expected_is_form_tag = true},
        // A form with two fields which are passwords should be extracted.
        {.html = R"(<form name=TestForm action='http://abc.com'>
                         <input type=password id=pw>
                         <input type=password id=new_pw>
                       </form>)",
         .expected_number_of_extracted_forms = 1u,
         .expected_is_form_tag = true},
    })),
    [](const TestParamInfo<FormCacheExtractNewFormsTest::ParamType>& info) {
      constexpr auto kNames = std::to_array<std::string_view>({
          "EmptyForm",
          "SmallFormNoAutocomplete",
          "SmallFormWithAutocomplete",
          "ThreeFieldForm",
          "SmallFormlessWithAutocomplete",
          "SmallFormlessNoAutocomplete",
          "PasswordOnly",
          "TwoPasswords",
      });
      return std::string{kNames[info.index]};
    });

TEST_P(FormCacheExtractNewFormsTest, ExtractNewForms) {
  const FormCacheExtractNewFormsTestParam& test_case = GetParam();
  LoadHTML(std::string(test_case.html).c_str());

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  EXPECT_EQ(test_case.expected_number_of_extracted_forms, forms.size());
  if (!forms.empty()) {
    EXPECT_EQ(test_case.expected_is_form_tag,
              !forms.back().renderer_id().is_null());
  }
}

class FormFieldConversionTest : public test::AutofillRendererTest {
 public:
  FormFieldConversionTest() = default;
  FormFieldConversionTest(const FormFieldConversionTest&) = delete;
  FormFieldConversionTest& operator=(const FormFieldConversionTest&) = delete;
  ~FormFieldConversionTest() override = default;

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<FormCache> form_cache_;
};

// We should be able to extract a normal text field.
TEST_F(FormFieldConversionTest, WebFormControlElementToFormField) {
  LoadHTML(R"(<input id=element value=value>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");

  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq({.name = u"element",
                                                    .id_attribute = u"element",
                                                    .value = u"value"}));
}
// We should be able to extract a nonce even if CSP is enabled (which clears
// the nonce content attribute).
TEST_F(FormFieldConversionTest, WebFormControlElementToFormFieldNonceWithCSP) {
  LoadHTML(R"(
    <meta http-equiv="Content-Security-Policy" content="default-src 'self'">
    <input id=element nonce=testnonce>
  )");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");

  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

  EXPECT_EQ(result.nonce(), u"testnonce");
}

// We should be able to extract a text field with autocomplete="off".
TEST_F(FormFieldConversionTest,
       WebFormControlElementToFormFieldAutocompleteOff) {
  LoadHTML(R"(<input id=element value=value autocomplete=off>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

  EXPECT_THAT(result,
              test::FormFieldDescriptionEq({.name = u"element",
                                            .id_attribute = u"element",
                                            .value = u"value",
                                            .autocomplete_attribute = "off"}));
}

// We should be able to extract a text field with maxlength specified.
TEST_F(FormFieldConversionTest, WebFormControlElementToFormFieldMaxLength) {
  LoadHTML(R"(<input id=element value=value maxlength=5>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq({.name = u"element",
                                                    .id_attribute = u"element",
                                                    .value = u"value",
                                                    .max_length = 5}));
}

// We should be able to extract a text field that has been autofilled.
TEST_F(FormFieldConversionTest, WebFormControlElementToFormFieldAutofilled) {
  LoadHTML(R"(<input id=element value=value>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebInputElement element = GetInputElementById("element");
  element.SetAutofillState(WebAutofillState::kAutofilled);
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq(
                          {.name = u"element",
                           .id_attribute = u"element",
                           .value = u"value",
                           .is_autofilled_according_to_renderer = true}));
}

// We should be able to extract a <select> field.
TEST_F(FormFieldConversionTest, WebFormControlElementToFormFieldSelect) {
  LoadHTML(R"(<select id=element>
                <option value=CA>California</option>
                <option value=TX>Texas</option>
              </select>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

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
TEST_F(FormFieldConversionTest,
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
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

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
TEST_F(FormFieldConversionTest, WebFormControlElementToFormFieldLongSelect) {
  std::string html = R"(<select id=element>)";
  for (size_t i = 0; i < 2 * kMaxListSize; ++i) {
    base::StrAppend(&html, {"<option value='", base::NumberToString(i), "'>",
                            base::NumberToString(i), "</option>"});
  }
  html += "</select>";
  LoadHTML(html.c_str());

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_TRUE(frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

  EXPECT_TRUE(result.options().empty());
}

// Test that we use the aria-label as the content if the <option> has no text.
TEST_F(FormFieldConversionTest,
       WebFormControlElementToFormFieldSelectAriaLabel) {
  LoadHTML(
      R"(<select id=element>
         <option aria-label='usa'><img></option>
         <option aria-label='uk'><img></option>
         </select>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);
  WebFormControlElement element = GetFormControlElementById("element");

  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);
  ASSERT_EQ(2u, result.options().size());
  EXPECT_EQ(u"usa", result.options()[0].text);
  EXPECT_EQ(u"uk", result.options()[1].text);
}

// Test that the content for the <option> can be computed when the <option>s
// have nested HTML nodes.
TEST_F(FormFieldConversionTest,
       WebFormControlElementToFormFieldSelectNestedNodes) {
  LoadHTML(
      R"(<select id=element>
           <option><div><img><b>+1</b> (Canada)</div></option>
         </select>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);
  WebFormControlElement element = GetFormControlElementById("element");

  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);
  ASSERT_EQ(1u, result.options().size());
  EXPECT_EQ(u"+1 (Canada)", result.options()[0].text);
}

// We should be able to extract a <textarea> field.
TEST_F(FormFieldConversionTest, WebFormControlElementToFormFieldTextArea) {
  LoadHTML(R"(<textarea id=element>This element's value
spans multiple lines.</textarea>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq(
                          {.name = u"element",
                           .id_attribute = u"element",
                           .value = u"This element's value\n"
                                    u"spans multiple lines.",
                           .form_control_type = FormControlType::kTextArea}));
}

// We should be able to extract an <input type=month> field.
TEST_F(FormFieldConversionTest, WebFormControlElementToFormFieldMonthInput) {
  LoadHTML(R"(<input type=month id=element value='2011-12'>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result_sans_value;
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

  EXPECT_THAT(result, test::FormFieldDescriptionEq(
                          {.name = u"element",
                           .id_attribute = u"element",
                           .value = u"2011-12",
                           .max_length = 0,
                           .form_control_type = FormControlType::kInputMonth}));
}

// We should be able to extract password fields.
TEST_F(FormFieldConversionTest, WebFormControlElementToPasswordFormField) {
  LoadHTML(R"(<form name=TestForm action='http://cnn.com'>
                <input type=password id=password value=secret>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("password");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      element.GetOwningFormForAutofill(), element, nullptr, &result);

  EXPECT_THAT(result,
              test::FormFieldDescriptionEq(
                  {.name = u"password",
                   .id_attribute = u"password",
                   .value = u"secret",
                   .form_control_type = FormControlType::kInputPassword}));
}

TEST_F(FormFieldConversionTest, DetectTextDirectionFromDirectStyle) {
  LoadHTML(R"(<style>input{direction:rtl}</style>
              <form>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      element.GetOwningFormForAutofill(), element, nullptr, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormFieldConversionTest, DetectTextDirectionFromDirectDIRAttribute) {
  LoadHTML(R"(<form>
                <input dir=rtl type=text id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      element.GetOwningFormForAutofill(), element, nullptr, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormFieldConversionTest, DetectTextDirectionFromParentStyle) {
  LoadHTML(R"(<style>form{direction:rtl}</style>
              <form>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      element.GetOwningFormForAutofill(), element, nullptr, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormFieldConversionTest, DetectTextDirectionFromParentDIRAttribute) {
  LoadHTML(R"(<form dir=rtl>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      element.GetOwningFormForAutofill(), element, nullptr, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

TEST_F(FormFieldConversionTest,
       DetectTextDirectionWhenStyleAndDIRAttributeMixed) {
  LoadHTML(R"(<style>input{direction:ltr}</style>
              <form dir=rtl>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      element.GetOwningFormForAutofill(), element, nullptr, &result);
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, result.text_direction());
}

TEST_F(FormFieldConversionTest,
       DetectTextDirectionWhenParentHasBothDIRAttributeAndStyle) {
  LoadHTML(R"(<style>form{direction:ltr}</style>
              <form dir=rtl>
                <input id=element>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      element.GetOwningFormForAutofill(), element, nullptr, &result);
  EXPECT_EQ(base::i18n::LEFT_TO_RIGHT, result.text_direction());
}

TEST_F(FormFieldConversionTest, DetectTextDirectionWhenAncestorHasInlineStyle) {
  LoadHTML(R"(<form style='direction:ltr'>
                <span dir=rtl>
                  <input id=element>
                </span>
              </form>)");

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      element.GetOwningFormForAutofill(), element, nullptr, &result);
  EXPECT_EQ(base::i18n::RIGHT_TO_LEFT, result.text_direction());
}

struct WebFormControlElementAutocompleteAttributeParams {
  std::string element_id;
  FormControlType form_control_type;
  std::string autocomplete_attribute;
  std::string value;
};

class WebFormControlElementAutocompleteAttributeTest
    : public FormFieldConversionTest,
      public WithParamInterface<
          WebFormControlElementAutocompleteAttributeParams> {
 protected:
  static std::string html() {
    return base::StrCat({
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
           </textarea>
           <input id=malicious autocomplete=')",
        std::string(10000, 'x'),
        "'>",
    });
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    WebFormControlElementAutocompleteAttributeTest,
    ValuesIn(std::to_array<WebFormControlElementAutocompleteAttributeParams>({
        // An absent attribute is equivalent to an empty one.
        {.element_id = "absent",
         .form_control_type = FormControlType::kInputText,
         .autocomplete_attribute = "",
         .value = ""},
        // Make sure there are no issues parsing an empty attribute.
        {.element_id = "empty",
         .form_control_type = FormControlType::kInputText,
         .autocomplete_attribute = "",
         .value = ""},
        // Make sure there are no issues parsing an attribute value that isn't
        // a
        // type hint.
        {.element_id = "off",
         .form_control_type = FormControlType::kInputText,
         .autocomplete_attribute = "off",
         .value = ""},
        // Common case: exactly one type specified.
        {.element_id = "regular",
         .form_control_type = FormControlType::kInputText,
         .autocomplete_attribute = "email",
         .value = ""},
        // Verify that we correctly extract multiple tokens as well.
        {.element_id = "multi-valued",
         .form_control_type = FormControlType::kInputText,
         .autocomplete_attribute = "billing email",
         .value = ""},
        // Verify that <input type=month> fields are supported.
        {.element_id = "month",
         .form_control_type = FormControlType::kInputMonth,
         .autocomplete_attribute = "cc-exp",
         .value = ""},
        // We previously extracted this data from the experimental
        // 'x-autocompletetype' attribute.  Now that the field type hints are
        // part
        // of the spec under the autocomplete attribute, we no longer support
        // the
        // experimental version.
        {.element_id = "experimental",
         .form_control_type = FormControlType::kInputText,
         .autocomplete_attribute = "",
         .value = ""},
        // <select> elements should behave no differently from text fields
        // here.
        {.element_id = "select",
         .form_control_type = FormControlType::kSelectOne,
         .autocomplete_attribute = "state",
         .value = "CA"},
        // <textarea> elements should also behave no differently from text
        // fields.
        {.element_id = "textarea",
         .form_control_type = FormControlType::kTextArea,
         .autocomplete_attribute = "street-address",
         .value =
             "             Some multi-\n             lined value\n           "},
        // Very long attribute values should be replaced by a default string,
        // to
        // prevent malicious websites from DOSing the browser process.
        {.element_id = "malicious",
         .form_control_type = FormControlType::kInputText,
         .autocomplete_attribute = "x-max-data-length-exceeded",
         .value = ""},
    })),
    [](const TestParamInfo<
        WebFormControlElementAutocompleteAttributeTest::ParamType>& info) {
      std::string name(info.param.element_id);
      std::ranges::replace(name, '-', '_');
      return name;
    });

// Tests that the autocompletetype attribute is extracted correctly.
TEST_P(WebFormControlElementAutocompleteAttributeTest,
       WebFormControlElementToFormFieldAutocompletetype) {
  LoadHTML(html());

  WebFormControlElement element =
      GetFormControlElementById(std::string(GetParam().element_id));
  ASSERT_FALSE(element.IsNull());

  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      WebFormElement(), element, nullptr, &result);

  EXPECT_THAT(
      result,
      test::FormFieldDescriptionEq(
          {.name = UTF8ToUTF16(GetParam().element_id),
           .id_attribute = UTF8ToUTF16(GetParam().element_id),
           .value = UTF8ToUTF16(GetParam().value),
           .max_length =
               (GetParam().form_control_type == FormControlType::kInputText ||
                GetParam().form_control_type == FormControlType::kTextArea)
                   ? FormFieldData::kDefaultMaxLength
                   : 0,
           .autocomplete_attribute =
               std::string(GetParam().autocomplete_attribute),
           .parsed_autocomplete = ParseAutocompleteAttribute(
               std::string(GetParam().autocomplete_attribute)),
           .form_control_type = GetParam().form_control_type}));
}

struct TextAlignOverridesDirectionParams {
  std::string style;
  base::i18n::TextDirection expected_direction;
};

class TextAlignOverridesDirectionTest
    : public FormFieldConversionTest,
      public WithParamInterface<TextAlignOverridesDirectionParams> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    TextAlignOverridesDirectionTest,
    ValuesIn(std::to_array<TextAlignOverridesDirectionParams>(
        {{.style = "direction:rtl;text-align:left",
          .expected_direction = base::i18n::LEFT_TO_RIGHT},
         {.style = "direction:rtl;text-align:right",
          .expected_direction = base::i18n::RIGHT_TO_LEFT},
         {.style = "direction:ltr;text-align:left",
          .expected_direction = base::i18n::LEFT_TO_RIGHT},
         {.style = "direction:ltr;text-align:right",
          .expected_direction = base::i18n::RIGHT_TO_LEFT}})),
    [](const TestParamInfo<TextAlignOverridesDirectionTest::ParamType>& info) {
      std::string name(info.param.style);
      std::ranges::replace_if(
          name, [](char c) { return !std::isalnum(c); }, '_');
      return name;
    });

TEST_P(TextAlignOverridesDirectionTest, TextAlignOverridesDirection) {
  std::string html = base::StrCat({"<style>input{", GetParam().style,
                                   "}</style><form><input id=element></form>"});
  LoadHTML(html.c_str());

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_NE(nullptr, frame);

  WebFormControlElement element = GetFormControlElementById("element");
  FormFieldData result;
  form_util::WebFormControlElementToFormFieldForTesting(
      element.GetOwningFormForAutofill(), element, nullptr, &result);
  EXPECT_EQ(GetParam().expected_direction, result.text_direction());
}

class FormAutofillWithConstraintsTest : public test::AutofillRendererTest {
 public:
  FormAutofillWithConstraintsTest() = default;

  FormAutofillWithConstraintsTest(const FormAutofillWithConstraintsTest&) =
      delete;
  FormAutofillWithConstraintsTest& operator=(
      const FormAutofillWithConstraintsTest&) = delete;

  ~FormAutofillWithConstraintsTest() override = default;

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
    return form_util::UpdateFormCache(*form_cache_);
  }

 protected:
  static std::vector<Matcher<FormFieldData>> FirstLastEmailIdFieldsMatchers() {
    return {test::FormFieldDescriptionEq(
                {.name = u"firstname", .id_attribute = u"firstname"}),
            test::FormFieldDescriptionEq(
                {.name = u"lastname", .id_attribute = u"lastname"}),
            test::FormFieldDescriptionEq(
                {.name = u"email", .id_attribute = u"email"})};
  }

  static std::vector<Matcher<FormFieldData>>
  CreditCardAutofilledFieldsMatchers() {
    return {test::FormFieldDescriptionEq(
                {.label = u"Credit Card Number",
                 .name = u"cc",
                 .id_attribute = u"cc",
                 .value = u"1111-2222-3333-4444",
                 .placeholder = u"Credit Card Number",
                 .is_autofilled_according_to_renderer = true}),
            test::FormFieldDescriptionEq(
                {.label = u"Expiration Date",
                 .name = u"expiration_date",
                 .id_attribute = u"expiration_date",
                 .value = u"03/2030",
                 .placeholder = u"Expiration Date",
                 .is_autofilled_according_to_renderer = true}),
            test::FormFieldDescriptionEq(
                {.label = u"Full Name",
                 .name = u"name",
                 .id_attribute = u"name",
                 .value = u"John Smith",
                 .placeholder = u"Full Name",
                 .is_autofilled_according_to_renderer = false})};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // We use a fresh `FormCache` in this fixture because the `AutofillAgent`'s
  // cache is used and populated by `AutofillAgent`.
  std::optional<FormCache> form_cache_;
};

TEST_F(FormAutofillWithConstraintsTest, ThreePartPhone) {
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

// This test re-creates the experience of typing in a field then selecting a
// profile from the Autofill suggestions popup.  The field that is being typed
// into should be filled even though it's not technically empty.
TEST_F(FormAutofillWithConstraintsTest, MaxLengthFields) {
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

// Tests that loading, dynamically editing, and then autofilling the form in
// an HTML string yields a specific result.
//
// The form contains the fields first name, last name, phone, credit card
// number, city, and state, each with the placeholder attribute set.
//
// Each field's value is modified dynamically. The second one is explicitly
// marked as user-edited; the other ones are not. The third and fourth field's
// values are typical placeholder values that are expected to be ignored.
TEST_F(FormAutofillWithConstraintsTest, FillFormModifyValues) {
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
  WebFormElement form_element = input_element.GetOwningFormForAutofill();
  std::vector<WebFormControlElement> control_elements =
      form_util::GetOwnedAutofillableFormControls(input_element.GetDocument(),
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
TEST_F(FormAutofillWithConstraintsTest, FillFormModifyInitiatingValue) {
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
  WebFormElement form_element = input_element.GetOwningFormForAutofill();
  std::vector<WebFormControlElement> control_elements =
      form_util::GetOwnedAutofillableFormControls(input_element.GetDocument(),
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
              ElementsAreArray(CreditCardAutofilledFieldsMatchers()));

  // Verify that the cursor position has been updated.
  EXPECT_EQ(19u, input_element.SelectionStart());
  EXPECT_EQ(19u, input_element.SelectionEnd());
}

// Similar to test case `FillFormModifyValues`.
TEST_F(FormAutofillWithConstraintsTest, FillFormJSModifiesUserInputValue) {
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
  WebFormElement form_element = input_element.GetOwningFormForAutofill();
  std::vector<WebFormControlElement> control_elements =
      form_util::GetOwnedAutofillableFormControls(input_element.GetDocument(),
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
              ElementsAreArray(CreditCardAutofilledFieldsMatchers()));

  // Verify that the cursor position has been updated.
  EXPECT_EQ(19u, input_element.SelectionStart());
  EXPECT_EQ(19u, input_element.SelectionEnd());
}

TEST_F(FormAutofillWithConstraintsTest, UndoAutofill) {
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
    return AllOf(
        Property(&WebFormControlElement::Value, value),
        Property(&WebFormControlElement::GetAutofillState, autofill_state));
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

struct FillFormNonEmptyFieldParams {
  std::string html;
  bool unowned;
  std::optional<std::string> initial_lastname;
  std::optional<std::string> initial_email;
  std::optional<std::string> placeholder_firstname;
  std::optional<std::string> placeholder_lastname;
  std::optional<std::string> placeholder_email;
};

class FormAutofillNonEmptyFieldTest
    : public FormAutofillWithConstraintsTest,
      public WithParamInterface<FillFormNonEmptyFieldParams> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormAutofillNonEmptyFieldTest,
    ValuesIn(std::to_array<FillFormNonEmptyFieldParams>({
        FillFormNonEmptyFieldParams{
            .html = R"(<form name=TestForm action='http://abc.com'>
                         <input id=firstname>
                         <input id=lastname>
                         <input id=email>
                         <input type=submit value=Send>
                       </form>)",
            .unowned = false},
        FillFormNonEmptyFieldParams{
            .html = R"(<form name=TestForm action='http://abc.com'>
                         <input id=firstname value='Enter first name'>
                         <input id=lastname value='Enter last name'>
                         <input id=email value='Enter email'>
                         <input type=submit value=Send>
                       </form>)",
            .unowned = false,
            .initial_lastname = "Enter last name",
            .initial_email = "Enter email"},
        FillFormNonEmptyFieldParams{
            .html = R"(<form name=TestForm action='http://abc.com' method=POST>
                         <input id=firstname
                                placeholder='First Name'
                                value='First Name'>
                         <input id=lastname
                                placeholder='Last Name'
                                value='Last Name'>
                         <input id=email placeholder=Email value=Email>
                         <input type=submit value=Send>
                       </form>)",
            .unowned = false,
            .placeholder_firstname = "First Name",
            .placeholder_lastname = "Last Name",
            .placeholder_email = "Email"},
        FillFormNonEmptyFieldParams{
            .html = R"(<head><title>delivery recipient info</title></head>
                       <input id=firstname>
                       <input id=lastname>
                       <input id=email>
                       <input type=submit value=Send>)",
            .unowned = true},
    })),
    [](const TestParamInfo<FormAutofillNonEmptyFieldTest::ParamType>&
           param_info) {
      constexpr auto kNames = std::to_array<std::string_view>(
          {"UserInput", "WithDefaultValues", "WithPlaceholderValues",
           "UnownedForm"});
      return std::string{kNames[param_info.index]};
    });

TEST_P(FormAutofillNonEmptyFieldTest, FillFormNonEmptyField) {
  LoadHTML(GetParam().html);

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebInputElement input_element = GetInputElementById("firstname");

  // Simulate typing by modifying the field value.
  constexpr std::string_view kNewFirstnameValue = "Wy";
  input_element.SetValue(WebString::FromAscii(kNewFirstnameValue));

  // Find the form that contains the input element.
  FormData form = FindForm(input_element);
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form.name());
    EXPECT_EQ(GURL("http://abc.com"), form.action());
  }

  const std::u16string firstname_label =
      UTF8ToUTF16(GetParam().placeholder_firstname.value_or(""));
  const std::u16string firstname_placeholder =
      UTF8ToUTF16(GetParam().placeholder_firstname.value_or(""));
  const std::u16string lastname_label =
      UTF8ToUTF16(GetParam().initial_lastname.value_or(
          GetParam().placeholder_lastname.value_or("")));
  const std::u16string lastname_placeholder =
      UTF8ToUTF16(GetParam().placeholder_lastname.value_or(""));
  const std::u16string email_label =
      UTF8ToUTF16(GetParam().initial_email.value_or(
          GetParam().placeholder_email.value_or("")));
  const std::u16string email_placeholder =
      UTF8ToUTF16(GetParam().placeholder_email.value_or(""));
  EXPECT_THAT(
      form.fields(),
      ElementsAre(
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
               .placeholder =
                   GetParam().initial_lastname ? u"" : lastname_placeholder,
               .is_autofilled_according_to_renderer = false}),
          test::FormFieldDescriptionEq(
              {.label = email_label,
               .name = u"email",
               .id_attribute = u"email",
               .value = email_label,
               .placeholder =
                   GetParam().initial_email ? u"" : email_placeholder,
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
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form2.name());
    EXPECT_EQ(GURL("http://abc.com"), form2.action());
  }

  EXPECT_THAT(form2.fields(),
              ElementsAre(test::FormFieldDescriptionEq(
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

class FormAutofillMaxLengthTest
    : public FormAutofillWithConstraintsTest,
      public WithParamInterface<FormAutofillTestParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormAutofillMaxLengthTest,
    ValuesIn(std::to_array<FormAutofillTestParam>(
        {{.html = R"(<form name=TestForm action='http://abc.com'>
                         <input id=firstname maxlength=5>
                         <input id=lastname maxlength=7>
                         <input id=email maxlength=9>
                         <input type=submit value=Send>
                       </form>)",
          .unowned = false},
         {.html = R"(<head><title>delivery recipient info</title></head>
                       <input id=firstname maxlength=5>
                       <input id=lastname maxlength=7>
                       <input id=email maxlength=9>
                       <input type=submit value=Send>)",
          .unowned = true}})),
    [](const TestParamInfo<FormAutofillMaxLengthTest::ParamType>& param_info) {
      return param_info.param.unowned ? "Unowned" : "Owned";
    });

TEST_P(FormAutofillMaxLengthTest, FillFormMaxLength) {
  LoadHTML(GetParam().html);

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebInputElement input_element = GetInputElementById("firstname");

  // Find the form that contains the input element.
  FormData form = FindForm(input_element);
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form.name());
    EXPECT_EQ(GURL("http://abc.com"), form.action());
  }

  EXPECT_THAT(form.fields(),
              ElementsAre(test::FormFieldDescriptionEq(
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
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form2.name());
    EXPECT_EQ(GURL("http://abc.com"), form2.action());
  }

  EXPECT_THAT(form2.fields(),
              ElementsAre(test::FormFieldDescriptionEq(
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

class FormAutofillNegativeMaxLengthTest
    : public FormAutofillWithConstraintsTest,
      public WithParamInterface<FormAutofillTestParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormAutofillNegativeMaxLengthTest,
    ValuesIn(std::to_array<FormAutofillTestParam>(
        {{.html = R"(<form name=TestForm action='http://abc.com'>
                         <input id=firstname maxlength='-1'>
                         <input id=lastname maxlength='-2'>
                         <input id=email maxlength='-3'>
                         <input type=submit value=Send>
                       </form>)",
          .unowned = false},
         {.html = R"(<head><title>delivery recipient info</title></head>
                       <input id=firstname maxlength='-1'>
                       <input id=lastname maxlength='-2'>
                       <input id=email maxlength='-3'>
                       <input type=submit value=Send>)",
          .unowned = true}})),
    [](const TestParamInfo<FormAutofillNegativeMaxLengthTest::ParamType>&
           param_info) {
      return param_info.param.unowned ? "Unowned" : "Owned";
    });

// This test uses negative values of the maxlength attribute for input elements.
// In this case, the maxlength of the input elements is set to the default
// maxlength (defined in WebKit.)
TEST_P(FormAutofillNegativeMaxLengthTest, FillFormNegativeMaxLength) {
  LoadHTML(GetParam().html);

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebInputElement input_element = GetInputElementById("firstname");

  // Find the form that contains the input element.
  FormData form = FindForm(input_element);
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form.name());
    EXPECT_EQ(GURL("http://abc.com"), form.action());
  }

  EXPECT_THAT(form.fields(),
              ElementsAreArray(FirstLastEmailIdFieldsMatchers()));

  // Fill the form.
  test_api(form).field(0).set_value(u"Brother");
  test_api(form).field(1).set_value(u"Jonathan");
  test_api(form).field(2).set_value(u"brotherj@example.com");
  ExecuteJavaScriptForTests("document.getElementById('firstname').focus();");
  ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                    mojom::ActionPersistence::kFill);

  // Find the newly-filled form that contains the input element.
  FormData form2 = FindForm(input_element);
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form2.name());
    EXPECT_EQ(GURL("http://abc.com"), form2.action());
  }

  EXPECT_THAT(
      form2.fields(),
      ElementsAre(
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

class FormAutofillEmptyNameTest
    : public FormAutofillWithConstraintsTest,
      public WithParamInterface<FormAutofillTestParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormAutofillEmptyNameTest,
    ValuesIn(std::to_array<FormAutofillTestParam>(
        {{.html = R"(<form name=TestForm action='http://abc.com'>
                         <input id=firstname name=''>
                         <input id=lastname name=''>
                         <input id=email name=''>
                         <input type=submit value=Send>
                       </form>)",
          .unowned = false},
         {.html = R"(<head><title>delivery recipient info</title></head>
                       <input id=firstname name=''>
                       <input id=lastname name=''>
                       <input id=email name=''>
                       <input type=submit value=Send>)",
          .unowned = true}})),
    [](const TestParamInfo<FormAutofillEmptyNameTest::ParamType>& param_info) {
      return param_info.param.unowned ? "Unowned" : "Owned";
    });

TEST_P(FormAutofillEmptyNameTest, FillFormEmptyName) {
  LoadHTML(GetParam().html);

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  ASSERT_EQ(1U, forms.size());

  // Get the input element we want to find.
  WebInputElement input_element = GetInputElementById("firstname");

  // Find the form that contains the input element.
  FormData form = FindForm(input_element);
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form.name());
    EXPECT_EQ(GURL("http://abc.com"), form.action());
  }

  EXPECT_THAT(form.fields(),
              ElementsAreArray(FirstLastEmailIdFieldsMatchers()));

  // Fill the form.
  test_api(form).field(0).set_value(u"Wyatt");
  test_api(form).field(1).set_value(u"Earp");
  test_api(form).field(2).set_value(u"wyatt@example.com");
  ExecuteJavaScriptForTests("document.getElementById('firstname').focus();");
  ApplyFieldsAction(input_element.GetDocument(), form.fields(),
                    mojom::ActionPersistence::kFill);

  // Find the newly-filled form that contains the input element.
  FormData form2 = FindForm(input_element);
  if (!GetParam().unowned) {
    EXPECT_EQ(u"TestForm", form2.name());
    EXPECT_EQ(GURL("http://abc.com"), form2.action());
  }

  EXPECT_THAT(
      form2.fields(),
      ElementsAre(
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

class FormAutofillEmptyFormNamesTest
    : public FormAutofillWithConstraintsTest,
      public WithParamInterface<FormAutofillTestParam> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    FormAutofillEmptyFormNamesTest,
    ValuesIn(std::to_array<FormAutofillTestParam>({
        {.html = R"(<form action='http://abc.com'>
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
         .unowned = false},
        {.html = R"(<head><title>enter delivery preferences</title></head>
                    <input id=firstname>
                    <input id=middlename>
                    <input id=lastname>
                    <input id=apple>
                    <input id=banana>
                    <input id=cantelope>
                    <input type=submit value=Send>)",
         .unowned = true},
    })),
    [](const TestParamInfo<FormAutofillEmptyFormNamesTest::ParamType>&
           param_info) {
      return param_info.param.unowned ? "Unowned" : "Owned";
    });

TEST_P(FormAutofillEmptyFormNamesTest, FillFormEmptyFormNames) {
  LoadHTML(GetParam().html);

  std::vector<FormData> forms = UpdateFormCache().updated_forms;
  const size_t expected_size = GetParam().unowned ? 1 : 2;
  ASSERT_EQ(expected_size, forms.size());

  // Get the input element we want to find.
  WebInputElement input_element = GetInputElementById("apple");

  // Find the form that contains the input element.
  FormData form = FindForm(input_element);
  if (!GetParam().unowned) {
    EXPECT_TRUE(form.name().empty());
    EXPECT_EQ(GURL("http://abc.com"), form.action());
  }

  const size_t unowned_offset = GetParam().unowned ? 3 : 0;
  ASSERT_EQ(unowned_offset + 3, form.fields().size());
  EXPECT_THAT(base::span(form.fields()).subspan(unowned_offset, 3U),
              ElementsAre(test::FormFieldDescriptionEq(
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
  if (!GetParam().unowned) {
    EXPECT_TRUE(form2.name().empty());
    EXPECT_EQ(GURL("http://abc.com"), form2.action());
  }

  ASSERT_EQ(unowned_offset + 3, form2.fields().size());
  EXPECT_THAT(base::span(form2.fields()).subspan(unowned_offset, 3U),
              ElementsAre(test::FormFieldDescriptionEq(
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

class AutofillFocusTest : public test::AutofillRendererTest {
 protected:
  void SetUp() override {
    test::AutofillRendererTest::SetUp();

    focus_test_utils_ = std::make_unique<test::FocusTestUtils>(
        base::BindRepeating(&AutofillFocusTest::ExecuteJavaScriptForTests,
                            base::Unretained(this)));
  }

  // See `AutofillRendererTest::SimulateFillForm`. Extracts `FormData` from main
  // frame if none is provided.
  AssertionResult SimulateFillForm(
      std::optional<FormData> form_data = std::nullopt) {
    if (!form_data) {
      form_data = ExtractFormData("myForm");
    }
    if (!form_data) {
      return AssertionFailure();
    }
    return AutofillRendererTest::SimulateFillForm(
        *form_data, "fname", {{u"fname", u"John"}, {u"lname", u"Smith"}});
  }

  std::string GetFocusLog() {
    return focus_test_utils_->GetFocusLog(GetMainFrame()->GetDocument());
  }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  std::unique_ptr<test::FocusTestUtils> focus_test_utils_;
};

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element in a form
// having non-fillable fields.
TEST_F(AutofillFocusTest, VerifyFocusAndBlurEventsAfterAutofill) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "<label>Middle Name:</label><input id='mname' name='2'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  ASSERT_TRUE(SimulateFillForm());

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element.
TEST_F(AutofillFocusTest,
       VerifyFocusAndBlurEventsAfterAutofillWithFocusedElement) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  ASSERT_TRUE(SimulateFillForm());

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element in a form having
// single field.
TEST_F(AutofillFocusTest,
       VerifyFocusAndBlurEventAfterAutofillWithFocusedElementForSingleElement) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  ASSERT_TRUE(SimulateFillForm());

  // Expected Result in order:
  // * Change fname
  EXPECT_EQ(GetFocusLog(), "c0");
}

// Tests that a field is added to the form between the times of triggering
// and executing the filling.
TEST_F(AutofillFocusTest, VerifyFocusAndBlurEventAfterElementAdded) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  std::optional<FormData> form = ExtractFormData("myForm");
  ASSERT_TRUE(form);
  // Simulate that the form was modified between parsing and executing the fill.
  // The element is inserted at the beginning of the form to verify that
  // everything works correctly even if `renderer_id`s of the `<input>`
  // elements are not in ascending order.
  ExecuteJavaScriptForTests(
      "document.getElementById('fname').insertAdjacentHTML('beforebegin', "
      "'<label>Zip code:</label><input id=\"zip_code\"/>');");
  ASSERT_TRUE(SimulateFillForm(form));

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that a field is removed from the form between the times of
// triggering and executing the filling.
TEST_F(AutofillFocusTest, VerifyFocusAndBlurEventAfterElementRemoved) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  std::optional<FormData> form = ExtractFormData("myForm");
  ASSERT_TRUE(form);

  ExecuteJavaScriptForTests("document.getElementById('lname').remove()");
  ASSERT_TRUE(SimulateFillForm(form));

  // Expected Result in order:
  // * Change fname
  EXPECT_EQ(GetFocusLog(), "c0");
}

}  // namespace
}  // namespace autofill::form_util
