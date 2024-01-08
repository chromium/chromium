// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Not;

// Returns a matcher that matches a `FormFieldData::id_attribute`.
auto HasFieldIdAttribute(std::u16string id_attribute) {
  return Field("id_attribute", &FormFieldData::id_attribute, id_attribute);
}

// Returns a matcher that matches a `FormFieldData::form_control_type`.
auto HasType(FormControlType type) {
  return Field(&FormFieldData::form_control_type, type);
}

auto IsContentEditable() {
  return HasType(FormControlType::kContentEditable);
}

// TODO(crbug.com/1496382): Clean up these functions once
// `kAutofillAndroidDisableSuggestionsOnJSFocus` is launched and Android and
// Desktop behave identically.

// Returns the expected number of calls to AskForValuesToFill when left
// clicking or tapping a previously unfocused field.
int NumCallsToAskForValuesToFillOnInitialLeftClick() {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    return base::FeatureList::IsEnabled(
               features::kAutofillAndroidDisableSuggestionsOnJSFocus)
               // Called solely by
               // `AutofillAgent::DidReceiveLeftMouseDownOrGestureTapInNode`:
               ? 1
               // Called also by `AutofillAgent::FocusElementChanged`.
               : 2;
  }
  // Called solely by `AutofillAgent::DidCompleteFocusChangeInFrame`.
  return 1;
}

// Returns the expected number of calls to AskForValuesToFill when focusing a
// text field without left clicking or tapping it.
int NumCallsToAskForValuesToFillOnTextfieldFocusWithoutLeftClick() {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    return base::FeatureList::IsEnabled(
               features::kAutofillAndroidDisableSuggestionsOnJSFocus)
               ? 0
               // Called by `AutofillAgent::FocusElementChanged`.
               : 1;
  }
  return 0;
}

}  // namespace

class AutofillAgentFormInteractionTest : public test::AutofillRendererTest {
 public:
  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    web_view_->SetDefaultPageScaleLimits(1, 4);

    LoadHTML(
        "<form>"
        "  <input type='text' id='text_1'></input><br>"
        "  <input type='text' id='text_2' disabled></input><br>"
        "  <textarea  id='textarea_1'></textarea><br>"
        "  <input type='button' id='button'></input><br>"
        "</form>");
    GetWebFrameWidget()->Resize(gfx::Size(500, 500));
    GetWebFrameWidget()->SetFocus(true);

    // Enable show-ime event when element is focused by indicating that a user
    // gesture has been processed since load.
    EXPECT_TRUE(SimulateElementClick("button"));
  }
};

// Tests that (repeatedly) clicking a text input field calls AskForValuesToFill
// each time, but clicking a button does not.
TEST_F(AutofillAgentFormInteractionTest, TextInputLeftClick) {
  MockFunction<void(int)> check;
  {
    InSequence s;
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, HasFieldIdAttribute(u"text_1"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked))
        .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());
    EXPECT_CALL(check, Call(1));
    // The second click only triggers a single call, regardless of OS.
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, HasFieldIdAttribute(u"text_1"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked));
    EXPECT_CALL(check, Call(2));
  }

  EXPECT_TRUE(SimulateElementClickAndWait("text_1"));
  check.Call(1);

  EXPECT_TRUE(SimulateElementClickAndWait("text_1"));
  task_environment_.RunUntilIdle();
  check.Call(2);

  // No notification should be sent on clicking the button.
  EXPECT_TRUE(SimulateElementClickAndWait("button"));
}

// Tests that a right click does not trigger AskForValuesToFill on Desktop, but
// does on Android.
TEST_F(AutofillAgentFormInteractionTest, TextInputRightClick) {
  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, HasFieldIdAttribute(u"text_1"), _,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked))
      .Times(NumCallsToAskForValuesToFillOnTextfieldFocusWithoutLeftClick());
  EXPECT_TRUE(SimulateElementRightClick("text_1"));
}

// Tests that focusing the text field without a click does not call
// AskForValuesToFill on Desktop, but does on Android. A later click interaction
// triggers it on both.
TEST_F(AutofillAgentFormInteractionTest, InputFocusAndLeftClick) {
  MockFunction<void(int)> check;
  {
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, HasFieldIdAttribute(u"text_1"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked))
        .Times(NumCallsToAskForValuesToFillOnTextfieldFocusWithoutLeftClick());
    InSequence s;
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, HasFieldIdAttribute(u"text_1"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked));
    EXPECT_CALL(check, Call(2));
  }

  SimulateElementFocusAndWait("text_1");
  check.Call(1);

  EXPECT_TRUE(SimulateElementClickAndWait("text_1"));
  check.Call(2);
}

// Tests that (repeated) left clicks on a textarea trigger AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, TextAreaLeftClick) {
  MockFunction<void(int)> check;
  {
    InSequence s;
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, HasFieldIdAttribute(u"textarea_1"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked))
        .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, HasFieldIdAttribute(u"textarea_1"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked));
    EXPECT_CALL(check, Call(2));
  }

  EXPECT_TRUE(SimulateElementClickAndWait("textarea_1"));
  check.Call(1);

  EXPECT_TRUE(SimulateElementClickAndWait("textarea_1"));
  check.Call(2);

  EXPECT_TRUE(SimulateElementClickAndWait("button"));
}

// Tests that focusing the text field without a click does not call
// AskForValuesToFill on Desktop, but does on Android. A later click interaction
// triggers it on both.
TEST_F(AutofillAgentFormInteractionTest, TextareaFocusAndLeftClick) {
  MockFunction<void(int)> check;
  {
    InSequence s;
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, HasFieldIdAttribute(u"textarea_1"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked))
        .Times(NumCallsToAskForValuesToFillOnTextfieldFocusWithoutLeftClick());
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, HasFieldIdAttribute(u"textarea_1"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked));
    EXPECT_CALL(check, Call(2));
  }

  SimulateElementFocusAndWait("textarea_1");
  check.Call(1);

  EXPECT_TRUE(SimulateElementClickAndWait("textarea_1"));
  check.Call(2);
}

// Tests that left clicking a scaled text area triggers AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, ScaledTextareaLeftClick) {
  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, HasFieldIdAttribute(u"textarea_1"), _,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked))
      .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());

  web_view_->SetPageScaleFactor(3);
  web_view_->SetVisualViewportOffset(gfx::PointF(50, 50));
  SimulatePointClick(gfx::Point(30, 30));
}

// Tests that tapping a scaled text area triggers AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, ScaledTextareaTapped) {
  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, HasFieldIdAttribute(u"textarea_1"), _,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked))
      .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());

  web_view_->SetPageScaleFactor(3);
  web_view_->SetVisualViewportOffset(gfx::PointF(50, 50));
  SimulateRectTap(gfx::Rect(30, 30, 30, 30));
  task_environment_.RunUntilIdle();
}

// Tests that left clicking a disabled input event does not trigger
// AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, DisabledInputLeftClick) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
  EXPECT_TRUE(SimulateElementClickAndWait("text_2"));
}

// Tests that a tap near the edge of an input field trigger AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, TapNearEdge) {
  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, HasFieldIdAttribute(u"text_1"), _,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked))
      .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());

  gfx::Rect element_bounds = GetElementBounds("text_1");
  SimulateRectTap(element_bounds -
                  gfx::Vector2d(element_bounds.width() / 2 + 1, 0));
}

class AutofillAgentContentEditableInteractionTest
    : public test::AutofillRendererTest {
 public:
  AutofillAgentContentEditableInteractionTest() {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillContentEditables},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Tests that left clicking on an contenteditable triggers AskForValuesToFill.
TEST_F(AutofillAgentContentEditableInteractionTest, LeftClick) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          Field(&FormData::fields, ElementsAre(IsContentEditable())),
          IsContentEditable(), _,
          mojom::AutofillSuggestionTriggerSource::kContentEditableClicked))
      .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());

  LoadHTML("<body><div id=ce contenteditable></body>");
  WaitForFormsSeen();
  SimulateElementClickAndWait("ce");
}

#if !BUILDFLAG(IS_ANDROID)
// Tests that unfocusing a contenteditable triggers a call to
// `AutofillDriver::HidePopup()`.
// The test is not enabled on Android because the keyboard accessory has
// different hiding logic for which `HidePopup` is not called.
TEST_F(AutofillAgentContentEditableInteractionTest,
       LossOfFocusOfContentEditableTriggersHideAutofillPopup) {
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            Field(&FormData::fields, ElementsAre(IsContentEditable())),
            IsContentEditable(), _,
            mojom::AutofillSuggestionTriggerSource::kContentEditableClicked));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(autofill_driver(), HidePopup);
  }

  LoadHTML("<body><div id=ce contenteditable></div>");
  WaitForFormsSeen();
  SimulateElementClickAndWait("ce");
  check.Call();
  ChangeFocusToNull(GetMainFrame()->GetDocument());
}
#endif

// Tests that clicking on a contenteditable form is ignored.
TEST_F(AutofillAgentContentEditableInteractionTest,
       ClickOnContentEditableFormIsIgnored) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);

  LoadHTML("<body><form id=ce contenteditable></form>");
  WaitForFormsSeen();
  SimulateElementClickAndWait("ce");
}

// Tests that clicking on a contenteditable form element triggers
// AskForValuesToFill for the form element and not the contenteditable.
TEST_F(AutofillAgentContentEditableInteractionTest,
       ClickOnContentEditableFormControlIsTreatedAsFormControl) {
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(autofill_driver(), FormsSeen);
    EXPECT_CALL(check, Call);
    EXPECT_CALL(autofill_driver(),
                AskForValuesToFill(_, Not(IsContentEditable()), _, _))
        .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());
  }

  LoadHTML("<body><textarea id=ce contenteditable></textarea>");
  WaitForFormsSeen();
  check.Call();
  SimulateElementClickAndWait("ce");
}

}  // namespace autofill
