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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::SaveArg;
using ::testing::SizeIs;

constexpr CallTimerState kCallTimerStateDummy = {
    .call_site = CallTimerState::CallSite::kUpdateFormCache,
    .last_autofill_agent_reset = {},
    .last_dom_content_loaded = {},
};

class MockAutofillAgent : public AutofillAgent {
 public:
  using AutofillAgent::AutofillAgent;
  MOCK_METHOD(void, DidDispatchDOMContentLoadedEvent, (), (override));

  void OverriddenDidDispatchDOMContentLoadedEvent() {
    AutofillAgent::DidDispatchDOMContentLoadedEvent();
  }
};

class MockFormTracker : public FormTracker {
 public:
  using FormTracker::FormTracker;
  MOCK_METHOD(void,
              ElementDisappeared,
              (const blink::WebElement& element),
              (override));
};

template <typename... Args>
auto FieldsAre(Args&&... matchers) {
  return Property("FormData::fields", &FormData::fields,
                  ElementsAre(std::forward<Args>(matchers)...));
}

// Matches a `FormData` whose `FormData::fields`' `FormFieldData::id_attribute`
// match `id_attributes`.
template <typename... Args>
auto HasFieldsWithIdAttributes(Args&&... id_attributes) {
  return FieldsAre(Property("FormFieldData::id_attribute",
                            &FormFieldData::id_attribute,
                            std::u16string(id_attributes))...);
}

// Matches a `FormData` with a specific `FormData::renderer_id`.
auto HasFormId(FormRendererId expectation) {
  return Property("FormData::renderer_id", &FormData::renderer_id, expectation);
}

// Matches a `FormData` with a specific `FormData::id_attribute`.
auto HasFormIdAttribute(std::u16string id_attribute) {
  return Property("FormData::id_attribute", &FormData::id_attribute,
                  std::move(id_attribute));
}

auto HasFieldIdAttribute(std::u16string id_attribute) {
  return Property("FormFieldData::id_attribute", &FormFieldData::id_attribute,
                  std::move(id_attribute));
}

auto HasSelectedText(std::u16string selected_text) {
  return Property("FormFieldData::selected_text", &FormFieldData::selected_text,
                  selected_text);
}

auto HasValue(std::u16string value) {
  return Property("FormFieldData::value", &FormFieldData::value,
                  std::move(value));
}

// Matches a FormData with |num| FormData::fields.
auto HasNumFields(size_t num) {
  return Property("FormData::fields", &FormData::fields, SizeIs(num));
}

// Matches a FormData with |num| FormData::child_frames.
auto HasNumChildFrames(size_t num) {
  return Property("FormData::child_frames", &FormData::child_frames,
                  SizeIs(num));
}

// Matches a container with a single element which (the element) matches all
// |element_matchers|.
auto HasSingleElementWhich(auto... element_matchers) {
  return AllOf(SizeIs(1), ElementsAre(AllOf(element_matchers...)));
}

auto HasType(FormControlType type) {
  return Property(&FormFieldData::form_control_type, type);
}

// TODO(crbug.com/41268731): Add many more test cases.
class AutofillAgentTest : public test::AutofillRendererTest {
 public:
  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    std::unique_ptr<MockFormTracker> form_tracker =
        std::make_unique<MockFormTracker>(
            GetMainRenderFrame(), FormTracker::UserGestureRequired(true),
            autofill_agent());
    form_tracker->AddObserver(&autofill_agent());
    test_api(autofill_agent()).set_form_tracker(std::move(form_tracker));
  }

  FormRendererId GetFormRendererIdById(std::string_view id) {
    return form_util::GetFormRendererId(GetWebElementById(id));
  }

  FieldRendererId GetFieldRendererIdById(std::string_view id) {
    return form_util::GetFieldRendererId(GetWebElementById(id));
  }

  void Focus(const char* id) {
    ExecuteJavaScriptForTests(base::StringPrintf(R"(
      document.getElementById('%s').focus();
    )",
                                                 id));
    task_environment_.FastForwardBy(base::Milliseconds(500));
    task_environment_.RunUntilIdle();
  }

  void Click(std::string_view target) {
    SimulatePointClick(
        GetWebElementById(target).BoundsInWidget().CenterPoint());
    task_environment_.RunUntilIdle();
  }

  void RightClick(std::string_view target) {
    SimulatePointRightClick(
        GetWebElementById(target).BoundsInWidget().CenterPoint());
    task_environment_.RunUntilIdle();
  }

  MockFormTracker& form_tracker() {
    return static_cast<MockFormTracker&>(
        test_api(autofill_agent()).form_tracker());
  }

  std::vector<FormFieldData::FillData> GetFieldsForFilling(
      const std::vector<FormData>& forms) {
    std::vector<FormFieldData::FillData> fields;
    for (const FormData& form : forms) {
      for (const FormFieldData& field : form.fields()) {
        fields.emplace_back(field);
      }
    }
    return fields;
  }
};

class AutofillAgentTestWithFeatures : public AutofillAgentTest {
 public:
  AutofillAgentTestWithFeatures() {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillReplaceCachedWebElementsByRendererIds},
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
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(HasFormId(FormRendererId(0)),
                                      HasNumFields(1), HasNumChildFrames(0)),
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

// Tests that when AutofillDetectRemovedFormControls is enabled, Autofill is
// directly notified of removed form elements.
TEST_F(AutofillAgentTestWithFeatures, FormsSeen_RemovedInput) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillDetectRemovedFormControls};
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

using AutofillAgentShadowDomTest = AutofillAgentTestWithFeatures;

// Tests that unassociated form control elements in a Shadow DOM tree that do
// not have a form ancestor are extracted correctly.
TEST_F(AutofillAgentShadowDomTest, UnownedUnassociatedElements) {
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(HasFieldsWithIdAttributes(u"t1", u"t2")),
                IsEmpty()));
  LoadHTML(R"(<body>
    <div>
      <template shadowrootmode="open">
        <input type="text" id="t1">
      </template>
    </div>
    <input type="text" id="t2">
    </body>)");
  WaitForFormsSeen();
}

// Tests that unassociated form control elements whose closest shadow-tree
// including form ancestor is not in a shadow tree are extracted correctly.
TEST_F(AutofillAgentShadowDomTest, UnassociatedElementsOwnedByNonShadowForm) {
  EXPECT_CALL(autofill_driver(),
              FormsSeen(HasSingleElementWhich(HasFormIdAttribute(u"f1"),
                                              HasFieldsWithIdAttributes(
                                                  u"t1", u"t2", u"t3", u"t4")),
                        IsEmpty()));
  LoadHTML(
      R"(<body><form id="f1">
          <div>
            <template shadowrootmode="open">
              <input type="text" id="t1">
              <input type="text" id="t2">
            </template>
          </div>
          <div>
            <template shadowrootmode="open">
              <input type="text" id="t3">
            </template>
          </div>
          <input type="text" id="t4">
       </form></body>)");
  WaitForFormsSeen();
}

// Tests that form control elements that are placed into a slot that is a child
// of a form inside a shadow DOM are not considered to be owned by the form
// inside the shadow DOM, but are considered to be unowned. This is consistent
// with how the DOM handles these form control elements - the "elements" of the
// form "ft" are considered to be empty.
TEST_F(AutofillAgentShadowDomTest, FormControlInsideSlotWithinFormInShadowDom) {
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(HasFormIdAttribute(u""),
                                      HasFieldsWithIdAttributes(u"t1", u"t2")),
                IsEmpty()));
  LoadHTML(
      R"(<body>
        <div>
          <template shadowrootmode=open>
            <form id=ft>
              <slot></slot>
            </form>
          </template>
          <input id=t1>
          <input id=t2>
        </div>
      </body>)");
  WaitForFormsSeen();
}

// Tests that a form that is inside a shadow tree and does not have a
// shadow-tree-including form ancestor is extracted correctly.
TEST_F(AutofillAgentShadowDomTest, ElementsOwnedByFormInShadowTree) {
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(HasFormIdAttribute(u"f1"),
                                      HasFieldsWithIdAttributes(u"t1", u"t2")),
                IsEmpty()));
  LoadHTML(R"(<body>
    <div>
      <template shadowrootmode="open">
        <form id="f1">
          <input type="text" id="t1">
          <input type="text" id="t2">
        </form>
      </template>
    </div></body>)");
  WaitForFormsSeen();
}

// Tests that a form whose shadow-tree including descendants include another
// form element, is extracted correctly.
TEST_F(AutofillAgentShadowDomTest, NestedForms) {
  EXPECT_CALL(autofill_driver(),
              FormsSeen(HasSingleElementWhich(
                            HasFormIdAttribute(u"f1"),
                            HasFieldsWithIdAttributes(u"t1", u"t2", u"t3")),
                        IsEmpty()));
  LoadHTML(R"(<body><form id="f1">
    <div>
      <template shadowrootmode="open">
        <form id="f2">
          <input type="text" id="t1">
          <input type="text" id="t2">
        </form>
      </template>
      <input type="text" id="t3">
    </div></form></body>)");
  WaitForFormsSeen();
}

// Tests that explicit form associations are handled correctly.
TEST_F(AutofillAgentShadowDomTest, NestedFormsWithAssociation) {
  EXPECT_CALL(autofill_driver(),
              FormsSeen(HasSingleElementWhich(HasFormIdAttribute(u"f1"),
                                              HasFieldsWithIdAttributes(
                                                  u"t1", u"t2", u"t3", u"t4",
                                                  u"t5", u"t6", u"t7", u"t8")),
                        IsEmpty()));
  LoadHTML(R"(<body><form id="f1">
    <div>
      <template shadowrootmode="open">
        <form id="f2">
          <input id="t1">
          <input id="t2">
          <input id="t3" form="f3">
        </form>
        <form id=f3">
          <input id="t4">
          <input id="t5" form="f2">
        </form>
        <input id="t6" form="f2">
      </template>
      <input id="t7">
    </div></form>
    <input id="t8" form="f1">
    </body>)");
  WaitForFormsSeen();
}

// Tests that multiple nested shadow DOM forms are extracted properly.
TEST_F(AutofillAgentShadowDomTest, MultipleNestedForms) {
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(HasFormIdAttribute(u"f1"),
                                      HasFieldsWithIdAttributes(
                                          u"t1", u"t2", u"t3", u"t4", u"t5")),
                IsEmpty()));
  LoadHTML(R"(<body><form id="f1">
    <div>
      <template shadowrootmode="open">
        <form id="f2">
          <input type="text" id="t1">
          <input type="text" id="t2">
        </form>
      </template>
    </div>
    <input type="text" id="t3">
    <div>
      <template shadowrootmode="open">
        <form id="f3">
          <input type="text" id="t4">
          <input type="text" id="t5">
        </form>
      </template>
    </div>
    </form></body>)");
  WaitForFormsSeen();
}

// Tests that nested shadow DOM forms are extracted properly even if the nesting
// is multiple levels deep.
TEST_F(AutofillAgentShadowDomTest, DeepNestedForms) {
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(HasFormIdAttribute(u"f1"),
                                      HasFieldsWithIdAttributes(
                                          u"t1", u"t2", u"t3", u"t4", u"t5")),
                IsEmpty()));
  LoadHTML(R"(<body><form id="f1">
    <div>
      <template shadowrootmode="open">
        <form id="f2">
          <input type="text" id="t1">
          <input type="text" id="t2">
          <div>
            <template shadowrootmode="open">
              <input type="text" id="t3">
            </template>
          </div>
        </form>
        <div>
          <template shadowrootmode="open">
            <input type="text" id="t4">
            <div>
              <template shadowrootmode="open">
                <form id="f3">
                  <input type="text" id="t5">
                </form>
              </template>
            </div>
          </template>
        </div>
      </template>
    </div></form></body>)");
  WaitForFormsSeen();
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
  EXPECT_CALL(callback,
              Run(Optional(AllOf(
                  Property(&FormData::renderer_id, GetFormRendererIdById("f")),
                  Property(&FormData::name, u"f"),
                  FieldsAre(is_text_input, is_text_input)))));
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
  EXPECT_CALL(callback,
              Run(Optional(AllOf(
                  Property(&FormData::renderer_id, GetFormRendererIdById("ce")),
                  FieldsAre(is_content_editable)))));
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
      FieldRendererId(2),
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

TEST_F(AutofillAgentTestWithFeatures,
       TriggerSuggestionsForElementWithDatalist) {
  EXPECT_CALL(autofill_driver(), FormsSeen);
  LoadHTML(R"(<body><form>
    <input id="ff" list="fruits">
    <datalist id="fruits">
      <option value="Strawberry">
      <option value="Apple">
    </datalist>
  </form></body>)");
  WaitForFormsSeen();

  FormData form;
  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  Property(&FormData::fields,
                           ElementsAre(Property(
                               &FormFieldData::datalist_options,
                               ElementsAre(SelectOption{.value = u"Strawberry"},
                                           SelectOption{.value = u"Apple"})))),
                  _, _, _));
  autofill_agent().TriggerSuggestions(
      GetFieldRendererIdById("ff"),
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  task_environment_.RunUntilIdle();
}

// Tests that `AutofillDriver::TriggerSuggestions()` works for contenteditables.
TEST_F(AutofillAgentTestWithFeatures, TriggerSuggestionsForContenteditable) {
  LoadHTML("<body><div id=ce contenteditable></div></body>");
  FormRendererId form_id = GetFormRendererIdById("ce");
  EXPECT_CALL(autofill_driver(), AskForValuesToFill);
  autofill_agent().TriggerSuggestions(
      FieldRendererId(form_id.value()),
      AutofillSuggestionTriggerSource::kComposeDialogLostFocus);
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
      GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());
  FormData form = *form_util::ExtractFormData(
      forms[0].GetDocument(), forms[0],
      *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy);
  ASSERT_EQ(form.fields().size(), 1u);
  blink::WebFormControlElement field =
      GetWebElementById("text_id").DynamicTo<blink::WebFormControlElement>();
  ASSERT_TRUE(field);

  std::u16string prior_value = form.fields()[0].value();
  test_api(form).field(0).set_value(form.fields()[0].value() + u"AUTOFILLED");
  test_api(form).field(0).set_is_autofilled(true);

  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
  autofill_agent().ApplyFieldsAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kPreview,
                                     GetFieldsForFilling({form}));
  EXPECT_EQ(field.GetAutofillState(), blink::WebAutofillState::kPreviewed);
  autofill_agent().ClearPreviewedForm();
  EXPECT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
}

// Tests that when JS modifies a value, the autofill state is only lost if the
// changes were not simple reformatting changes.
TEST_F(AutofillAgentTest, JavaScriptChangedValue_AutofillState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {blink::features::kAllowJavaScriptToResetAutofillState,
       features::kAutofillFixCachingOnJavaScriptChanges},
      /*disabled_features=*/{});
  LoadHTML(R"(
    <form id="form_id">
      <input id="cc_number">
      <input id="phone_number">
      <input id="full_name">
    </form>
  )");
  blink::WebFormControlElement cc_field =
      GetWebElementById("cc_number").DynamicTo<blink::WebFormControlElement>();
  blink::WebFormControlElement phone_field =
      GetWebElementById("phone_number")
          .DynamicTo<blink::WebFormControlElement>();
  blink::WebFormControlElement name_field =
      GetWebElementById("full_name").DynamicTo<blink::WebFormControlElement>();

  cc_field.SetAutofillValue("4111111111111111");
  ASSERT_EQ(cc_field.Value().Ascii(), "4111111111111111");
  ASSERT_TRUE(cc_field.IsAutofilled());

  phone_field.SetAutofillValue("12345678900");  //+1 [234] 567-8900
  ASSERT_EQ(phone_field.Value().Ascii(), "12345678900");
  ASSERT_TRUE(phone_field.IsAutofilled());

  name_field.SetAutofillValue("John Doe");
  ASSERT_EQ(name_field.Value().Ascii(), "John Doe");
  ASSERT_TRUE(name_field.IsAutofilled());

  ExecuteJavaScriptForTests(R"(
    document.forms[0].elements[0].value = '4111 1111 1111 1111';
    document.forms[0].elements[1].value = '+1 (234) 567-8900';
    document.forms[0].elements[2].value = 'Mr. John Doe';
  )");

  ASSERT_EQ(cc_field.Value().Ascii(), "4111 1111 1111 1111");
  EXPECT_TRUE(cc_field.IsAutofilled());

  ASSERT_EQ(phone_field.Value().Ascii(), "+1 (234) 567-8900");
  EXPECT_TRUE(phone_field.IsAutofilled());

  ASSERT_EQ(name_field.Value().Ascii(), "Mr. John Doe");
  EXPECT_FALSE(name_field.IsAutofilled());
}

class AutofillAgentSubmissionTest : public AutofillAgentTest,
                                    public testing::WithParamInterface<int> {
 public:
  AutofillAgentSubmissionTest() {
    EXPECT_LE(GetParam(), 2);
    std::vector<base::test::FeatureRef> features = {
        features::kAutofillReplaceCachedWebElementsByRendererIds,
        features::kAutofillReplaceFormElementObserver};

    std::vector<base::test::FeatureRef> enabled_features(
        features.begin(), features.begin() + GetParam());
    std::vector<base::test::FeatureRef> disabled_features(
        features.begin() + GetParam(), features.end());
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool improved_submission_detection() {
    return base::FeatureList::IsEnabled(
        features::kAutofillReplaceFormElementObserver);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AutofillSubmissionTest,
                         AutofillAgentSubmissionTest,
                         ::testing::Values(0, 1, 2));

// Test that AutofillAgent::JavaScriptChangedValue updates the
// last interacted saved state.
TEST_P(AutofillAgentSubmissionTest,
       JavaScriptChangedValueUpdatesLastInteractedSavedState) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillReplaceFormElementObserver)) {
    GTEST_SKIP();
  }
  LoadHTML(R"(<form id="form_id"><input id="text_id"></form>)");

  blink::WebFormElement form =
      GetWebElementById("form_id").DynamicTo<blink::WebFormElement>();
  FormRendererId form_id = form_util::GetFormRendererId(form);

  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].value = 'js_set_value';)");
  std::optional<FormData> provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  // Since we do not have a tracked form yet, the JS call should not update (in
  // this case set) the last interacted form.
  ASSERT_FALSE(provisionally_saved_form.has_value());

  SimulateUserInputChangeForElementById("text_id", "user_set_value");
  provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  ASSERT_TRUE(provisionally_saved_form.has_value());
  EXPECT_EQ(provisionally_saved_form->renderer_id(), form_id);
  ASSERT_EQ(1u, provisionally_saved_form->fields().size());
  EXPECT_EQ(u"user_set_value", provisionally_saved_form->fields()[0].value());

  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].value = 'js_set_value';)");
  provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  // Since we now have a tracked form and JS modified the same form, we should
  // see the JS modification reflected in the last interacted saved form.
  ASSERT_TRUE(provisionally_saved_form.has_value());
  EXPECT_EQ(provisionally_saved_form->renderer_id(), form_id);
  ASSERT_EQ(1u, provisionally_saved_form->fields().size());
  EXPECT_EQ(u"js_set_value", provisionally_saved_form->fields()[0].value());
  EXPECT_EQ(u"user_set_value",
            provisionally_saved_form->fields()[0].user_input());
}

// Test that AutofillAgent::ApplyFormAction(mojom::ActionPersistence::kFill)
// updates the last interacted saved state when the <input>s have no containing
// <form>.
TEST_P(AutofillAgentSubmissionTest,
       FormlessApplyFormActionUpdatesLastInteractedSavedState) {
  LoadHTML(R"(
    <input id="text_id">
  )");

  blink::WebFormControlElement field =
      GetWebElementById("text_id").DynamicTo<blink::WebFormControlElement>();
  ASSERT_TRUE(field);

  FormFieldData form_field;
  form_util::WebFormControlElementToFormFieldForTesting(
      blink::WebFormElement(), field, &autofill_agent().field_data_manager(),
      {}, &form_field);

  form_field.set_value(u"autofilled");
  form_field.set_is_autofilled(true);

  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
  FormData form;
  form.set_fields({form_field});
  autofill_agent().ApplyFieldsAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kFill,
                                     GetFieldsForFilling({form}));
  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kAutofilled);

  std::optional<FormData> provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  ASSERT_TRUE(provisionally_saved_form.has_value());
  ASSERT_EQ(1u, provisionally_saved_form->fields().size());
  EXPECT_EQ(u"autofilled", provisionally_saved_form->fields()[0].value());
}

// Test that AutofillAgent::ApplyFormAction(mojom::ActionPersistence::kFill)
// updates the last interacted saved state when the <input>s have a containing
// <form>.
TEST_P(AutofillAgentSubmissionTest,
       FormApplyFormActionUpdatesLastInteractedSavedState) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="text_id">
    </form>
  )");

  blink::WebFormElement form_element =
      GetWebElementById("form_id").DynamicTo<blink::WebFormElement>();
  ASSERT_EQ(1u, form_element.GetFormControlElements().size());
  blink::WebFormControlElement field = form_element.GetFormControlElements()[0];
  ASSERT_TRUE(field);
  ASSERT_EQ("text_id", field.GetIdAttribute().Ascii());

  FormData form = *form_util::ExtractFormData(
      form_element.GetDocument(), form_element,
      *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy);

  ASSERT_EQ(1u, form.fields().size());
  test_api(form).field(0).set_value(u"autofilled");
  test_api(form).field(0).set_is_autofilled(true);

  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
  autofill_agent().ApplyFieldsAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kFill,
                                     GetFieldsForFilling({form}));
  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kAutofilled);

  std::optional<FormData> provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  ASSERT_TRUE(provisionally_saved_form.has_value());
  ASSERT_EQ(1u, provisionally_saved_form->fields().size());
  EXPECT_EQ(u"autofilled", provisionally_saved_form->fields()[0].value());
}

TEST_P(AutofillAgentSubmissionTest,
       HideElementTriggersFormTracker_DisplayNone) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="field_id">
    </form>
  )");
  blink::WebElement element = GetWebElementById("field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].style.display = 'none';)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

TEST_P(AutofillAgentSubmissionTest,
       HideElementTriggersFormTracker_VisibilityHidden) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="field_id">
    </form>
  )");
  blink::WebElement element = GetWebElementById("field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].style.visibility = 'hidden';)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

TEST_P(AutofillAgentSubmissionTest, HideElementTriggersFormTracker_TypeHidden) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="field_id">
    </form>
  )");
  blink::WebElement element = GetWebElementById("field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].setAttribute('type', 'hidden');)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

TEST_P(AutofillAgentSubmissionTest, HideElementTriggersFormTracker_HiddenTrue) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="field_id">
    </form>
  )");
  blink::WebElement element = GetWebElementById("field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].setAttribute('hidden', 'true');)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

TEST_P(AutofillAgentSubmissionTest, HideElementTriggersFormTracker_ShadowDom) {
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
  blink::WebElement element = GetWebElementById("field_id");

  EXPECT_CALL(form_tracker(), ElementDisappeared(element));
  ExecuteJavaScriptForTests(R"(field_id.slot = "unknown";)");
  GetWebFrameWidget()->UpdateAllLifecyclePhases(
      blink::DocumentUpdateReason::kTest);
}

// Test that an inferred form submission as a result of a page deleting ALL of
// the <input>s (that the user has edited) on a page with no <form> sends the
// contents of all of the fields to the browser.
TEST_P(AutofillAgentSubmissionTest,
       FormlessOnInferredFormSubmissionAfterXhrAndAllInputsRemoved) {
  LoadHTML(R"(
    <div id='shipping'>
    Name: <input type='text' id='name'><br>
    Address: <input type='text' id='address'>
    </div>
  )");

  SimulateUserInputChangeForElementById("name", "Ariel");
  SimulateUserInputChangeForElementById("address", "Atlantica");

  EXPECT_CALL(autofill_driver(),
              FormSubmitted(
                  AllOf(FieldsAre(HasFieldIdAttribute(u"name"),
                                  HasFieldIdAttribute(u"address")),
                        FieldsAre(HasValue(u"Ariel"), HasValue(u"Atlantica"))),
                  _, _));

  // Simulate inferred form submission as a result the focused field being
  // removed after an AJAX call.
  ExecuteJavaScriptForTests(
      R"(document.getElementById('shipping').innerHTML = '')");
  autofill_agent().OnInferredFormSubmission(
      mojom::SubmissionSource::XHR_SUCCEEDED);
}

// Tests that an inferred form submission as a result of a page deleting ALL of
// the <input>s that the user has edited but NOT ALL of the <inputs> on the page
// sends the user-edited <inputs> to the browser.
TEST_P(AutofillAgentSubmissionTest,
       FormlessOnInferredFormSubmissionAfterXhrAndSomeInputsRemoved) {
  LoadHTML(R"(
    Search: <input type='text' id='search'><br>
    <div id='shipping'>
    Name: <input type='text' id='name'><br>
    Address: <input type='text' id='address'>
    </div>
  )");

  SimulateUserInputChangeForElementById("name", "Ariel");
  SimulateUserInputChangeForElementById("address", "Atlantica");

  EXPECT_CALL(autofill_driver(),
              FormSubmitted(AllOf(FieldsAre(HasFieldIdAttribute(u"search"),
                                            HasFieldIdAttribute(u"name"),
                                            HasFieldIdAttribute(u"address")),
                                  FieldsAre(HasValue(u""), HasValue(u"Ariel"),
                                            HasValue(u"Atlantica"))),
                            _, _));

  // Simulate inferred form submission as a result the focused field being
  // removed after an AJAX call.
  ExecuteJavaScriptForTests(R"(document.getElementById('shipping').remove();)");
  autofill_agent().OnInferredFormSubmission(
      mojom::SubmissionSource::XHR_SUCCEEDED);
}

// Test scenario WHERE:
// - AutofillAgent::OnProbablyFormSubmitted() is called as a result of a page
// navigation. AND
// - There is no <form> element.
// AND
// - An <input> other than the last interacted <input> is hidden.
// THAT
// The edited <input>s are sent to the browser.
TEST_P(AutofillAgentSubmissionTest,
       FormlessOnNavigationAfterSomeInputsRemoved) {
  LoadHTML(R"(
    Name: <input type='text' id='name'><br>
    Address: <input type='text' id='address'>
  )");

  SimulateUserInputChangeForElementById("name", "Ariel");
  SimulateUserInputChangeForElementById("address", "Atlantica");

  if (improved_submission_detection()) {
    EXPECT_CALL(autofill_driver(),
                FormSubmitted(AllOf(FieldsAre(HasFieldIdAttribute(u"name"),
                                              HasFieldIdAttribute(u"address")),
                                    FieldsAre(HasValue(u"Ariel"),
                                              HasValue(u"Atlantica"))),
                              _, _));
  } else {
    EXPECT_CALL(autofill_driver(),
                FormSubmitted(AllOf(FieldsAre(HasFieldIdAttribute(u"address")),
                                    FieldsAre(HasValue(u"Atlantica"))),
                              _, _));
  }

  // Remove element that the user did not interact with last.
  ExecuteJavaScriptForTests(R"(document.getElementById('name').remove();)");
  // Simulate page navigation.
  autofill_agent().OnProbablyFormSubmitted();
}

// Test that in the scenario that:
// - The user autofills a form which dynamically removes -
//   during autofill - `AutofillAgent::last_queried_element_` from the DOM
//   hierarchy.
// THAT
// - Inferred form submission as a result of the page removing the <form> from
//   the DOM hierarchy does not send fields which were removed from the DOM
//   hierarchy at autofill time.
TEST_P(AutofillAgentSubmissionTest,
       OnInferredFormSubmissionAfterAutofillRemovesLastQueriedElement) {
  LoadHTML(R"(
    <form id="form">
      <input id="input1">
      <input id="input2" onchange="document.getElementById('input1').remove();">
    </form>
  )");

  blink::WebFormElement form_element =
      GetWebElementById("form").DynamicTo<blink::WebFormElement>();
  ASSERT_TRUE(form_element);
  std::optional<FormData> form = form_util::ExtractFormData(
      GetDocument(), form_element, autofill_agent().field_data_manager(),
      kCallTimerStateDummy);
  ASSERT_TRUE(form.has_value());

  blink::WebVector<blink::WebFormControlElement> field_elements =
      form_element.GetFormControlElements();

  for (const blink::WebFormControlElement& field_element : field_elements) {
    ASSERT_EQ(field_element.GetAutofillState(),
              blink::WebAutofillState::kNotFilled);
  }

  for (FormFieldData& field : test_api(*form).fields()) {
    field.set_value(field.id_attribute() + u" autofilled");
    field.set_is_autofilled(true);
  }

  // Update `AutofillAgent::last_queried_element_`.
  static_cast<content::RenderFrameObserver*>(&autofill_agent())
      ->FocusedElementChanged(field_elements[0]);

  autofill_agent().ApplyFieldsAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kFill,
                                     GetFieldsForFilling({*form}));

  for (const blink::WebFormControlElement& field_element : field_elements) {
    ASSERT_EQ(field_element.GetAutofillState(),
              blink::WebAutofillState::kAutofilled);
  }

  EXPECT_CALL(autofill_driver(),
              FormSubmitted(AllOf(FieldsAre(HasFieldIdAttribute(u"input2")),
                                  FieldsAre(HasValue(u"input2 autofilled"))),
                            _, _));
  ExecuteJavaScriptForTests(R"(document.getElementById('form').remove();)");
  autofill_agent().OnInferredFormSubmission(
      mojom::SubmissionSource::XHR_SUCCEEDED);
}

class AutofillAgentTestNavigationReset : public AutofillAgentTest {
 public:
  std::unique_ptr<AutofillAgent> CreateAutofillAgent(
      content::RenderFrame* render_frame,
      const AutofillAgent::Config& config,
      std::unique_ptr<PasswordAutofillAgent> password_autofill_agent,
      std::unique_ptr<PasswordGenerationAgent> password_generation_agent,
      blink::AssociatedInterfaceRegistry* associated_interfaces) override {
    return std::make_unique<MockAutofillAgent>(
        render_frame, config, std::move(password_autofill_agent),
        std::move(password_generation_agent), associated_interfaces);
  }

  MockAutofillAgent& autofill_agent() {
    return static_cast<MockAutofillAgent&>(AutofillAgentTest::autofill_agent());
  }
};

TEST_F(AutofillAgentTestNavigationReset, NavigationResetsIsDomContentLoaded) {
  std::vector<bool> is_dom_content_loaded;
  EXPECT_CALL(autofill_agent(), DidDispatchDOMContentLoadedEvent)
      .WillRepeatedly([&]() {
        is_dom_content_loaded.push_back(
            test_api(autofill_agent()).is_dom_content_loaded());
        autofill_agent().OverriddenDidDispatchDOMContentLoadedEvent();
        is_dom_content_loaded.push_back(
            test_api(autofill_agent()).is_dom_content_loaded());
      });
  LoadHTML(R"(Hello world)");
  LoadHTML(R"(Hello world)");
  EXPECT_THAT(is_dom_content_loaded, ElementsAre(false, true, false, true));
}

// Test fixture for FocusedElementChanged().
class AutofillAgentTestFocus : public AutofillAgentTest {
 public:
  // A permutation of the fields. Cycling through these fields guarantees a
  // diverse collection of transitions:
  // - [un]owned -> [un]owned (all four combinations),
  // - <input>, <select>, <textarea>, contenteditable
  static constexpr std::array kPermutationOfFields = {
      "owned_field",  "owned_select",  "unowned_field",  "contenteditable",
      "owned_field2", "unowned_field", "unowned_select", "owned_select2"};

  void SetUp() override {
    AutofillAgentTest::SetUp();
    LoadHTML(R"(
      <html>
      <div id=uneditable></div>
      <div id=contenteditable contenteditable></div>
      <input id=unowned_field>
      <select id=unowned_select><option>Something</option></select>
      <form>
        <input id=owned_field>
        <select id=owned_select><option>Something</option></select>
      </form>
      <form>
        <textarea id=owned_field2></textarea>
        <select id=owned_select2><option>Something</option></select>
      </form>
    )");
    for (std::string_view id : kPermutationOfFields) {
      ASSERT_TRUE(GetWebElementById(id));
    }
  }

  void FocusedElementChanged(blink::WebElement e) {
    test_api(autofill_agent()).FocusedElementChanged(e);
    task_environment_.RunUntilIdle();
  }

  void FocusedElementChanged(std::string_view id) {
    blink::WebElement e = GetWebElementById(id);
    ASSERT_TRUE(e) << "Field " << id << " doesn't exist";
    FocusedElementChanged(e);
  }
};

// Tests that when the focus moves from field to field, FocusedElementChanged()
// fires FocusOnFormField() and FocusOnNonFormField().
TEST_F(AutofillAgentTestFocus, FireFocusEventsWhenCyclingThroughFields) {
  testing::MockFunction<void(std::string_view)> checkpoint;
  {
    testing::InSequence s;
    // Moves the focus one field to another.
    for (std::string_view id : kPermutationOfFields) {
      EXPECT_CALL(checkpoint, Call(id));
      EXPECT_CALL(autofill_driver(), FocusOnNonFormField).Times(0);
      EXPECT_CALL(autofill_driver(),
                  FocusOnFormField(_, GetFieldRendererIdById(id)));
    }
  }
  for (std::string_view id : kPermutationOfFields) {
    checkpoint.Call(id);
    FocusedElementChanged(id);
  }
}

// Tests that when the focus switches between an uneditable <div> and
// a field, FocusedElementChanged() fires FocusOnFormField() and
// FocusOnNonFormField().
TEST_F(AutofillAgentTestFocus,
       FireFocusEventsWhenSwitchingBetweenFieldAndNonField) {
  testing::MockFunction<void(std::string_view)> checkpoint;
  {
    testing::InSequence s;
    for (std::string_view id : kPermutationOfFields) {
      EXPECT_CALL(checkpoint, Call("uneditable"));
      EXPECT_CALL(autofill_driver(), FocusOnNonFormField);
      EXPECT_CALL(autofill_driver(), FocusOnFormField).Times(0);
      EXPECT_CALL(checkpoint, Call(id));
      EXPECT_CALL(autofill_driver(), FocusOnNonFormField).Times(0);
      EXPECT_CALL(autofill_driver(),
                  FocusOnFormField(_, GetFieldRendererIdById(id)));
    }
  }
  for (std::string_view id : kPermutationOfFields) {
    checkpoint.Call("uneditable");
    FocusedElementChanged("uneditable");
    checkpoint.Call(id);
    FocusedElementChanged(id);
  }
}

// Tests that FocusedElementChanged() treats null as a non-FormField.
TEST_F(AutofillAgentTestFocus, FireFocusEventsForNullElement) {
  testing::MockFunction<void(std::string_view)> checkpoint;
  {
    testing::InSequence s;
    EXPECT_CALL(checkpoint, Call("owned_field"));
    EXPECT_CALL(autofill_driver(), FocusOnFormField);
    EXPECT_CALL(checkpoint, Call("null"));
    EXPECT_CALL(autofill_driver(), FocusOnNonFormField);
    EXPECT_CALL(checkpoint, Call("contenteditable"));
    EXPECT_CALL(autofill_driver(), FocusOnFormField);
    EXPECT_CALL(checkpoint, Call("null"));
    EXPECT_CALL(autofill_driver(), FocusOnNonFormField);
  }
  checkpoint.Call("owned_field");
  FocusedElementChanged("owned_field");
  checkpoint.Call("null");
  FocusedElementChanged(blink::WebElement());
  checkpoint.Call("contenteditable");
  FocusedElementChanged("contenteditable");
  checkpoint.Call("null");
  FocusedElementChanged(blink::WebElement());
}

// Test fixture for caret position extraction and movement detection.
class AutofillAgentTestCaret
    : public AutofillAgentTest,
      public ::testing::WithParamInterface<FormControlType> {
 public:
  FormControlType form_control_type() const { return GetParam(); }

  void SetUp() override {
    AutofillAgentTest::SetUp();
    switch (form_control_type()) {
      case FormControlType::kContentEditable:
        LoadHTML(
            R"(<div id=f contenteditable
               style="width: 10em; height: 3ex;">012345</div>)");
        break;
      case FormControlType::kInputText:
        LoadHTML(R"(<input id=f value=012345>)");
        break;
      case FormControlType::kTextArea:
        LoadHTML(R"(<textarea id=f>012345</textarea>)");
        break;
      default:
        NOTREACHED();
    }
  }

  blink::WebElement GetElement() { return GetWebElementById("f"); }

  void TriggerAskForValuesToFill() {
    switch (form_control_type()) {
      case FormControlType::kContentEditable:
        test_api(autofill_agent())
            .ShowSuggestionsForContentEditable(GetElement(), {});
        break;
      case FormControlType::kInputText:
      case FormControlType::kTextArea:
        test_api(autofill_agent())
            .QueryAutofillSuggestions(
                GetElement().DynamicTo<blink::WebFormControlElement>(), {});
        break;
      default:
        NOTREACHED();
    }
    task_environment_.RunUntilIdle();
  }

  void SetCaret(int begin, int end, base::TimeDelta pause_for) {
    switch (form_control_type()) {
      case FormControlType::kContentEditable:
        ExecuteJavaScriptForTests(base::StringPrintf(
            R"(var c = document.getElementById('f').firstChild;
               document.getSelection().setBaseAndExtent(c, %d, c, %d);)",
            begin, end));
        break;
      case FormControlType::kInputText:
      case FormControlType::kTextArea:
        ExecuteJavaScriptForTests(base::StringPrintf(
            R"(document.getElementById('f').setSelectionRange(%d, %d);)", begin,
            end));
        break;
      default:
        NOTREACHED();
    }
    task_environment_.FastForwardBy(pause_for);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::kAutofillCaretExtraction};
};

INSTANTIATE_TEST_SUITE_P(AutofillAgentTest,
                         AutofillAgentTestCaret,
                         ::testing::Values(FormControlType::kTextArea,
                                           FormControlType::kContentEditable));

// Tests that AskForValuesToFill() is parameterized with the caret position.
TEST_P(AutofillAgentTestCaret, AskForValuesToFillContainsCaret) {
  FormData form;
  gfx::Rect caret_bounds;
  EXPECT_CALL(autofill_driver(), AskForValuesToFill)
      .WillOnce(DoAll(SaveArg<0>(&form), SaveArg<2>(&caret_bounds)));
  Focus("f");
  TriggerAskForValuesToFill();
  EXPECT_FALSE(form.fields()[0].bounds().IsEmpty());
  EXPECT_FALSE(caret_bounds.origin().IsOrigin());
  EXPECT_GT(caret_bounds.height(), 0);
  EXPECT_TRUE(form.fields()[0].bounds().Contains(gfx::RectF(caret_bounds)));
}

// Tests that CaretMovedInFormField() is fired for each caret movement, provided
// there's enough time between the movements.
TEST_P(AutofillAgentTestCaret, MovingCaretSlowlyFiresEvent) {
  std::array<FormData, 3> forms;
  std::array<gfx::Rect, 3> caret_bounds;
  testing::MockFunction<void(std::string_view)> checkpoint;
  {
    testing::InSequence s;
    EXPECT_CALL(checkpoint, Call("focus"));
    EXPECT_CALL(autofill_driver(), CaretMovedInFormField)
        .WillOnce(DoAll(SaveArg<0>(&forms[0]), SaveArg<2>(&caret_bounds[0])));
    EXPECT_CALL(checkpoint, Call("first move"));
    EXPECT_CALL(autofill_driver(), CaretMovedInFormField)
        .WillOnce(DoAll(SaveArg<0>(&forms[1]), SaveArg<2>(&caret_bounds[1])));
    EXPECT_CALL(checkpoint, Call("second move"));
    EXPECT_CALL(autofill_driver(), CaretMovedInFormField)
        .WillOnce(DoAll(SaveArg<0>(&forms[2]), SaveArg<2>(&caret_bounds[2])));
    EXPECT_CALL(checkpoint, Call("done"));
  }
  checkpoint.Call("focus");
  Focus("f");
  checkpoint.Call("first move");
  SetCaret(1, 1, /*pause_for=*/base::Seconds(1));
  checkpoint.Call("second move");
  SetCaret(2, 2, /*pause_for=*/base::Seconds(1));
  checkpoint.Call("done");
  EXPECT_TRUE(
      forms[0].fields()[0].bounds().Contains(gfx::RectF(caret_bounds[0])));
  EXPECT_TRUE(
      forms[1].fields()[0].bounds().Contains(gfx::RectF(caret_bounds[1])));
  EXPECT_TRUE(
      forms[2].fields()[0].bounds().Contains(gfx::RectF(caret_bounds[2])));
  EXPECT_FALSE(caret_bounds[0].origin().IsOrigin());
  EXPECT_FALSE(caret_bounds[1].origin().IsOrigin());
  EXPECT_FALSE(caret_bounds[2].origin().IsOrigin());
  EXPECT_NE(caret_bounds[0], caret_bounds[1]);
  EXPECT_NE(caret_bounds[0], caret_bounds[2]);
  EXPECT_NE(caret_bounds[1], caret_bounds[2]);
}

// Tests that CaretMovedInFormField() is fired in a throttled manner when the
// caret moves fast.
TEST_P(AutofillAgentTestCaret, MovingCaretFastThrottlesEvent) {
  testing::MockFunction<void(std::string_view)> checkpoint;
  {
    testing::InSequence s;
    EXPECT_CALL(checkpoint, Call("focus"));
    EXPECT_CALL(autofill_driver(), CaretMovedInFormField);
    EXPECT_CALL(checkpoint, Call("first move"));
    EXPECT_CALL(autofill_driver(), CaretMovedInFormField);
    EXPECT_CALL(checkpoint, Call("second move is ignored"));
    EXPECT_CALL(autofill_driver(), CaretMovedInFormField).Times(0);
    EXPECT_CALL(checkpoint, Call("third move is throttled"));
    EXPECT_CALL(autofill_driver(), CaretMovedInFormField).Times(0);
    EXPECT_CALL(checkpoint, Call("timer expires"));
    EXPECT_CALL(autofill_driver(), CaretMovedInFormField);
    EXPECT_CALL(checkpoint, Call("done"));
  }
  checkpoint.Call("focus");
  Focus("f");
  checkpoint.Call("first move");
  SetCaret(1, 1, /*pause_for=*/base::Milliseconds(1));
  checkpoint.Call("second move is ignored");
  SetCaret(2, 2, /*pause_for=*/base::Milliseconds(1));
  checkpoint.Call("third move is throttled");
  SetCaret(3, 3, /*pause_for=*/base::Milliseconds(1));
  checkpoint.Call("timer expires");
  task_environment_.FastForwardBy(base::Seconds(1));
  checkpoint.Call("done");
}

// Tests that selecting text fires CaretMovedInFormField() with the text
// selection.
TEST_P(AutofillAgentTestCaret, SelectionFiresEvent) {
  testing::MockFunction<void(std::string_view)> checkpoint;
  {
    testing::InSequence s;
    EXPECT_CALL(checkpoint, Call("focus"));
    EXPECT_CALL(autofill_driver(),
                CaretMovedInFormField(FieldsAre(HasSelectedText(u"")), _, _));
    EXPECT_CALL(checkpoint, Call("selection"));
    EXPECT_CALL(
        autofill_driver(),
        CaretMovedInFormField(FieldsAre(HasSelectedText(u"123")), _, _));
    EXPECT_CALL(checkpoint, Call("done"));
  }
  checkpoint.Call("focus");
  Focus("f");
  checkpoint.Call("selection");
  SetCaret(1, 4, /*pause_for=*/base::Seconds(1));
  checkpoint.Call("done");
}

// Tests fixture for click handling.
class AutofillAgentTestClick
    : public AutofillAgentTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  const char* field_html() const { return GetParam(); }

  void SetUp() override {
    AutofillAgentTest::SetUp();
    // The DIV and SPAN dimensions are chosen so that
    // - an empty DIV is clickable and
    // - clicking on a non-empty DIV hits the node (a text node or SPAN) inside
    //   that DIV.
    LoadHTML(base::StringPrintf(R"(<html>
                                   <style>
                                   div { width: 5em; height: 2ex; }
                                   </style>
                                   <body>
                                   <div id=other></div>
                                   %s)",
                                field_html()));
  }
};

INSTANTIATE_TEST_SUITE_P(
    AutofillAgentTest,
    AutofillAgentTestClick,
    ::testing::Values(R"(<input id=f>)",
                      R"(<textarea id=f></textarea>)",
                      R"(<div contenteditable id=f></div>)",
                      R"(<div contenteditable id=f>Hello world</div>)",
                      R"(<div contenteditable id=f><div></div></div>)"));

// Tests that clicking on a field triggers AskForValuesToFillOnClick().
// TODO(crbug.com/342126797): Fix Android's OnAskForValuesToFill() event.
#if !BUILDFLAG(IS_ANDROID)
#define MAYBE_AskForValuesToFillOnClick AskForValuesToFillOnClick
#else
#define MAYBE_AskForValuesToFillOnClick DISABLED_AskForValuesToFillOnClick
#endif
TEST_P(AutofillAgentTestClick, MAYBE_AskForValuesToFillOnClick) {
  testing::MockFunction<void(std::string_view)> checkpoint;
  {
    testing::InSequence s;
    FieldRendererId field = GetFieldRendererIdById("f");

    EXPECT_CALL(checkpoint, Call("click on field"));
    EXPECT_CALL(autofill_driver(), AskForValuesToFill(_, field, _, _));

    EXPECT_CALL(checkpoint, Call("click on field"));
    EXPECT_CALL(autofill_driver(), AskForValuesToFill(_, field, _, _));

    EXPECT_CALL(checkpoint, Call("click outside of field"));
    EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);

    EXPECT_CALL(checkpoint, Call("right click on field"));
    EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, _, _,
            AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick))
        .Times(AtMost(1));
  }

  WaitForFormsSeen();
  checkpoint.Call("click on field");
  Click("f");
  checkpoint.Call("click on field");
  Click("f");
  checkpoint.Call("click outside of field");
  Click("other");
  checkpoint.Call("right click on field");
  RightClick("f");
}

// Tests that DOMContentLoaded() emits a metric.
TEST_F(AutofillAgentTest, DOMContentLoadedEmitsMetric) {
  base::HistogramTester histogram_tester;
  LoadHTML(R"(
    <p>Hello world</p>
  )");
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.DOMContentLoadedInOutermostMainFrame"),
              base::BucketsAre(base::Bucket(true, 1), base::Bucket(false, 0)));
}

}  // namespace

}  // namespace autofill
