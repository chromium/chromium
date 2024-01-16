// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::SizeIs;

class MockFormTracker : public FormTracker {
 public:
  using FormTracker::FormTracker;
  MOCK_METHOD(void,
              ElementDisappeared,
              (const blink::WebElement& element),
              (override));
};

// Matches a specific FormRendererId.
auto IsFormId(absl::variant<FormRendererId, size_t> expectation) {
  FormRendererId id = absl::holds_alternative<FormRendererId>(expectation)
                          ? absl::get<FormRendererId>(expectation)
                          : FormRendererId(absl::get<size_t>(expectation));
  return Eq(id);
}

// Matches a FormData with a specific FormData::unique_renderer_id.
auto HasFormId(absl::variant<FormRendererId, size_t> expectation) {
  return Field(&FormData::unique_renderer_id, IsFormId(expectation));
}

// Matches a FormData with |num| FormData::fields.
auto HasNumFields(size_t num) {
  return Field(&FormData::fields, SizeIs(num));
}

// Matches a FormData with |num| FormData::child_frames.
auto HasNumChildFrames(size_t num) {
  return Field(&FormData::child_frames, SizeIs(num));
}

// Matches a container with a single element which (the element) matches all
// |element_matchers|.
auto HasSingleElementWhich(auto... element_matchers) {
  return AllOf(SizeIs(1), ElementsAre(AllOf(element_matchers...)));
}

auto HasType(FormControlType type) {
  return Field(&FormFieldData::form_control_type, type);
}

// TODO(crbug.com/63573): Add many more test cases.
class AutofillAgentTest : public test::AutofillRendererTest {
 public:
  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    test_api(autofill_agent())
        .set_form_tracker(
            std::make_unique<MockFormTracker>(GetMainRenderFrame()));
  }

  MockFormTracker& form_tracker() {
    return static_cast<MockFormTracker&>(
        test_api(autofill_agent()).form_tracker());
  }
};

class AutofillAgentTestWithFeatures : public AutofillAgentTest {
 public:
  AutofillAgentTestWithFeatures() {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kAutofillUseDomNodeIdForRendererId,
         features::kAutofillReplaceCachedWebElementsByRendererIds,
         features::kAutofillDetectRemovedFormControls,
         features::kAutofillContentEditables},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_Empty) {
  EXPECT_CALL(autofill_driver(), FormsSeen).Times(0);
  LoadHTML(R"(<body> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NoEmpty) {
  EXPECT_CALL(autofill_driver(), FormsSeen).Times(0);
  LoadHTML(R"(<body> <form></form> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NewFormUnowned) {
  EXPECT_CALL(autofill_driver(),
              FormsSeen(HasSingleElementWhich(HasFormId(0u), HasNumFields(1),
                                              HasNumChildFrames(0)),
                        SizeIs(0)));
  LoadHTML(R"(<body> <input> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NewForm) {
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(HasNumFields(1), HasNumChildFrames(0)),
                SizeIs(0)));
  LoadHTML(R"(<body> <form><input></form> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NewIframe) {
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(HasNumFields(0), HasNumChildFrames(1)),
                SizeIs(0)));
  LoadHTML(R"(<body> <form><iframe></iframe></form> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_UpdatedForm) {
  {
    EXPECT_CALL(
        autofill_driver(),
        FormsSeen(HasSingleElementWhich(HasNumFields(1), HasNumChildFrames(0)),
                  SizeIs(0)));
    LoadHTML(R"(<body> <form><input></form> </body>)");
    WaitForFormsSeen();
  }
  {
    EXPECT_CALL(
        autofill_driver(),
        FormsSeen(HasSingleElementWhich(HasNumFields(2), HasNumChildFrames(0)),
                  SizeIs(0)));
    ExecuteJavaScriptForTests(
        R"(document.forms[0].appendChild(document.createElement('input'));)");
    WaitForFormsSeen();
  }
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_RemovedInput) {
  {
    EXPECT_CALL(autofill_driver(), FormsSeen(SizeIs(1), SizeIs(0)));
    LoadHTML(R"(<body> <form><input></form> </body>)");
    WaitForFormsSeen();
  }
  {
    EXPECT_CALL(autofill_driver(), FormsSeen(SizeIs(0), SizeIs(1)));
    ExecuteJavaScriptForTests(R"(document.forms[0].elements[0].remove();)");
    WaitForFormsSeen();
  }
}

TEST_F(AutofillAgentTestWithFeatures, TriggerFormExtractionWithResponse) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML(R"(<body> <input> </body>)");
  WaitForFormsSeen();
  base::MockOnceCallback<void(bool)> mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  autofill_agent().TriggerFormExtractionWithResponse(mock_callback.Get());
  task_environment_.FastForwardBy(AutofillAgent::kFormsSeenThrottle / 2);
  EXPECT_CALL(mock_callback, Run(true));
  task_environment_.FastForwardBy(AutofillAgent::kFormsSeenThrottle / 2);
}

class AutofillAgentTestExtractForms : public AutofillAgentTestWithFeatures {
 public:
  using Callback = base::MockCallback<
      base::OnceCallback<void(const std::optional<FormData>&)>>;

  void LoadHTML(const char* html, bool wait_for_forms_seen = true) {
    if (wait_for_forms_seen) {
      EXPECT_CALL(autofill_driver(), FormsSeen);
    }
    AutofillAgentTestWithFeatures::LoadHTML(html);
    WaitForFormsSeen();
  }

  FormRendererId GetFormRendererIdById(std::string_view id) {
    return form_util::GetFormRendererId(
        GetMainFrame()->GetDocument().GetElementById(
            blink::WebString::FromUTF8(id)));
  }
};

TEST_F(AutofillAgentTestExtractForms, CallbackIsCalledIfFormIsNotFound) {
  LoadHTML("<body>", /*wait_for_forms_seen=*/false);
  Callback callback;
  EXPECT_CALL(callback, Run(Eq(std::nullopt)));
  autofill_agent().ExtractForm(GetFormRendererIdById("f"), callback.Get());
}

TEST_F(AutofillAgentTestExtractForms, CallbackIsCalledForForm) {
  const auto is_text_input = HasType(FormControlType::kInputText);
  LoadHTML("<body><form id=f><input><input></form>");
  Callback callback;
  EXPECT_CALL(
      callback,
      Run(Optional(AllOf(
          Field(&FormData::unique_renderer_id, GetFormRendererIdById("f")),
          Field(&FormData::name, u"f"),
          Field(&FormData::fields,
                ElementsAre(is_text_input, is_text_input))))));
  autofill_agent().ExtractForm(GetFormRendererIdById("f"), callback.Get());
}

TEST_F(AutofillAgentTestExtractForms, CallbackIsCalledForFormlessFields) {
  const auto is_text_area = HasType(FormControlType::kTextArea);
  LoadHTML(R"(<body><input><input>)");
  Callback callback;
  EXPECT_CALL(callback, Run(Optional(_)));
  autofill_agent().ExtractForm(GetFormRendererIdById("f"), callback.Get());
}

TEST_F(AutofillAgentTestExtractForms, CallbackIsCalledForContentEditable) {
  const auto is_content_editable = HasType(FormControlType::kContentEditable);
  LoadHTML("<body><div id=ce contenteditable></div>",
           /*wait_for_forms_seen=*/false);
  base::MockCallback<base::OnceCallback<void(const std::optional<FormData>&)>>
      callback;
  EXPECT_CALL(
      callback,
      Run(Optional(AllOf(
          Field(&FormData::unique_renderer_id, GetFormRendererIdById("ce")),
          Field(&FormData::fields, ElementsAre(is_content_editable))))));
  autofill_agent().ExtractForm(GetFormRendererIdById("ce"), callback.Get());
}

TEST_F(AutofillAgentTestWithFeatures,
       TriggerFormExtractionWithResponse_CalledTwice) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML(R"(<body> <input> </body>)");
  WaitForFormsSeen();
  base::MockOnceCallback<void(bool)> mock_callback;
  autofill_agent().TriggerFormExtractionWithResponse(mock_callback.Get());
  EXPECT_CALL(mock_callback, Run(false));
  autofill_agent().TriggerFormExtractionWithResponse(mock_callback.Get());
}

// Tests that `AutofillDriver::TriggerSuggestions()` triggers
// `AutofillAgent::AskForValuesToFill()` (which will ultimately trigger
// suggestions).
TEST_F(AutofillAgentTestWithFeatures, TriggerSuggestions) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML("<body><input></body>");
  WaitForFormsSeen();
  EXPECT_CALL(autofill_driver(), AskForValuesToFill);
  autofill_agent().TriggerSuggestions(
      FieldRendererId(1 +
                      base::FeatureList::IsEnabled(
                          blink::features::kAutofillUseDomNodeIdForRendererId)),
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

TEST_F(AutofillAgentTest, UndoAutofillSetsLastQueriedElement) {
  LoadHTML(R"(
    <form id="form_id">
        <input id="text_id_1">
        <select id="select_id_1">
          <option value="undo_select_option_1">Foo</option>
          <option value="autofill_select_option_1">Bar</option>
        </select>
        <selectlist id="selectlist_id_1">
          <option value="undo_selectlist_option_1">Foo</option>
          <option value="autofill_selectlist_option_1">Bar</option>
        </selectlist>
      </form>
  )");

  blink::WebVector<blink::WebFormElement> forms =
      GetMainFrame()->GetDocument().Forms();
  EXPECT_EQ(1U, forms.size());
  FormData form = *form_util::WebFormElementToFormDataForTesting(
      forms[0], blink::WebFormControlElement(),
      *base::MakeRefCounted<FieldDataManager>(),
      {form_util::ExtractOption::kValue}, nullptr);

  ASSERT_TRUE(autofill_agent().focused_element().IsNull());
  autofill_agent().ApplyFormAction(mojom::ActionType::kUndo,
                                   mojom::ActionPersistence::kFill,
                                   form.unique_renderer_id, form.fields);
  EXPECT_FALSE(autofill_agent().focused_element().IsNull());
}

// Tests that AutofillAgent::ApplyFormAction(kFill, kPreview) and
// AutofillAgent::ClearPreviewedForm correctly set/reset the autofill state of a
// field.
TEST_F(AutofillAgentTest, PreviewThenClear) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="text_id">
    </form>
  )");

  blink::WebVector<blink::WebFormElement> forms =
      GetMainFrame()->GetDocument().Forms();
  ASSERT_EQ(1U, forms.size());
  FormData form = *form_util::WebFormElementToFormDataForTesting(
      forms[0], blink::WebFormControlElement(),
      *base::MakeRefCounted<FieldDataManager>(),
      {form_util::ExtractOption::kValue}, nullptr);
  ASSERT_EQ(form.fields.size(), 1u);
  blink::WebFormControlElement field =
      GetMainFrame()
          ->GetDocument()
          .GetElementById("text_id")
          .DynamicTo<blink::WebFormControlElement>();
  ASSERT_FALSE(field.IsNull());

  std::u16string prior_value = form.fields[0].value;
  form.fields[0].value += u"AUTOFILLED";
  form.fields[0].is_autofilled = true;

  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
  autofill_agent().ApplyFormAction(mojom::ActionType::kFill,
                                   mojom::ActionPersistence::kPreview,
                                   form.unique_renderer_id, form.fields);
  EXPECT_EQ(field.GetAutofillState(), blink::WebAutofillState::kPreviewed);
  autofill_agent().ClearPreviewedForm();
  EXPECT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
}

TEST_F(AutofillAgentTest, HideElementTriggersFormTracker_DisplayNone) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="field_id">
    </form>
  )");
  blink::WebElement element =
      GetElementById(GetMainFrame()->GetDocument(), "field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].style.display = 'none';)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

TEST_F(AutofillAgentTest, HideElementTriggersFormTracker_VisibilityHidden) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="field_id">
    </form>
  )");
  blink::WebElement element =
      GetElementById(GetMainFrame()->GetDocument(), "field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].style.visibility = 'hidden';)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

TEST_F(AutofillAgentTest, HideElementTriggersFormTracker_TypeHidden) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="field_id">
    </form>
  )");
  blink::WebElement element =
      GetElementById(GetMainFrame()->GetDocument(), "field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].setAttribute('type', 'hidden');)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

TEST_F(AutofillAgentTest, HideElementTriggersFormTracker_HiddenTrue) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="field_id">
    </form>
  )");
  blink::WebElement element =
      GetElementById(GetMainFrame()->GetDocument(), "field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].setAttribute('hidden', 'true');)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

TEST_F(AutofillAgentTest, HideElementTriggersFormTracker_ShadowDom) {
  LoadHTML(R"(
   <form id="form_id">
    <div>
      <template shadowrootmode="open">
        <slot></slot>
      </template>
      <input id="field_id">
    </div>
  </form>
  )");
  blink::WebElement element =
      GetElementById(GetMainFrame()->GetDocument(), "field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(R"(field_id.slot = "unknown";)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

}  // namespace

}  // namespace autofill
