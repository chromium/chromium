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
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_tracker.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
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
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::SizeIs;

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

// Matches a `FormData` whose `FormData::fields`' `FormFieldData::id_attribute`
// match `id_attributes`.
auto HasFieldsWithIdAttributes(std::vector<std::u16string> id_attributes) {
  std::vector<Matcher<FormFieldData>> field_matchers;
  for (std::u16string& id_attribute : id_attributes) {
    field_matchers.push_back(
        Property(&FormFieldData::id_attribute, std::move(id_attribute)));
  }
  return Field(&FormData::fields, ElementsAreArray(field_matchers));
}

// Matches a `FormData` with a specific `FormData::renderer_id`.
auto HasFormId(FormRendererId expectation) {
  return Field(&FormData::renderer_id, expectation);
}

// Matches a `FormFieldData` with a specific `FormData::renderer_id`.
auto HasFieldId(FieldRendererId expectation) {
  return Property(&FormFieldData::renderer_id, expectation);
}

// Matches a `FormData` with a specific `FormData::id_attribute`.
auto HasFormIdAttribute(std::u16string id_attribute) {
  return Field(&FormData::id_attribute, std::move(id_attribute));
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
  return Property(&FormFieldData::form_control_type, type);
}

// Matches a FormFieldData if its `i`th field's `member` matches the
// `expected_values[i]`.
// `member` may point to a data member or to a member function of
// `FormFieldData`.
auto FieldsAre(std::string field_name,
               auto&& member,
               std::vector<std::u16string> expected_values) {
  std::vector<::testing::Matcher<FormFieldData>> matchers;
  for (const std::u16string& expected : expected_values) {
    if constexpr (std::is_member_function_pointer_v<
                      std::decay_t<decltype(member)>>) {
      matchers.push_back(Property(field_name, member, expected));
    } else {
      matchers.push_back(Field(field_name, member, expected));
    }
  }
  return Field(&FormData::fields, ElementsAreArray(matchers));
}

// TODO(crbug.com/41268731): Add many more test cases.
class AutofillAgentTest : public test::AutofillRendererTest {
 public:
  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    test_api(autofill_agent())
        .set_form_tracker(std::make_unique<MockFormTracker>(
            GetMainRenderFrame(), FormTracker::UserGestureRequired(true),
            autofill_agent()));
  }

  blink::WebElement GetWebElementById(std::string_view id) {
    return GetMainFrame()->GetDocument().GetElementById(
        blink::WebString::FromUTF8(id));
  }

  FormRendererId GetFormRendererIdById(std::string_view id) {
    return form_util::GetFormRendererId(
        GetMainFrame()->GetDocument().GetElementById(
            blink::WebString::FromUTF8(id)));
  }

  FieldRendererId GetFieldRendererIdById(std::string_view id) {
    return form_util::GetFieldRendererId(
        GetMainFrame()->GetDocument().GetElementById(
            blink::WebString::FromUTF8(id)));
  }

  void SimulateUserEditField(const blink::WebFormElement& form,
                             const std::string& field_id,
                             const std::string& value) {
    blink::WebFormControlElement element =
        GetWebElementById(field_id).To<blink::WebFormControlElement>();
    element.SetValue(blink::WebString::FromUTF8(value));
    // Call AutofillAgent::OnProvisionallySaveForm() in order to update
    // AutofillAgent::formless_elements_user_edited_
    autofill_agent().OnProvisionallySaveForm(
        form, element,
        FormTracker::Observer::SaveFormReason::kTextFieldChanged);
  }

  MockFormTracker& form_tracker() {
    return static_cast<MockFormTracker&>(
        test_api(autofill_agent()).form_tracker());
  }

  std::vector<FormFieldData::FillData> GetFieldsForFilling(
      const std::vector<FormData>& forms) {
    std::vector<FormFieldData::FillData> fields;
    for (const FormData& form : forms) {
      for (const FormFieldData& field : form.fields) {
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
        {features::kAutofillReplaceCachedWebElementsByRendererIds,
         features::kAutofillDetectRemovedFormControls},
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

class AutofillAgentShadowDomTest : public AutofillAgentTestWithFeatures {
 public:
  AutofillAgentShadowDomTest() {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kAutofillIncludeShadowDomInUnassociatedListedElements,
         blink::features::kAutofillIncludeFormElementsInShadowDom},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Tests that unassociated form control elements in a Shadow DOM tree that do
// not have a form ancestor are extracted correctly.
TEST_F(AutofillAgentShadowDomTest, UnownedUnassociatedElements) {
  EXPECT_CALL(autofill_driver(),
              FormsSeen(HasSingleElementWhich(
                            HasFieldsWithIdAttributes({u"t1", u"t2"})),
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
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(
                    HasFormIdAttribute(u"f1"),
                    HasFieldsWithIdAttributes({u"t1", u"t2", u"t3", u"t4"})),
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
  EXPECT_CALL(autofill_driver(),
              FormsSeen(HasSingleElementWhich(
                            HasFormIdAttribute(u""),
                            HasFieldsWithIdAttributes({u"t1", u"t2"})),
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
  EXPECT_CALL(autofill_driver(),
              FormsSeen(HasSingleElementWhich(
                            HasFormIdAttribute(u"f1"),
                            HasFieldsWithIdAttributes({u"t1", u"t2"})),
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
                            HasFieldsWithIdAttributes({u"t1", u"t2", u"t3"})),
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
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(HasSingleElementWhich(
                    HasFormIdAttribute(u"f1"),
                    HasFieldsWithIdAttributes({u"t1", u"t2", u"t3", u"t4",
                                               u"t5", u"t6", u"t7", u"t8"})),
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
                                          {u"t1", u"t2", u"t3", u"t4", u"t5"})),
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
                                          {u"t1", u"t2", u"t3", u"t4", u"t5"})),
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
                  Field(&FormData::renderer_id, GetFormRendererIdById("f")),
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
      Run(Optional(
          AllOf(Field(&FormData::renderer_id, GetFormRendererIdById("ce")),
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
      FieldRendererId(2),
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
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
      GetMainFrame()->GetDocument().GetTopLevelForms();
  EXPECT_EQ(1U, forms.size());
  FormData form =
      *form_util::ExtractFormData(forms[0].GetDocument(), forms[0],
                                  *base::MakeRefCounted<FieldDataManager>(),
                                  {form_util::ExtractOption::kValue});

  ASSERT_TRUE(autofill_agent().focused_element().IsNull());
  autofill_agent().ApplyFieldsAction(mojom::FormActionType::kUndo,
                                     mojom::ActionPersistence::kFill,
                                     GetFieldsForFilling({form}));
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
      GetMainFrame()->GetDocument().GetTopLevelForms();
  ASSERT_EQ(1U, forms.size());
  FormData form =
      *form_util::ExtractFormData(forms[0].GetDocument(), forms[0],
                                  *base::MakeRefCounted<FieldDataManager>(),
                                  {form_util::ExtractOption::kValue});
  ASSERT_EQ(form.fields.size(), 1u);
  blink::WebFormControlElement field =
      GetWebElementById("text_id").DynamicTo<blink::WebFormControlElement>();
  ASSERT_FALSE(field.IsNull());

  std::u16string prior_value = form.fields[0].value();
  form.fields[0].set_value(form.fields[0].value() + u"AUTOFILLED");
  form.fields[0].set_is_autofilled(true);

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
      {blink::features::kAutofillDontSetAutofillStateAfterJavaScriptChanges,
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
    EXPECT_LE(GetParam(), 3);
    std::vector<base::test::FeatureRef> features = {
        features::kAutofillUnifyAndFixFormTracking,
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
                         ::testing::Values(0, 1, 2, 3));

// Test that AutofillAgent::JavaScriptChangedValue updates the
// last interacted saved state.
TEST_P(AutofillAgentSubmissionTest,
       JavaScriptChangedValueUpdatesLastInteractedSavedState) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillReplaceFormElementObserver};
  LoadHTML(R"(<form id="form_id"><input id="text_id"></form>)");

  blink::WebFormElement form = GetMainFrame()
                                   ->GetDocument()
                                   .GetElementById("form_id")
                                   .DynamicTo<blink::WebFormElement>();
  FormRendererId form_id = form_util::GetFormRendererId(form);

  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].value = 'js-set value';)");
  std::optional<FormData> provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  // Since we do not have a tracked form yet, the JS call should not update (in
  // this case set) the last interacted form.
  ASSERT_FALSE(provisionally_saved_form.has_value());

  SimulateUserEditField(form, "text_id", "user-set value");
  provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  ASSERT_TRUE(provisionally_saved_form.has_value());
  EXPECT_EQ(provisionally_saved_form->renderer_id, form_id);
  ASSERT_EQ(1u, provisionally_saved_form->fields.size());
  EXPECT_EQ(u"user-set value", provisionally_saved_form->fields[0].value());

  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].value = 'js-set value';)");
  provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  // Since we now have a tracked form and JS modified the same form, we should
  // see the JS modification reflected in the last interacted saved form.
  ASSERT_TRUE(provisionally_saved_form.has_value());
  EXPECT_EQ(provisionally_saved_form->renderer_id, form_id);
  ASSERT_EQ(1u, provisionally_saved_form->fields.size());
  EXPECT_EQ(u"js-set value", provisionally_saved_form->fields[0].value());
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
  ASSERT_FALSE(field.IsNull());

  FormFieldData form_field;
  form_util::WebFormControlElementToFormField(
      blink::WebFormElement(), field, &autofill_agent().field_data_manager(),
      {form_util::ExtractOption::kValue}, &form_field);

  form_field.set_value(u"autofilled");
  form_field.set_is_autofilled(true);

  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
  FormData form;
  form.fields = {form_field};
  autofill_agent().ApplyFieldsAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kFill,
                                     GetFieldsForFilling({form}));
  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kAutofilled);

  std::optional<FormData> provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  ASSERT_TRUE(provisionally_saved_form.has_value());
  ASSERT_EQ(1u, provisionally_saved_form->fields.size());
  EXPECT_EQ(u"autofilled", provisionally_saved_form->fields[0].value());
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
  ASSERT_FALSE(field.IsNull());
  ASSERT_EQ("text_id", field.GetIdAttribute().Ascii());

  FormData form =
      *form_util::ExtractFormData(form_element.GetDocument(), form_element,
                                  *base::MakeRefCounted<FieldDataManager>(),
                                  {form_util::ExtractOption::kValue});

  ASSERT_EQ(1u, form.fields.size());
  form.fields[0].set_value(u"autofilled");
  form.fields[0].set_is_autofilled(true);

  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
  autofill_agent().ApplyFieldsAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kFill,
                                     GetFieldsForFilling({form}));
  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kAutofilled);

  std::optional<FormData> provisionally_saved_form =
      AutofillAgentTestApi(&autofill_agent()).provisionally_saved_form();
  ASSERT_TRUE(provisionally_saved_form.has_value());
  ASSERT_EQ(1u, provisionally_saved_form->fields.size());
  EXPECT_EQ(u"autofilled", provisionally_saved_form->fields[0].value());
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

  SimulateUserEditField(blink::WebFormElement(), "name", "Ariel");
  SimulateUserEditField(blink::WebFormElement(), "address", "Atlantica");

  EXPECT_CALL(autofill_driver(),
              FormSubmitted(AllOf(FieldsAre("id", &FormFieldData::id_attribute,
                                            {u"name", u"address"}),
                                  FieldsAre("value", &FormFieldData::value,
                                            {u"Ariel", u"Atlantica"})),
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

  SimulateUserEditField(blink::WebFormElement(), "name", "Ariel");
  SimulateUserEditField(blink::WebFormElement(), "address", "Atlantica");

  EXPECT_CALL(autofill_driver(),
              FormSubmitted(AllOf(FieldsAre("id", &FormFieldData::id_attribute,
                                            {u"search", u"name", u"address"}),
                                  FieldsAre("value", &FormFieldData::value,
                                            {u"", u"Ariel", u"Atlantica"})),
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

  SimulateUserEditField(blink::WebFormElement(), "name", "Ariel");
  SimulateUserEditField(blink::WebFormElement(), "address", "Atlantica");

  std::vector<std::u16string> expected_id_attributes =
      improved_submission_detection()
          ? std::vector<std::u16string>{u"name", u"address"}
          : std::vector<std::u16string>{u"address"};

  std::vector<std::u16string> expected_values =
      improved_submission_detection()
          ? std::vector<std::u16string>{u"Ariel", u"Atlantica"}
          : std::vector<std::u16string>{u"Atlantica"};

  EXPECT_CALL(autofill_driver(),
              FormSubmitted(AllOf(FieldsAre("id", &FormFieldData::id_attribute,
                                            expected_id_attributes),
                                  FieldsAre("value", &FormFieldData::value,
                                            expected_values)),
                            _, _));

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
  ASSERT_FALSE(form_element.IsNull());
  std::optional<FormData> form =
      form_util::ExtractFormData(GetMainFrame()->GetDocument(), form_element,
                                 autofill_agent().field_data_manager(),
                                 {form_util::ExtractOption::kValue});
  ASSERT_TRUE(form.has_value());

  blink::WebVector<blink::WebFormControlElement> field_elements =
      form_element.GetFormControlElements();

  for (const blink::WebFormControlElement& field_element : field_elements) {
    ASSERT_EQ(field_element.GetAutofillState(),
              blink::WebAutofillState::kNotFilled);
  }

  for (FormFieldData& field : form->fields) {
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
              FormSubmitted(AllOf(FieldsAre("id", &FormFieldData::id_attribute,
                                            {u"input2"}),
                                  FieldsAre("value", &FormFieldData::value,
                                            {u"input2 autofilled"})),
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
      ASSERT_FALSE(GetWebElementById(id).IsNull());
    }
  }

  void FocusedElementChanged(blink::WebElement e) {
    test_api(autofill_agent()).FocusedElementChanged(e);
    task_environment_.RunUntilIdle();
  }

  void FocusedElementChanged(std::string_view id) {
    blink::WebElement e = GetWebElementById(id);
    ASSERT_FALSE(e.IsNull()) << "Field " << id << " doesn't exist";
    FocusedElementChanged(e);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::kAutofillNewFocusEvents};
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
                  FocusOnFormField(_, HasFieldId(GetFieldRendererIdById(id))));
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
                  FocusOnFormField(_, HasFieldId(GetFieldRendererIdById(id))));
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

}  // namespace

}  // namespace autofill
