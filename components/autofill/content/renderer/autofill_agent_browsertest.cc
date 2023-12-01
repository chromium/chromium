// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::SizeIs;

namespace autofill {

namespace {

// The throttling amount of ProcessForms().
constexpr base::TimeDelta kFormsSeenThrottle = base::Milliseconds(100);

class MockFormTracker : public FormTracker {
 public:
  using FormTracker::FormTracker;
  MOCK_METHOD(void,
              ElementDisappeared,
              (const blink::WebElement& element),
              (override));
};

class MockAutofillDriver : public mojom::AutofillDriver {
 public:
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this, mojo::PendingAssociatedReceiver<mojom::AutofillDriver>(
                             std::move(handle)));
  }

  MOCK_METHOD(void,
              SetFormToBeProbablySubmitted,
              (const absl::optional<FormData>& form),
              (override));
  MOCK_METHOD(void,
              FormsSeen,
              (const std::vector<FormData>& updated_forms,
               const std::vector<FormRendererId>& removed_forms),
              (override));
  MOCK_METHOD(void,
              FormSubmitted,
              (const FormData& form,
               bool known_success,
               mojom::SubmissionSource source),
              (override));
  MOCK_METHOD(void,
              TextFieldDidChange,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box,
               base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void,
              TextFieldDidScroll,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              SelectControlDidChange,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              SelectOrSelectListFieldOptionsDidChange,
              (const FormData& form),
              (override));
  MOCK_METHOD(void,
              JavaScriptChangedAutofilledValue,
              (const FormData& form,
               const FormFieldData& field,
               const std::u16string& old_value),
              (override));
  MOCK_METHOD(void,
              AskForValuesToFill,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box,
               AutofillSuggestionTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void, HidePopup, (), (override));
  MOCK_METHOD(void,
              FocusNoLongerOnForm,
              (bool had_interacted_form),
              (override));
  MOCK_METHOD(void,
              FocusOnFormField,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              DidFillAutofillFormData,
              (const FormData& form, base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void, DidEndTextFieldEditing, (), (override));

 private:
  mojo::AssociatedReceiverSet<mojom::AutofillDriver> receivers_;
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
template <typename... Matchers>
auto HasSingleElementWhich(Matchers... element_matchers) {
  return AllOf(SizeIs(1), ElementsAre(AllOf(element_matchers...)));
}

// TODO(crbug.com/63573): Add many more test cases.
class AutofillAgentTest : public content::RenderViewTest {
 public:
  void SetUp() override {
    RenderViewTest::SetUp();

    blink::AssociatedInterfaceProvider* remote_interfaces =
        GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::AutofillDriver::Name_,
        base::BindRepeating(&MockAutofillDriver::BindPendingReceiver,
                            base::Unretained(&autofill_driver_)));

    auto password_autofill_agent = std::make_unique<TestPasswordAutofillAgent>(
        GetMainRenderFrame(), &associated_interfaces_);
    auto password_generation = std::make_unique<PasswordGenerationAgent>(
        GetMainRenderFrame(), password_autofill_agent.get(),
        &associated_interfaces_);
    autofill_agent_ = std::make_unique<AutofillAgent>(
        GetMainRenderFrame(), std::move(password_autofill_agent),
        std::move(password_generation), &associated_interfaces_);
    autofill_agent_->set_form_tracker_for_testing(
        std::make_unique<MockFormTracker>(GetMainRenderFrame()));
  }

  void TearDown() override {
    autofill_agent_.reset();
    RenderViewTest::TearDown();
  }

  MockFormTracker& form_tracker() {
    return *static_cast<MockFormTracker*>(
        autofill_agent_->form_tracker_for_testing());
  }

  // AutofillDriver::FormsSeen() is throttled indirectly because some callsites
  // of AutofillAgent::ProcessForms() are throttled. This function blocks until
  // FormsSeen() has happened.
  void WaitForFormsSeen() {
    task_environment_.FastForwardBy(kFormsSeenThrottle * 3 / 2);
  }

 protected:
  MockAutofillDriver autofill_driver_;
  std::unique_ptr<AutofillAgent> autofill_agent_;

 private:
  blink::AssociatedInterfaceRegistry associated_interfaces_;
};

class AutofillAgentTestWithFeatures : public AutofillAgentTest {
 public:
  AutofillAgentTestWithFeatures() {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kAutofillUseDomNodeIdForRendererId,
         features::kAutofillDetectRemovedFormControls,
         features::kAutofillContentEditables},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_Empty) {
  EXPECT_CALL(autofill_driver_, FormsSeen).Times(0);
  LoadHTML(R"(<body> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NoEmpty) {
  EXPECT_CALL(autofill_driver_, FormsSeen).Times(0);
  LoadHTML(R"(<body> <form></form> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NewFormUnowned) {
  EXPECT_CALL(autofill_driver_,
              FormsSeen(HasSingleElementWhich(HasFormId(0u), HasNumFields(1),
                                              HasNumChildFrames(0)),
                        SizeIs(0)));
  LoadHTML(R"(<body> <input> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NewForm) {
  EXPECT_CALL(
      autofill_driver_,
      FormsSeen(HasSingleElementWhich(HasNumFields(1), HasNumChildFrames(0)),
                SizeIs(0)));
  LoadHTML(R"(<body> <form><input></form> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_NewIframe) {
  EXPECT_CALL(
      autofill_driver_,
      FormsSeen(HasSingleElementWhich(HasNumFields(0), HasNumChildFrames(1)),
                SizeIs(0)));
  LoadHTML(R"(<body> <form><iframe></iframe></form> </body>)");
  WaitForFormsSeen();
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_UpdatedForm) {
  {
    EXPECT_CALL(
        autofill_driver_,
        FormsSeen(HasSingleElementWhich(HasNumFields(1), HasNumChildFrames(0)),
                  SizeIs(0)));
    LoadHTML(R"(<body> <form><input></form> </body>)");
    WaitForFormsSeen();
  }
  {
    EXPECT_CALL(
        autofill_driver_,
        FormsSeen(HasSingleElementWhich(HasNumFields(2), HasNumChildFrames(0)),
                  SizeIs(0)));
    ExecuteJavaScriptForTests(
        R"(document.forms[0].appendChild(document.createElement('input'));)");
    WaitForFormsSeen();
  }
}

TEST_F(AutofillAgentTestWithFeatures, FormsSeen_RemovedInput) {
  {
    EXPECT_CALL(autofill_driver_, FormsSeen(SizeIs(1), SizeIs(0)));
    LoadHTML(R"(<body> <form><input></form> </body>)");
    WaitForFormsSeen();
  }
  {
    EXPECT_CALL(autofill_driver_, FormsSeen(SizeIs(0), SizeIs(1)));
    ExecuteJavaScriptForTests(R"(document.forms[0].elements[0].remove();)");
    WaitForFormsSeen();
  }
}

TEST_F(AutofillAgentTestWithFeatures, TriggerFormExtractionWithResponse) {
  EXPECT_CALL(autofill_driver_, FormsSeen);
  LoadHTML(R"(<body> <input> </body>)");
  WaitForFormsSeen();
  base::MockOnceCallback<void(bool)> mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);
  autofill_agent_->TriggerFormExtractionWithResponse(mock_callback.Get());
  task_environment_.FastForwardBy(kFormsSeenThrottle / 2);
  EXPECT_CALL(mock_callback, Run(true));
  task_environment_.FastForwardBy(kFormsSeenThrottle / 2);
}

auto HasType(FormControlType type) {
  return Field(&FormFieldData::form_control_type, type);
}

TEST_F(AutofillAgentTestWithFeatures,
       FocusOnContentEditableTriggersAskForValuesToFill) {
  const auto is_content_editable = HasType(FormControlType::kContentEditable);
  LoadHTML("<body><div id=ce contenteditable></body>");
  WaitForFormsSeen();
  EXPECT_CALL(
      autofill_driver_,
      AskForValuesToFill(
          Field(&FormData::fields, ElementsAre(is_content_editable)),
          is_content_editable, _,
          mojom::AutofillSuggestionTriggerSource::kContentEditableClicked))
#if BUILDFLAG(IS_ANDROID)
      // TODO(crbug.com/1490581): Android calls HandleFocusChangeComplete()
      // twice, once from FocusedElementChanged() and once from
      // DidReceiveLeftMouseDownOrGestureTapInNode().
      .Times(2)
#endif
      ;
  SimulateElementClick("ce");
}

TEST_F(AutofillAgentTestWithFeatures, FocusOnContentEditableFormIsIgnored) {
  LoadHTML("<body><form id=ce contenteditable></form>");
  WaitForFormsSeen();
  EXPECT_CALL(autofill_driver_, AskForValuesToFill).Times(0);
  SimulateElementClick("ce");
}

TEST_F(AutofillAgentTestWithFeatures,
       FocusOnContentEditableFormControlIsIgnored) {
  EXPECT_CALL(autofill_driver_, FormsSeen);
  LoadHTML("<body><textarea id=ce contenteditable></textarea>");
  WaitForFormsSeen();
  EXPECT_CALL(autofill_driver_, AskForValuesToFill)
#if BUILDFLAG(IS_ANDROID)
      // TODO(crbug.com/1490581): Android calls HandleFocusChangeComplete()
      // twice, once from FocusedElementChanged() and once from
      // DidReceiveLeftMouseDownOrGestureTapInNode().
      .Times(2)
#endif
      ;
  EXPECT_CALL(
      autofill_driver_,
      AskForValuesToFill(
          _, _, _,
          mojom::AutofillSuggestionTriggerSource::kContentEditableClicked))
      .Times(0);
  SimulateElementClick("ce");
}

class AutofillAgentTestExtractForms : public AutofillAgentTestWithFeatures {
 public:
  using Callback = base::MockCallback<
      base::OnceCallback<void(const std::optional<FormData>&)>>;

  void LoadHTML(const char* html, bool wait_for_forms_seen = true) {
    if (wait_for_forms_seen) {
      EXPECT_CALL(autofill_driver_, FormsSeen);
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
  autofill_agent_->ExtractForm(GetFormRendererIdById("f"), callback.Get());
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
  autofill_agent_->ExtractForm(GetFormRendererIdById("f"), callback.Get());
}

TEST_F(AutofillAgentTestExtractForms, CallbackIsCalledForFormlessFields) {
  const auto is_text_area = HasType(FormControlType::kTextArea);
  LoadHTML(R"(<body><input><input>)");
  Callback callback;
  EXPECT_CALL(callback, Run(Optional(_)));
  autofill_agent_->ExtractForm(GetFormRendererIdById("f"), callback.Get());
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
  autofill_agent_->ExtractForm(GetFormRendererIdById("ce"), callback.Get());
}

TEST_F(AutofillAgentTestWithFeatures,
       TriggerFormExtractionWithResponse_CalledTwice) {
  EXPECT_CALL(autofill_driver_, FormsSeen);
  LoadHTML(R"(<body> <input> </body>)");
  WaitForFormsSeen();
  base::MockOnceCallback<void(bool)> mock_callback;
  autofill_agent_->TriggerFormExtractionWithResponse(mock_callback.Get());
  EXPECT_CALL(mock_callback, Run(false));
  autofill_agent_->TriggerFormExtractionWithResponse(mock_callback.Get());
}

// Tests that `AutofillDriver::TriggerSuggestions()` triggers
// `AutofillAgent::AskForValuesToFill()` (which will ultimately trigger
// suggestions).
TEST_F(AutofillAgentTestWithFeatures, TriggerSuggestions) {
  EXPECT_CALL(autofill_driver_, FormsSeen);
  LoadHTML("<body><input></body>");
  WaitForFormsSeen();
  EXPECT_CALL(autofill_driver_, AskForValuesToFill);
  autofill_agent_->TriggerSuggestions(
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
  FormData form;
  EXPECT_TRUE(form_util::WebFormElementToFormData(
      forms[0], blink::WebFormControlElement(),
      *base::MakeRefCounted<FieldDataManager>(),
      {form_util::ExtractOption::kValue}, &form, nullptr));

  ASSERT_TRUE(autofill_agent_->focused_element().IsNull());
  autofill_agent_->ApplyFormAction(mojom::ActionType::kUndo,
                                   mojom::ActionPersistence::kFill, form);
  EXPECT_FALSE(autofill_agent_->focused_element().IsNull());
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
