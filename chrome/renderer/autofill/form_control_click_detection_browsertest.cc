// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
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

FieldRendererId GetFieldRendererId(blink::WebElement element) {
  blink::WebFormControlElement field =
      element.To<blink::WebFormControlElement>();
  if (field.IsNull())
    return {};
  return form_util::GetFieldRendererId(field);
}

}  // namespace

class FormControlClickDetectionTest : public ChromeRenderViewTest {
 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    // Must be set before loading HTML.
    web_view_->SetDefaultPageScaleLimits(1, 4);

    LoadHTML(
        "<form>"
        "  <input type='text' id='text_1'></input><br>"
        "  <input type='text' id='text_2'></input><br>"
        "  <textarea  id='textarea_1'></textarea><br>"
        "  <textarea  id='textarea_2'></textarea><br>"
        "  <input type='button' id='button'></input><br>"
        "  <input type='button' id='button_2' disabled></input><br>"
        "</form>");
    GetWebFrameWidget()->Resize(gfx::Size(500, 500));
    GetWebFrameWidget()->SetFocus(true);
    blink::WebDocument document = GetMainFrame()->GetDocument();
    text_ = document.GetElementById("text_1");
    textarea_ = document.GetElementById("textarea_1");
    ASSERT_FALSE(text_.IsNull());
    ASSERT_FALSE(textarea_.IsNull());

    // Enable show-ime event when element is focused by indicating that a user
    // gesture has been processed since load.
    EXPECT_TRUE(SimulateElementClick("button"));
  }

  void TearDown() override {
    text_.Reset();
    textarea_.Reset();
    ChromeRenderViewTest::TearDown();
  }

  void ClearAutofillAgentTestState() {
    autofill_agent_->last_clicked_form_control_element_for_testing_ = {};
  }

  FieldRendererId last_clicked_form_control_element() const {
    return autofill_agent_->last_clicked_form_control_element_for_testing_;
  }

  bool form_control_element_clicked_called() const {
    return !last_clicked_form_control_element().is_null();
  }

  blink::WebElement text_;
  blink::WebElement textarea_;
};

// Tests that a clicked input call is properly handled by AutofillAgent.
TEST_F(FormControlClickDetectionTest, InputClicked) {
  ClearAutofillAgentTestState();
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(text_), last_clicked_form_control_element());
  ClearAutofillAgentTestState();

  // Click the text field again and verify that AutofillAgent knows about its
  // focus.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(text_), last_clicked_form_control_element());
  ClearAutofillAgentTestState();

  // Click the button, no notification should happen (this is not a text-input).
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_FALSE(form_control_element_clicked_called());
}

// Tests that AutofillAgent ignores a right click.
TEST_F(FormControlClickDetectionTest, InputRightClicked) {
  ClearAutofillAgentTestState();
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Right click the text field once.
  EXPECT_TRUE(SimulateElementRightClick("text_1"));
  EXPECT_FALSE(form_control_element_clicked_called());
  EXPECT_NE(GetFieldRendererId(text_), last_clicked_form_control_element());
}

TEST_F(FormControlClickDetectionTest, InputFocusedAndClicked) {
  ClearAutofillAgentTestState();
  // Focus the text field without a click.
  ExecuteJavaScriptForTests("document.getElementById('text_1').focus();");
  EXPECT_FALSE(form_control_element_clicked_called());
  ClearAutofillAgentTestState();

  // Click the focused text field to test that was_focused_ is set correctly.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(text_), last_clicked_form_control_element());
}

// Tests that AutofillAgent accepts form clicks for a textarea element which is
// clicked.
TEST_F(FormControlClickDetectionTest, TextAreaClicked) {
  ClearAutofillAgentTestState();
  // Click the textarea field once.
  EXPECT_TRUE(SimulateElementClick("textarea_1"));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(textarea_), last_clicked_form_control_element());
  ClearAutofillAgentTestState();

  // Click the text field again and verify that AutofillAgent knows about its
  // focus.
  EXPECT_TRUE(SimulateElementClick("textarea_1"));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(textarea_), last_clicked_form_control_element());
  ClearAutofillAgentTestState();

  // Click the button, no notification should happen (this is not a text-input).
  EXPECT_TRUE(SimulateElementClick("button"));
  EXPECT_FALSE(form_control_element_clicked_called());
}

TEST_F(FormControlClickDetectionTest, TextAreaFocusedAndClicked) {
  ClearAutofillAgentTestState();
  // Focus the textarea without a click.
  ExecuteJavaScriptForTests("document.getElementById('textarea_1').focus();");
  EXPECT_FALSE(form_control_element_clicked_called());
  ClearAutofillAgentTestState();

  // Click the text field again and verify that AutofillAgent knows about its
  // focus.
  EXPECT_TRUE(SimulateElementClick("textarea_1"));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(textarea_), last_clicked_form_control_element());
  ClearAutofillAgentTestState();
}

TEST_F(FormControlClickDetectionTest, ScaledTextareaClicked) {
  ClearAutofillAgentTestState();
  EXPECT_NE(textarea_, textarea_.GetDocument().FocusedElement());
  web_view_->SetPageScaleFactor(3);
  web_view_->SetVisualViewportOffset(gfx::PointF(50, 50));

  // Click textarea_1.
  SimulatePointClick(gfx::Point(30, 30));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(textarea_), last_clicked_form_control_element());
}

TEST_F(FormControlClickDetectionTest, ScaledTextareaTapped) {
  ClearAutofillAgentTestState();
  EXPECT_NE(textarea_, textarea_.GetDocument().FocusedElement());
  web_view_->SetPageScaleFactor(3);
  web_view_->SetVisualViewportOffset(gfx::PointF(50, 50));

  // Tap textarea_1.
  SimulateRectTap(gfx::Rect(30, 30, 30, 30));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(textarea_), last_clicked_form_control_element());
}

TEST_F(FormControlClickDetectionTest, DisabledInputClickedNoEvent) {
  ClearAutofillAgentTestState();
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(text_), last_clicked_form_control_element());
  ClearAutofillAgentTestState();

  // Click the disabled element.
  EXPECT_TRUE(SimulateElementClick("button_2"));
  EXPECT_FALSE(form_control_element_clicked_called());
}

TEST_F(FormControlClickDetectionTest,
       ClickDisabledInputDoesNotResetClickCounter) {
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(text_), last_clicked_form_control_element());
  ClearAutofillAgentTestState();

  // Click the disabled element and focus should change.
  EXPECT_TRUE(SimulateElementClick("button_2"));
  EXPECT_FALSE(form_control_element_clicked_called());
  ClearAutofillAgentTestState();

  // Click the text field second time and verify it has lost
  // focus already.
  EXPECT_TRUE(SimulateElementClick("text_1"));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(text_), last_clicked_form_control_element());
}

TEST_F(FormControlClickDetectionTest, TapNearEdgeIsPageClick) {
  EXPECT_NE(text_, text_.GetDocument().FocusedElement());
  // Tap outside of element bounds, but tap width is overlapping the field.
  gfx::Rect element_bounds = GetElementBounds("text_1");
  SimulateRectTap(element_bounds -
                  gfx::Vector2d(element_bounds.width() / 2 + 1, 0));
  EXPECT_TRUE(form_control_element_clicked_called());
  EXPECT_EQ(GetFieldRendererId(text_), last_clicked_form_control_element());
}

}  // namespace autofill
