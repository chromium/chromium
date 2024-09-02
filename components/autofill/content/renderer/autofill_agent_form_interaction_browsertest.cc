// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/feature_list.h"
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
#include "ui/gfx/geometry/rect.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::Property;

// Returns a matcher that matches a `FormFieldData::form_control_type`.
auto HasType(FormControlType type) {
  return Property("FormFieldData::form_control_type",
                  &FormFieldData::form_control_type, type);
}

auto IsContentEditable() {
  return HasType(FormControlType::kContentEditable);
}

template <typename... Args>
auto FieldsAre(Args&&... matchers) {
  return Property("FormData::fields", &FormData::fields,
                  ElementsAre(std::forward<Args>(matchers)...));
}

// TODO(crbug.com/40286775): Clean up these functions once
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

// Returns the expected number of calls to HidePopup when unfocusing a field.
int NumCallsToHidePopupOnFocusLoss() {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    return 0;  // The accessory allows to fill on focus loss.
  }
  return 1;  // Any dropdown should disappear on focus loss.
}

class AutofillAgentFormInteractionTest : public test::AutofillRendererTest {
 public:
  void SetUp() override {
    // TODO(crbug.com/41268731): parameterize tests over AutofillAgent::Config.
    test::AutofillRendererTest::SetUp();
    web_view_->SetDefaultPageScaleLimits(1, 4);

    LoadHTML(
        "<form>"
        "  <input type='text' id='text'></input><br>"
        "  <input type='text' id='text_disabled' disabled></input><br>"
        "  <input type='text' id='text_readonly' readonly></input><br>"
        "  <textarea  id='textarea'></textarea><br>"
        "  <textarea  id='textarea_disabled' disabled></textarea><br>"
        "  <textarea  id='textarea_readonly' readonly></textarea><br>"
        "  <input type='button' id='button'></input><br>"
        "</form>");
    GetWebFrameWidget()->Resize(gfx::Size(500, 500));
    GetWebFrameWidget()->SetFocus(true);

    // Enable show-ime event when element is focused by indicating that a user
    // gesture has been processed since load.
    EXPECT_TRUE(SimulateElementClick("button"));
  }

  FieldRendererId GetFieldRendererIdById(std::string_view id) {
    return form_util::GetFieldRendererId(
        GetMainFrame()->GetDocument().GetElementById(
            blink::WebString::FromUTF8(id)));
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
            _, GetFieldRendererIdById("text"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked))
        .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());
    EXPECT_CALL(check, Call(1));
    // The second click only triggers a single call, regardless of OS.
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, GetFieldRendererIdById("text"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked));
    EXPECT_CALL(check, Call(2));
  }

  EXPECT_TRUE(SimulateElementClickAndWait("text"));
  check.Call(1);

  EXPECT_TRUE(SimulateElementClickAndWait("text"));
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
                  _, GetFieldRendererIdById("text"), _,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked))
      .Times(NumCallsToAskForValuesToFillOnTextfieldFocusWithoutLeftClick());
  EXPECT_TRUE(SimulateElementRightClick("text"));
}

// Tests that focusing the text field without a click does not call
// AskForValuesToFill on Desktop, but does on Android. A later click interaction
// triggers it on both.
TEST_F(AutofillAgentFormInteractionTest, TextInputFocusAndLeftClick) {
  MockFunction<void(int)> check;
  {
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, GetFieldRendererIdById("text"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked))
        .Times(NumCallsToAskForValuesToFillOnTextfieldFocusWithoutLeftClick());
    InSequence s;
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, GetFieldRendererIdById("text"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked));
    EXPECT_CALL(check, Call(2));
  }

  SimulateElementFocusAndWait("text");
  check.Call(1);

  EXPECT_TRUE(SimulateElementClickAndWait("text"));
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
            _, GetFieldRendererIdById("textarea"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked))
        .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            _, GetFieldRendererIdById("textarea"), _,
            AutofillSuggestionTriggerSource::kFormControlElementClicked));
    EXPECT_CALL(check, Call(2));
  }

  EXPECT_TRUE(SimulateElementClickAndWait("textarea"));
  check.Call(1);

  EXPECT_TRUE(SimulateElementClickAndWait("textarea"));
  check.Call(2);

  EXPECT_TRUE(SimulateElementClickAndWait("button"));
}

// Tests that focusing a textarea without a click calls AskForValuesToFill with
// trigger source `kTextareaFocusedWithoutClick` on Desktop.
TEST_F(AutofillAgentFormInteractionTest, TextareaFocusAndLeftClick) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAndroidDisableSuggestionsOnJSFocus};
  MockFunction<void(int)> check;
  {
    InSequence s;
    using enum AutofillSuggestionTriggerSource;
    if constexpr (!BUILDFLAG(IS_ANDROID)) {
      EXPECT_CALL(autofill_driver(),
                  AskForValuesToFill(_, GetFieldRendererIdById("textarea"), _,
                                     kTextareaFocusedWithoutClick));
    } else {
      EXPECT_CALL(autofill_driver(),
                  AskForValuesToFill(_, GetFieldRendererIdById("textarea"), _,
                                     kFormControlElementClicked))
          .Times(
              NumCallsToAskForValuesToFillOnTextfieldFocusWithoutLeftClick());
    }
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(autofill_driver(),
                AskForValuesToFill(_, GetFieldRendererIdById("textarea"), _,
                                   kFormControlElementClicked));
    EXPECT_CALL(check, Call(2));
  }

  SimulateElementFocusAndWait("textarea");
  check.Call(1);

  EXPECT_TRUE(SimulateElementClickAndWait("textarea"));
  check.Call(2);
}

// Tests that left clicking a scaled text area triggers AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, ScaledTextareaLeftClick) {
  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, GetFieldRendererIdById("textarea"), _,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked))
      .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());

  web_view_->SetPageScaleFactor(3);
  web_view_->SetVisualViewportOffset(gfx::PointF(50, 50));
  SimulatePointClick(GetElementBounds("textarea").CenterPoint());
}

// Tests that tapping a scaled text area triggers AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, ScaledTextareaTapped) {
  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, GetFieldRendererIdById("textarea"), _,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked))
      .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());

  web_view_->SetPageScaleFactor(3);
  web_view_->SetVisualViewportOffset(gfx::PointF(50, 50));
  gfx::Point center = GetElementBounds("textarea").CenterPoint();
  SimulateRectTap(gfx::Rect(center, gfx::Size(30, 30)));
  task_environment_.RunUntilIdle();
}

// Tests that left clicking a disabled input field does not trigger
// AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, DisabledInputLeftClick) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
  EXPECT_TRUE(SimulateElementClickAndWait("text_disabled"));
}

// Tests that focusing a disabled input field does not trigger
// AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, DisabledInputFocusWithoutClick) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
  SimulateElementFocusAndWait("text_disabled");
}

// Tests that left clicking a disabled textarea does not trigger
// AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, DisabledTextareaLeftClick) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
  EXPECT_TRUE(SimulateElementClickAndWait("textarea_disabled"));
}

// Tests that focusing a disabled textarea without click does not trigger
// AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, DisabledTextareaFocusWithoutClick) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
  SimulateElementFocusAndWait("textarea_disabled");
}

// Tests that left clicking a readonly input field does not trigger
// AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, ReadonlyInputLeftClick) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
  EXPECT_TRUE(SimulateElementClickAndWait("text_readonly"));
}

// Tests that focusing a readonly input field does not trigger
// AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, ReadonlyInputFocusWithoutClick) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
  SimulateElementFocusAndWait("text_readonly");
}

// Tests that left clicking a readonly textarea does not trigger
// AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, ReadonlyTextareaLeftClick) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
  EXPECT_TRUE(SimulateElementClickAndWait("textarea_readonly"));
}

// Tests that focusing a readonly textarea without click does not trigger
// AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, ReadonlyTextareaFocusWithoutClick) {
  EXPECT_CALL(autofill_driver(), AskForValuesToFill).Times(0);
  SimulateElementFocusAndWait("textarea_readonly");
}

// Tests that a tap near the edge of an input field trigger AskForValuesToFill.
TEST_F(AutofillAgentFormInteractionTest, TapNearEdge) {
  EXPECT_CALL(autofill_driver(),
              AskForValuesToFill(
                  _, GetFieldRendererIdById("text"), _,
                  AutofillSuggestionTriggerSource::kFormControlElementClicked))
      .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());

  gfx::Rect element_bounds = GetElementBounds("text");
  SimulateRectTap(element_bounds -
                  gfx::Vector2d(element_bounds.width() / 2 + 1, 0));
}

using AutofillAgentContentEditableInteractionTest = test::AutofillRendererTest;

// Tests that left clicking on an contenteditable triggers AskForValuesToFill.
TEST_F(AutofillAgentContentEditableInteractionTest, LeftClick) {
  EXPECT_CALL(
      autofill_driver(),
      AskForValuesToFill(
          FieldsAre(IsContentEditable()), _, _,
          mojom::AutofillSuggestionTriggerSource::kContentEditableClicked))
      .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());

  LoadHTML("<body><div id=ce contenteditable></body>");
  WaitForFormsSeen();
  SimulateElementClickAndWait("ce");
}

// Tests that unfocusing a contenteditable triggers a call to
// `AutofillDriver::HidePopup()`.
TEST_F(AutofillAgentContentEditableInteractionTest,
       LossOfFocusOfContentEditableTriggersHideAutofillSuggestions) {
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(
            FieldsAre(IsContentEditable()), _, _,
            mojom::AutofillSuggestionTriggerSource::kContentEditableClicked))
        .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());
    EXPECT_CALL(check, Call);
    EXPECT_CALL(autofill_driver(), HidePopup)
        .Times(NumCallsToHidePopupOnFocusLoss());
  }

  LoadHTML("<body><div id=ce contenteditable></div>");
  WaitForFormsSeen();
  SimulateElementClickAndWait("ce");
  check.Call();
  ChangeFocusToNull(GetMainFrame()->GetDocument());
}

// Scrolling doesn't hide the popup on Android.
#if !BUILDFLAG(IS_ANDROID)
// Tests that scrolling triggers a call to `AutofillDriver::HidePopup()`.
TEST_F(AutofillAgentContentEditableInteractionTest,
       ScrollingHidesAutofillPopup) {
  EXPECT_CALL(autofill_driver(), HidePopup);
  // "height" is needed so that the page is long enough to be able to scroll.
  LoadHTML(
      "<body><textarea style=\"height:10000px\" id=ce "
      "contenteditable></textarea></body>");
  SimulateElementClickAndWait("ce");
  SimulateElementFocusAndWait("ce");
  SimulateScrollingAndWait();
}
#endif  // !BUILDFLAG(IS_ANDROID)

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
    EXPECT_CALL(
        autofill_driver(),
        AskForValuesToFill(FieldsAre(Not(IsContentEditable())), _, _, _))
        .Times(NumCallsToAskForValuesToFillOnInitialLeftClick());
  }

  LoadHTML("<body><textarea id=ce contenteditable></textarea>");
  WaitForFormsSeen();
  check.Call();
  SimulateElementClickAndWait("ce");
}

}  // namespace
}  // namespace autofill
