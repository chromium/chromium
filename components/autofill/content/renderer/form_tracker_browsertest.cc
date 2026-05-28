// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_tracker.h"

#include <optional>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_tracker_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_type.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::Property;

constexpr CallTimerState kCallTimerStateDummy = {
    .call_site = CallTimerState::CallSite::kUpdateFormCache,
    .last_autofill_agent_reset = {},
    .last_dom_content_loaded = {},
};

template <typename... Args>
auto FieldsAre(Args&&... matchers) {
  return Property("FormData::fields", &FormData::fields,
                  ElementsAre(std::forward<Args>(matchers)...));
}

auto HasFieldIdAttribute(std::u16string id_attribute) {
  return Property("FormFieldData::id_attribute", &FormFieldData::id_attribute,
                  std::move(id_attribute));
}

auto HasFieldName(std::u16string name) {
  return Property("FormFieldData::name", &FormFieldData::name, std::move(name));
}

auto HasValue(std::u16string value) {
  return Property("FormFieldData::value", &FormFieldData::value,
                  std::move(value));
}

class MockFormTracker : public FormTracker {
 public:
  MockFormTracker(content::RenderFrame* render_frame,
                  AutofillAgent& autofill_agent,
                  PasswordAutofillAgent* password_autofill_agent)
      : FormTracker(render_frame, autofill_agent, password_autofill_agent) {
    ON_CALL(*this, FireFormSubmission)
        .WillByDefault([this](mojom::SubmissionSource source,
                              std::optional<blink::WebFormElement> form,
                              bool reset_last_interacted_elements) {
          test_api(*this).FireFormSubmission(source, form,
                                             reset_last_interacted_elements);
        });
    ON_CALL(*this, ElementDisappeared)
        .WillByDefault([this](const blink::WebElement& element) {
          FormTracker::ElementDisappeared(element);
        });
  }
  MOCK_METHOD((void),
              FireFormSubmission,
              (mojom::SubmissionSource,
               std::optional<blink::WebFormElement>,
               bool),
              (override));
  MOCK_METHOD(void,
              ElementDisappeared,
              (const blink::WebElement& element),
              (override));
};

class FormTrackerTest : public test::AutofillRendererTest,
                        public testing::WithParamInterface<int> {
 public:
  FormTrackerTest() {
    EXPECT_LE(GetParam(), 3);
    std::vector<base::test::FeatureRef> features = {
        features::kAutofillFixFormTracking,
        features::kAutofillReplaceCachedWebElementsByRendererIds,
        features::kAutofillReplaceFormElementObserver};

    std::vector<base::test::FeatureRef> enabled_features(
        features.begin(), features.begin() + GetParam());
    std::vector<base::test::FeatureRef> disabled_features(
        features.begin() + GetParam(), features.end());
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    auto tracker = std::make_unique<MockFormTracker>(
        GetMainRenderFrame(), autofill_agent(), password_autofill_agent());
    tracker->SetUserGestureRequired(FormTracker::UserGestureRequired(true));
    test_api(autofill_agent()).set_form_tracker(std::move(tracker));
  }

  MockFormTracker& form_tracker() {
    return static_cast<MockFormTracker&>(
        test_api(autofill_agent()).form_tracker());
  }

  blink::WebFormControlElement GetFormControlById(const std::string& id) {
    return GetMainFrame()
        ->GetDocument()
        .GetElementById(blink::WebString::FromUtf8(id))
        .DynamicTo<blink::WebFormControlElement>();
  }

  std::vector<FormFieldData::FillData> GetFillData(
      base::span<const FormFieldData> fields) {
    return base::ToVector(fields, [](const FormFieldData& field) {
      return FormFieldData::FillData(field);
    });
  }

  void SimulateFillForm(std::string_view fname_id = "fname",
                        std::string_view lname_id = "lname") {
    blink::WebFormControlElement fname_element =
        GetFormControlElementById(fname_id);
    ASSERT_TRUE(fname_element);
    SimulateElementClickAndWait(std::string(fname_id));

    blink::WebFormElement form_element =
        fname_element.GetOwningFormForAutofill();
    FormData form;
    if (!form_element.IsNull()) {
      form = *form_util::ExtractFormData(
          form_element.GetDocument(), form_element,
          *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy,
          /*button_titles_cache=*/nullptr);
    }

    for (FormFieldData& field : test_api(form).fields()) {
      if (field.renderer_id() == form_util::GetFieldRendererId(fname_element)) {
        field.set_value(u"John");
        field.set_is_autofilled_according_to_renderer(true);
      } else if (lname_id != "" &&
                 field.renderer_id() ==
                     form_util::GetFieldRendererId(
                         GetFormControlElementById(lname_id))) {
        field.set_value(u"Smith");
        field.set_is_autofilled_according_to_renderer(true);
      }
    }

    autofill_agent().ApplyFieldsAction(
        mojom::FormActionType::kFill, mojom::ActionPersistence::kFill,
        GetFillData(form.fields()), FillId::Create(),
        /*supports_refill=*/false);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AutofillSubmissionTest,
                         FormTrackerTest,
                         ::testing::Values(0, 1, 2, 3));

// Check that submission is detected on a page with no <form> when in sequence:
// 1) User types into a field.
// 2) Page does an XHR.
// 3) Page hides all of the inputs.
TEST_P(FormTrackerTest, FormlessXHRThenHide) {
  LoadHTML("<!DOCTYPE HTML><input id='input1'/><input id='input2'/>");

  blink::WebFormControlElement input1 = GetFormControlById("input1");

  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  ExecuteJavaScriptForTests("document.getElementById('input1').focus();");
  form_tracker().TextFieldValueChanged(input1);

  task_environment_.RunUntilIdle();

  form_tracker().AjaxSucceeded();
  task_environment_.RunUntilIdle();
  // FormTracker should not think there is a submission because the <input>s are
  // still visible.

  // FormTracker should detect a submission after the <input>s are hidden.
  EXPECT_CALL(form_tracker(),
              FireFormSubmission(mojom::SubmissionSource::XHR_SUCCEEDED, _, _))
      .Times(1);
  ExecuteJavaScriptForTests(
      R"(document.getElementById('input1').style.display = 'none';
         document.getElementById('input2').style.display = 'none';)");
  ForceLayoutUpdate();
}

// Check that submission is detected on a page with no <form> when in sequence:
// 1) User types into a field.
// 2) Page hides all of the inputs.
// 3) Page does an XHR.
TEST_P(FormTrackerTest, FormlessHideThenXhr) {
  LoadHTML("<!DOCTYPE HTML><input id='input1'/><input id='input2'/>");

  blink::WebFormControlElement input1 = GetFormControlById("input1");

  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  ExecuteJavaScriptForTests("document.getElementById('input1').focus();");
  form_tracker().TextFieldValueChanged(input1);
  task_environment_.RunUntilIdle();

  ExecuteJavaScriptForTests(
      "document.getElementById('input1').style.display = 'none';"
      "document.getElementById('input2').style.display = 'none';");
  ForceLayoutUpdate();
  task_environment_.RunUntilIdle();
  // FormTracker should not think there is a submission because the page has not
  // done any XHRs.

  // FormTracker should detect a submission when the XHR succeeds.
  EXPECT_CALL(form_tracker(),
              FireFormSubmission(mojom::SubmissionSource::XHR_SUCCEEDED, _, _))
      .Times(1);
  form_tracker().AjaxSucceeded();
  task_environment_.RunUntilIdle();
}

// Check that if a SelectControlSelectionChanged() is called asynchronously
// after a navigation, the event is a dead end.
TEST_P(FormTrackerTest, IgnoreSelectChangeInOldDocument) {
  LoadHTML(R"(<!DOCTYPE HTML>
    <select id=select>
    <option value=0>0</option>
    <option value=1>1</option>
    <option value=2>2</option>
    <option value=3>3</option>
    </select>)");
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);

  EXPECT_CALL(autofill_driver(), SelectControlSelectionChanged);
  ExecuteJavaScriptForTests("document.getElementById('select').value = '1';");
  task_environment_.RunUntilIdle();

  EXPECT_CALL(autofill_driver(), SelectControlSelectionChanged);
  ExecuteJavaScriptForTests("document.getElementById('select').value = '2';");
  task_environment_.RunUntilIdle();

  EXPECT_CALL(autofill_driver(), SelectControlSelectionChanged).Times(0);
  ExecuteJavaScriptForTests("document.getElementById('select').value = '3';");
  LoadHTML(R"(<!DOCTYPE HTML><input>)");  // Turns the event into a no-op.
  task_environment_.RunUntilIdle();
}

// Tests that a submission is fired upon starting a navigation resulting from
// `kWebNavigationTypeOther`.
TEST_P(FormTrackerTest, ProbablyFormSubmitted) {
  LoadHTML("<!DOCTYPE HTML><input id='input1'/>");
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);

  ExecuteJavaScriptForTests("document.getElementById('input1').focus();");
  ExecuteJavaScriptForTests("document.getElementById('input1').value = '1';");
  blink::WebFormControlElement input1 = GetFormControlById("input1");
  form_tracker().TextFieldValueChanged(input1);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(form_tracker(),
              FireFormSubmission(
                  mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED, _, _))
      .Times(1);

  test_api(form_tracker())
      .DidStartNavigation(blink::WebNavigationType::kWebNavigationTypeOther);
}

// Tests that a submission is not fired upon starting a navigation resulting
// from an uninteresting `WebNavigationType`.
TEST_P(FormTrackerTest, ProbablyFormSubmitted_IgnoreUninterestingNavigations) {
  using enum blink::WebNavigationType;
  constexpr auto kUninterestingNavigationTypes = std::to_array({
      kWebNavigationTypeLinkClicked,
      kWebNavigationTypeFormSubmitted,
      kWebNavigationTypeBackForward,
      kWebNavigationTypeReload,
      kWebNavigationTypeFormResubmittedBackForward,
      kWebNavigationTypeFormResubmittedReload,
      kWebNavigationTypeRestore,
  });

  LoadHTML("<!DOCTYPE HTML><input id='input1'/>");
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);

  ExecuteJavaScriptForTests("document.getElementById('input1').focus();");
  ExecuteJavaScriptForTests("document.getElementById('input1').value = '1';");
  blink::WebFormControlElement input1 = GetFormControlById("input1");
  form_tracker().TextFieldValueChanged(input1);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(form_tracker(), FireFormSubmission).Times(0);

  if (base::FeatureList::IsEnabled(features::kAutofillFixFormTracking)) {
    for (const blink::WebNavigationType navigation_type :
         kUninterestingNavigationTypes) {
      test_api(form_tracker()).DidStartNavigation(navigation_type);
    }
  } else {
    test_api(form_tracker()).DidStartNavigation(kWebNavigationTypeLinkClicked);
  }
}

// Tests that AutofillAgent::JavaScriptChangedValue updates the last interacted
// saved state.
TEST_P(FormTrackerTest, JavaScriptChangedValueUpdatesLastInteractedSavedState) {
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
      test_api(form_tracker()).provisionally_saved_form();
  // Since we do not have a tracked form yet, the JS call should not update (in
  // this case set) the last interacted form.
  ASSERT_FALSE(provisionally_saved_form.has_value());

  SimulateUserInputChangeForElementById("text_id", "user_set_value");
  provisionally_saved_form =
      test_api(form_tracker()).provisionally_saved_form();
  ASSERT_TRUE(provisionally_saved_form.has_value());
  EXPECT_EQ(provisionally_saved_form->renderer_id(), form_id);
  ASSERT_EQ(1u, provisionally_saved_form->fields().size());
  EXPECT_EQ(u"user_set_value", provisionally_saved_form->fields()[0].value());

  ExecuteJavaScriptForTests(
      R"(document.forms[0].elements[0].value = 'js_set_value';)");
  provisionally_saved_form =
      test_api(form_tracker()).provisionally_saved_form();
  // Since we now have a tracked form and JS modified the same form, we should
  // see the JS modification reflected in the last interacted saved form.
  ASSERT_TRUE(provisionally_saved_form.has_value());
  EXPECT_EQ(provisionally_saved_form->renderer_id(), form_id);
  ASSERT_EQ(1u, provisionally_saved_form->fields().size());
  EXPECT_EQ(u"js_set_value", provisionally_saved_form->fields()[0].value());
  EXPECT_EQ(u"user_set_value",
            provisionally_saved_form->fields()[0].user_input());
}

// Tests that AutofillAgent::ApplyFormAction(mojom::ActionPersistence::kFill)
// updates the last interacted saved state when the <input>s have no containing
// <form>.
TEST_P(FormTrackerTest,
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
      &form_field);

  form_field.set_value(u"autofilled");
  form_field.set_is_autofilled_according_to_renderer(true);

  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
  FormData form;
  form.set_fields({form_field});
  autofill_agent().ApplyFieldsAction(
      mojom::FormActionType::kFill, mojom::ActionPersistence::kFill,
      GetFillData(form.fields()), FillId::Create(),
      /*supports_refill=*/false);
  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kAutofilled);

  std::optional<FormData> provisionally_saved_form =
      test_api(form_tracker()).provisionally_saved_form();
  ASSERT_TRUE(provisionally_saved_form.has_value());
  ASSERT_EQ(1u, provisionally_saved_form->fields().size());
  EXPECT_EQ(u"autofilled", provisionally_saved_form->fields()[0].value());
}

// Tests that AutofillAgent::ApplyFormAction(mojom::ActionPersistence::kFill)
// updates the last interacted saved state when the <input>s have a containing
// <form>.
TEST_P(FormTrackerTest, FormApplyFormActionUpdatesLastInteractedSavedState) {
  LoadHTML(R"(
    <form id="form_id">
      <input id="text_id">
    </form>
  )");

  blink::WebFormElement form_element =
      GetWebElementById("form_id").DynamicTo<blink::WebFormElement>();
  std::vector<blink::WebFormControlElement> fields =
      form_util::GetOwnedFormControlsForTesting(form_element.GetDocument(),
                                                form_element);
  ASSERT_EQ(1u, fields.size());
  blink::WebFormControlElement field = fields[0];

  ASSERT_TRUE(field);
  ASSERT_EQ("text_id", field.GetIdAttribute().Ascii());

  FormData form = *form_util::ExtractFormData(
      form_element.GetDocument(), form_element,
      *base::MakeRefCounted<FieldDataManager>(), kCallTimerStateDummy,
      /*button_titles_cache=*/nullptr);

  ASSERT_EQ(1u, form.fields().size());
  test_api(form).field(0).set_value(u"autofilled");
  test_api(form).field(0).set_is_autofilled_according_to_renderer(true);

  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kNotFilled);
  autofill_agent().ApplyFieldsAction(
      mojom::FormActionType::kFill, mojom::ActionPersistence::kFill,
      GetFillData(form.fields()), FillId::Create(),
      /*supports_refill=*/false);
  ASSERT_EQ(field.GetAutofillState(), blink::WebAutofillState::kAutofilled);

  std::optional<FormData> provisionally_saved_form =
      test_api(form_tracker()).provisionally_saved_form();
  ASSERT_TRUE(provisionally_saved_form.has_value());
  ASSERT_EQ(1u, provisionally_saved_form->fields().size());
  EXPECT_EQ(u"autofilled", provisionally_saved_form->fields()[0].value());
}

// Tests that hiding an element via display: none triggers ElementDisappeared
// on FormTracker.
TEST_P(FormTrackerTest, HideElementTriggersFormTracker_DisplayNone) {
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

// Tests that hiding an element via visibility: hidden triggers
// ElementDisappeared on FormTracker.
TEST_P(FormTrackerTest, HideElementTriggersFormTracker_VisibilityHidden) {
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

// Tests that changing an input type to hidden triggers ElementDisappeared on
// FormTracker.
TEST_P(FormTrackerTest, HideElementTriggersFormTracker_TypeHidden) {
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

// Tests that setting the hidden attribute to true triggers ElementDisappeared
// on FormTracker.
TEST_P(FormTrackerTest, HideElementTriggersFormTracker_HiddenTrue) {
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

// Tests that moving an element to an unassigned slot in Shadow DOM triggers
// ElementDisappeared on FormTracker.
TEST_P(FormTrackerTest, HideElementTriggersFormTracker_ShadowDom) {
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

// Tests that an inferred form submission as a result of a page deleting ALL of
// the <input>s (that the user has edited) on a page with no <form> sends the
// contents of all of the fields to the browser.
TEST_P(FormTrackerTest,
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
                  _));

  // Simulate inferred form submission as a result the focused field being
  // removed after an AJAX call.
  ExecuteJavaScriptForTests(
      R"(document.getElementById('shipping').innerHTML = '')");
  form_tracker().AjaxSucceeded();
}

// Tests that an inferred form submission as a result of a page deleting ALL of
// the <input>s that the user has edited but NOT ALL of the <inputs> on the page
// sends the user-edited <inputs> to the browser.
TEST_P(FormTrackerTest,
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
                            _));

  // Simulate inferred form submission as a result the focused field being
  // removed after an AJAX call.
  ExecuteJavaScriptForTests(R"(document.getElementById('shipping').remove();)");
  form_tracker().AjaxSucceeded();
}

// Tests that edited inputs are sent to the browser when navigation occurs after
// some inputs are removed on a formless page.
TEST_P(FormTrackerTest, FormlessOnNavigationAfterSomeInputsRemoved) {
  LoadHTML(R"(
    Name: <input type='text' id='name'><br>
    Address: <input type='text' id='address'>
  )");

  SimulateUserInputChangeForElementById("name", "Ariel");
  SimulateUserInputChangeForElementById("address", "Atlantica");

  EXPECT_CALL(autofill_driver(),
              FormSubmitted(
                  AllOf(FieldsAre(HasFieldIdAttribute(u"name"),
                                  HasFieldIdAttribute(u"address")),
                        FieldsAre(HasValue(u"Ariel"), HasValue(u"Atlantica"))),
                  _));

  // Remove element that the user did not interact with last.
  ExecuteJavaScriptForTests(R"(document.getElementById('name').remove();)");
  // Simulate page navigation.
  test_api(form_tracker())
      .DidStartNavigation(blink::WebNavigationType::kWebNavigationTypeOther);
}

// Tests that inferred form submission does not send fields that were removed
// from the DOM hierarchy at autofill time, even if the removed element was the
// last queried element.
TEST_P(FormTrackerTest,
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
      kCallTimerStateDummy, /*button_titles_cache=*/nullptr);
  ASSERT_TRUE(form.has_value());

  std::vector<blink::WebFormControlElement> field_elements =
      form_util::GetOwnedFormControlsForTesting(form_element.GetDocument(),
                                                form_element);

  for (const blink::WebFormControlElement& field_element : field_elements) {
    ASSERT_EQ(field_element.GetAutofillState(),
              blink::WebAutofillState::kNotFilled);
  }

  for (FormFieldData& field : test_api(*form).fields()) {
    field.set_value(field.id_attribute() + u" autofilled");
    field.set_is_autofilled_according_to_renderer(true);
  }

  // Update `AutofillAgent::last_queried_element_`.
  static_cast<content::RenderFrameObserver*>(&autofill_agent())
      ->FocusedElementChanged(field_elements[0]);

  autofill_agent().ApplyFieldsAction(
      mojom::FormActionType::kFill, mojom::ActionPersistence::kFill,
      GetFillData(form->fields()), FillId::Create(),
      /*supports_refill=*/false);

  for (const blink::WebFormControlElement& field_element : field_elements) {
    ASSERT_EQ(field_element.GetAutofillState(),
              blink::WebAutofillState::kAutofilled);
  }

  EXPECT_CALL(autofill_driver(),
              FormSubmitted(AllOf(FieldsAre(HasFieldIdAttribute(u"input2")),
                                  FieldsAre(HasValue(u"input2 autofilled"))),
                            _));
  ExecuteJavaScriptForTests(R"(document.getElementById('form').remove();)");
  form_tracker().AjaxSucceeded();
}

TEST_P(FormTrackerTest, NormalFormSubmit) {
  LoadHTML(R"(
      <html>
        <form id='myForm' action='about:blank'>
          <input name='fname' id='fname'/>
          <input name='lname' id='lname' value='Deckard'/>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Deckard")))),
          mojom::SubmissionSource::FORM_SUBMISSION))
      .WillOnce([&]() { run_loop.Quit(); });
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  run_loop.Run();
}

// Tests that FormSubmitted message is generated even the submit event isn't
// propagated by Javascript.
TEST_P(FormTrackerTest, SubmitEventPrevented) {
  LoadHTML(R"(
      <html>
        <form id='myForm'>
          <input name='fname' id='fname'/>
          <input name='lname' id='lname' value='Deckard'/>
          <input type=submit>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Deckard")))),
          mojom::SubmissionSource::FORM_SUBMISSION))
      .WillOnce([&]() { run_loop.Quit(); });
  ExecuteJavaScriptForTests(
      "var form = document.forms[0];"
      "form.onsubmit = function(event) { event.preventDefault(); };"
      "document.querySelector('input[type=submit]').click();");
  run_loop.Run();
}

// Tests that having the form disappear after autofilling triggers submission
// from Autofill's point of view.
TEST_P(FormTrackerTest, DomMutationAfterAutofill) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillAcceptDomMutationAfterAutofillSubmission};
  LoadHTML(R"(
      <html>
        <form id='myForm' action='http://example.com/blade.php'>
          <input name='fname' id='fname'/>
          <input name='lname' id='lname'/>
        </form>
      </html>)");
  SimulateFillForm();

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"John")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Smith")))),
          mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL))
      .WillOnce([&]() { run_loop.Quit(); });
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");
  run_loop.Run();
}

// Tests that completing an Ajax request and having the form disappear will
// trigger submission from Autofill's point of view.
TEST_P(FormTrackerTest, AjaxSucceeded_NoLongerVisible) {
  LoadHTML(R"(
      <html>
        <form id='myForm' action='http://example.com/blade.php'>
          <input name='fname' id='fname' value='Bob'/>
          <input name='lname' id='lname' value='Deckard'/>
          <input type=submit>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Deckard")))),
          mojom::SubmissionSource::XHR_SUCCEEDED))
      .WillOnce([&]() { run_loop.Quit(); });
  form_tracker().AjaxSucceeded();
  run_loop.Run();
}

// Tests that completing an Ajax request and having the form with a specific
// action disappear will trigger submission from Autofill's point of view, even
// if there is another form with the same data but different action on the page.
TEST_P(FormTrackerTest,
       AjaxSucceeded_NoLongerVisible_DifferentActionsSameData) {
  LoadHTML(R"(
      <html>
        <form id='myForm' action='http://example.com/blade.php'>
          <input name='fname' id='fname' value='Bob'/>
          <input name='lname' id='lname' value='Deckard'/>
          <input type=submit>
        </form>
        <form id='myForm2' action='http://example.com/runner.php'>
          <input name='fname' id='fname2' value='Bob'/>
          <input name='lname' id='lname2' value='Deckard'/>
          <input type=submit>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Deckard")))),
          mojom::SubmissionSource::XHR_SUCCEEDED))
      .WillOnce([&]() { run_loop.Quit(); });
  form_tracker().AjaxSucceeded();
  run_loop.Run();
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_NoLongerVisibleBothNoActions DISABLED_NoLongerVisibleBothNoActions
#else
#define MAYBE_NoLongerVisibleBothNoActions NoLongerVisibleBothNoActions
#endif
// Tests that completing an Ajax request and having the form with no action
// specified disappear will trigger submission from Autofill's point of view,
// even if there is still another form with no action in the page. It will
// compare field data within the forms.
TEST_P(FormTrackerTest, MAYBE_NoLongerVisibleBothNoActions) {
  LoadHTML(R"(
      <html>
        <form id='myForm'>
          <input name='fname' id='fname' value='Bob'/>
          <input name='lname' id='lname' value='Deckard'/>
          <input type=submit>
        </form>
        <form id='myForm2'>
          <input name='fname' id='fname2' value='John'/>
          <input name='lname' id='lname2' value='Doe'/>
          <input type=submit>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Deckard")))),
          mojom::SubmissionSource::XHR_SUCCEEDED))
      .WillOnce([&]() { run_loop.Quit(); });
  form_tracker().AjaxSucceeded();
  run_loop.Run();
}

// Tests that completing an Ajax request and having the form with no action
// specified disappear will trigger submission from Autofill's point of view.
TEST_P(FormTrackerTest, AjaxSucceeded_NoLongerVisible_NoAction) {
  LoadHTML(R"(
      <html>
        <form id='myForm'>
          <input name='fname' id='fname' value='Bob'/>
          <input name='lname' id='lname' value='Deckard'/>
          <input type=submit>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Deckard")))),
          mojom::SubmissionSource::XHR_SUCCEEDED))
      .WillOnce([&]() { run_loop.Quit(); });
  form_tracker().AjaxSucceeded();
  run_loop.Run();
}

// Tests that completing an Ajax request but leaving a form visible will not
// trigger submission from Autofill's point of view.
TEST_P(FormTrackerTest, AjaxSucceeded_StillVisible) {
  LoadHTML(R"(
      <html>
        <form id='myForm' action='http://example.com/blade.php'>
          <input name='fname' id='fname' value='Bob'/>
          <input name='lname' id='lname' value='Deckard'/>
          <input type=submit>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  EXPECT_CALL(autofill_driver(), FormSubmitted).Times(0);
  base::RunLoop run_loop;
  form_tracker().AjaxSucceeded();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that completing an Ajax request without any prior form interaction
// does not trigger form submission from Autofill's point of view.
TEST_P(FormTrackerTest, AjaxSucceeded_NoFormInteractionInvisible) {
  LoadHTML(R"(
      <html>
        <form id='myForm' action='http://example.com/blade.php'>
          <input name='fname' id='fname' value='Bob'/>
          <input name='lname' id='lname' value='Deckard'/>
          <input type=submit>
        </form>
      </html>)");

  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  EXPECT_CALL(autofill_driver(), FormSubmitted).Times(0);
  base::RunLoop run_loop;
  form_tracker().AjaxSucceeded();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that completing an Ajax request after having autofilled a form,
// with the form disappearing, will trigger submission from Autofill's
// point of view.
TEST_P(FormTrackerTest, AjaxSucceeded_FilledFormIsInvisible) {
  LoadHTML(R"(
      <html>
        <form id='myForm' action='http://example.com/blade.php'>
          <input name='fname' id='fname'/>
          <input name='lname' id='lname'/>
        </form>
      </html>)");
  SimulateFillForm();
  SimulateUserInputChangeForElementById("fname", "Rick");

  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Smith")))),
          mojom::SubmissionSource::XHR_SUCCEEDED))
      .WillOnce([&]() { run_loop.Quit(); });
  form_tracker().AjaxSucceeded();
  run_loop.Run();
}

// Tests that completing an Ajax request after having autofilled a form,
// without the form disappearing, will not trigger submission from Autofill's
// point of view.
TEST_P(FormTrackerTest, AjaxSucceeded_FilledFormStillVisible) {
  LoadHTML(R"(
      <html>
        <form id='myForm' action='http://example.com/blade.php'>
          <input name='fname' id='fname' value='Rick'/>
          <input name='lname' id='lname' value='Deckard'/>
        </form>
      </html>)");
  SimulateFillForm();

  EXPECT_CALL(autofill_driver(), FormSubmitted).Times(0);
  base::RunLoop run_loop;
  form_tracker().AjaxSucceeded();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that completing an Ajax request without a form present will still
// trigger submission, if all the inputs the user has modified disappear.
TEST_P(FormTrackerTest, AjaxSucceeded_FormlessElements) {
  LoadHTML(R"(
      <head>
        <title>Checkout</title>
      </head>
      <input type='text' name='fname' id='fname'/>
      <input type='text' name='lname' id='lname' value='Puckett'/>
      <input type='number' name='number' id='number' value='34'/>)");
  SimulateUserInputChangeForElementById("fname", "Kirby");

  ExecuteJavaScriptForTests(
      "var element = document.getElementById('fname');"
      "element.style.display = 'none';");
  ForceLayoutUpdate();

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Kirby")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Puckett")),
                          AllOf(HasFieldName(u"number"), HasValue(u"34")))),
          mojom::SubmissionSource::XHR_SUCCEEDED))
      .WillOnce([&]() { run_loop.Quit(); });
  form_tracker().AjaxSucceeded();
  run_loop.Run();
}

// Tests that submitting a form that has autocomplete="off" generates
// WillSubmitForm and FormSubmitted messages.
TEST_P(FormTrackerTest, AutoCompleteOffFormSubmit) {
  LoadHTML(R"(
      <html>
        <form id='myForm' autocomplete='off' action='about:blank'>
          <input name='fname' id='fname'/>
          <input name='lname' id='lname' value='Deckard'/>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Deckard")))),
          mojom::SubmissionSource::FORM_SUBMISSION))
      .WillOnce([&]() { run_loop.Quit(); });
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  run_loop.Run();
}

// Tests that fields with autocomplete off are submitted.
TEST_P(FormTrackerTest, AutoCompleteOffInputSubmit) {
  LoadHTML(R"(
      <html>
        <form id='myForm' action='about:blank'>
          <input name='fname' id='fname'/>
          <input name='lname' id='lname' value='Deckard' autocomplete='off'/>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Deckard")))),
          mojom::SubmissionSource::FORM_SUBMISSION))
      .WillOnce([&]() { run_loop.Quit(); });
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  run_loop.Run();
}

// Tests that submitting a form that has been dynamically set as autocomplete
// off generates WillSubmitForm and FormSubmitted messages.
TEST_P(FormTrackerTest, DynamicAutoCompleteOffFormSubmit) {
  LoadHTML(R"(
      <html>
        <form id='myForm' action='about:blank'>
          <input name='fname' id='fname'/>
          <input name='lname' id='lname' value='Deckard'/>
        </form>
      </html>)");
  SimulateUserInputChangeForElementById("fname", "Rick");

  blink::WebFormElement form =
      GetWebElementById("myForm").DynamicTo<blink::WebFormElement>();
  ASSERT_TRUE(form);
  EXPECT_TRUE(form.AutoComplete());

  ExecuteJavaScriptForTests(
      "document.getElementById('myForm')."
      "setAttribute('autocomplete', 'off');");
  ASSERT_TRUE(base::test::RunUntil([&]() { return !form.AutoComplete(); }));

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"fname"), HasValue(u"Rick")),
                          AllOf(HasFieldName(u"lname"), HasValue(u"Deckard")))),
          mojom::SubmissionSource::FORM_SUBMISSION))
      .WillOnce([&]() { run_loop.Quit(); });
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  run_loop.Run();
}

// Tests that hiding a formless element after an AJAX request triggers form
// submission with XHR_SUCCEEDED source.
TEST_P(FormTrackerTest, FormSubmittedByDOMMutationAfterXHR) {
  LoadHTML(R"(
      <html>
        <input type='text' id='address_field' name='address' autocomplete='on'>
      </html>)");
  SimulateUserInputChangeForElementById("address_field", "City");
  form_tracker().AjaxSucceeded();

  std::string hide_elements =
      "var address = document.getElementById('address_field');"
      "address.style = 'display:none';";
  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"address"), HasValue(u"City")))),
          mojom::SubmissionSource::XHR_SUCCEEDED))
      .WillOnce([&]() { run_loop.Quit(); });
  ExecuteJavaScriptForTests(hide_elements.c_str());
  ForceLayoutUpdate();
  run_loop.Run();
}

// Tests that same document navigation after hiding a modified formless element
// triggers form submission with SAME_DOCUMENT_NAVIGATION source.
TEST_P(FormTrackerTest, FormSubmittedBySameDocumentNavigation) {
  LoadHTML(R"(
      <html>
        <input type='text' id='address_field' name='address' autocomplete='on'>
      </html>)");
  SimulateUserInputChangeForElementById("address_field", "City");

  std::string hide_elements =
      "var address = document.getElementById('address_field');"
      "address.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"address"), HasValue(u"City")))),
          mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION))
      .WillOnce([&]() { run_loop.Quit(); });
  test_api(form_tracker()).DidFinishSameDocumentNavigation();
  run_loop.Run();
}

// Tests that starting a navigation after hiding a modified formless element
// triggers form submission with PROBABLY_FORM_SUBMITTED source.
TEST_P(FormTrackerTest, FormSubmittedByProbablyFormSubmitted) {
  LoadHTML(R"(
      <html>
        <input type='text' id='address_field' name='address' autocomplete='on'>
      </html>)");
  SimulateUserInputChangeForElementById("address_field", "City");

  std::string hide_elements =
      "var address = document.getElementById('address_field');"
      "address.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());

  base::RunLoop run_loop;
  EXPECT_CALL(
      autofill_driver(),
      FormSubmitted(
          AllOf(FieldsAre(AllOf(HasFieldName(u"address"), HasValue(u"City")))),
          mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED))
      .WillOnce([&]() { run_loop.Quit(); });
  test_api(form_tracker()).DidStartNavigation(blink::kWebNavigationTypeOther);
  run_loop.Run();
}

}  // namespace
}  // namespace autofill
