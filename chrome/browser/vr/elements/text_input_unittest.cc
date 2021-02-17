// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/mock_keyboard_delegate.h"
#include "chrome/browser/vr/test/mock_text_input_delegate.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/render_text.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace vr {

namespace {
constexpr float kFontHeightMeters = 0.050f;
}

class TextInputSceneTest : public UiTest {
 public:
  void SetUp() override {
    UiTest::SetUp();
    CreateScene(kNotInWebVr);

    // Make test text input.
    text_input_delegate_ =
        std::make_unique<StrictMock<MockTextInputDelegate>>();
    edited_text_ = std::make_unique<EditedText>(base::ASCIIToUTF16("asdfg"));
    auto text_input = CreateTextInput(1, model_, edited_text_.get(),
                                      text_input_delegate_.get());
    text_input_ = text_input.get();
    scene_->AddUiElement(k2dBrowsingForeground, std::move(text_input));
    EXPECT_TRUE(AdvanceFrame());
  }

 protected:
  TextInput* text_input_;
  std::unique_ptr<StrictMock<MockTextInputDelegate>> text_input_delegate_;
  std::unique_ptr<EditedText> edited_text_;
  testing::Sequence in_sequence_;

 private:
  std::unique_ptr<TextInput> CreateTextInput(
      float font_height_meters,
      Model* model,
      EditedText* text_input_model,
      TextInputDelegate* text_input_delegate) {
    auto text_input = std::make_unique<TextInput>(
        font_height_meters,
        base::BindRepeating(
            [](EditedText* model, const EditedText& text_input_info) {
              *model = text_input_info;
            },
            base::Unretained(text_input_model)));
    EventHandlers event_handlers;
    event_handlers.focus_change = base::BindRepeating(
        [](Model* model, TextInput* text_input, EditedText* text_input_info,
           bool focused) {
          if (focused) {
            model->editing_input = true;
            text_input->UpdateInput(*text_input_info);
          } else {
            model->editing_input = false;
          }
        },
        base::Unretained(model), base::Unretained(text_input.get()),
        base::Unretained(text_input_model));
    text_input->SetSize(1.0f, 0.1f);
    text_input->set_event_handlers(event_handlers);
    text_input->SetDrawPhase(kPhaseNone);
    text_input->SetTextInputDelegate(text_input_delegate);
    text_input->AddBinding(std::make_unique<Binding<EditedText>>(
        VR_BIND_LAMBDA([](EditedText* info) { return *info; },
                       base::Unretained(text_input_model)),
        VR_BIND_LAMBDA([](TextInput* e,
                          const EditedText& value) { e->UpdateInput(value); },
                       base::Unretained(text_input.get()))));
    return text_input;
  }
};

TEST_F(TextInputSceneTest, InputFieldFocus) {
  // Set mock delegates.
  auto* kb = static_cast<Keyboard*>(scene_->GetUiElementByName(kKeyboard));
  auto kb_delegate = std::make_unique<StrictMock<MockKeyboardDelegate>>();
  EXPECT_CALL(*kb_delegate, HideKeyboard()).InSequence(in_sequence_);
  kb->SetKeyboardDelegate(kb_delegate.get());

  // Clicking on the text field should request focus.
  EXPECT_CALL(*text_input_delegate_, RequestFocus(_)).InSequence(in_sequence_);
  text_input_->OnButtonUp(gfx::PointF(), base::TimeTicks());

  // Focusing on an input field should show the keyboard and tell the delegate
  // the field's content.
  EXPECT_CALL(*text_input_delegate_, UpdateInput(edited_text_->current))
      .InSequence(in_sequence_);
  text_input_->OnFocusChanged(true);
  EXPECT_CALL(*kb_delegate, ShowKeyboard()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  EXPECT_TRUE(AdvanceFrame());
  EXPECT_EQ(*edited_text_, text_input_->edited_text());

  // Focusing out of an input field should hide the keyboard.
  text_input_->OnFocusChanged(false);
  EXPECT_CALL(*kb_delegate, HideKeyboard()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, OnBeginFrame()).InSequence(in_sequence_);
  EXPECT_CALL(*kb_delegate, SetTransform(_)).InSequence(in_sequence_);
  EXPECT_TRUE(AdvanceFrame());
  EXPECT_EQ(*edited_text_, text_input_->edited_text());
}

TEST_F(TextInputSceneTest, InputFieldEdit) {
  // UpdateInput should not be called if the text input doesn't have focus.
  EXPECT_CALL(*text_input_delegate_, UpdateInput(_))
      .Times(0)
      .InSequence(in_sequence_);
  text_input_->OnFocusChanged(false);

  // Focus on input field.
  EXPECT_CALL(*text_input_delegate_, UpdateInput(_)).InSequence(in_sequence_);
  text_input_->OnFocusChanged(true);

  // Edits from the keyboard update the underlying text input  model.
  EXPECT_CALL(*text_input_delegate_, UpdateInput(_)).InSequence(in_sequence_);
  EditedText info(base::ASCIIToUTF16("asdfgh"));
  text_input_->OnInputEdited(info);
  EXPECT_TRUE(AdvanceFrame());
  EXPECT_EQ(info, *edited_text_);
}

TEST_F(TextInputSceneTest, ClickOnTextGrabsFocus) {
  EXPECT_CALL(*text_input_delegate_, RequestFocus(_));
  text_input_->get_text_element()->OnButtonUp({0, 0}, base::TimeTicks());
}

TEST(TextInputTest, ControllerInteractionsSentToDelegate) {
  auto keyboard = std::make_unique<Keyboard>();
  auto kb_delegate = std::make_unique<StrictMock<MockKeyboardDelegate>>();
  testing::Sequence s;
  EXPECT_CALL(*kb_delegate, HideKeyboard()).InSequence(s);
  keyboard->SetKeyboardDelegate(kb_delegate.get());

  EXPECT_CALL(*kb_delegate, OnHoverEnter(_)).InSequence(s);
  EXPECT_CALL(*kb_delegate, OnHoverLeave()).InSequence(s);
  EXPECT_CALL(*kb_delegate, OnHoverMove(_)).InSequence(s);
  EXPECT_CALL(*kb_delegate, OnButtonDown(_)).InSequence(s);
  EXPECT_CALL(*kb_delegate, OnButtonUp(_)).InSequence(s);
  gfx::PointF p;
  keyboard->OnHoverEnter(p, base::TimeTicks());
  keyboard->OnHoverLeave(base::TimeTicks());
  keyboard->OnHoverMove(p, base::TimeTicks());
  keyboard->OnButtonDown(p, base::TimeTicks());
  keyboard->OnButtonUp(p, base::TimeTicks());
}

TEST(TextInputTest, HintText) {
  UiScene scene;

  auto instance = std::make_unique<TextInput>(
      kFontHeightMeters, TextInput::OnInputEditedCallback());
  EventHandlers event_handlers;
  event_handlers.focus_change = TextInput::OnFocusChangedCallback();
  instance->set_event_handlers(event_handlers);
  instance->SetName(kOmniboxTextField);
  instance->SetSize(1, 0);
  TextInput* element = instance.get();
  scene.root_element().AddChild(std::move(instance));

  // Text field is empty, so we should be showing hint text.
  scene.OnBeginFrame(base::TimeTicks(), kStartHeadPose);
  EXPECT_GT(element->get_hint_element()->GetTargetOpacity(), 0);

  // When text enters the field, the hint should disappear.
  EditedText info(base::UTF8ToUTF16("text"));
  element->UpdateInput(info);
  scene.OnBeginFrame(base::TimeTicks(), kStartHeadPose);
  EXPECT_EQ(element->get_hint_element()->GetTargetOpacity(), 0);
}

TEST(TextInputTest, CursorBlinking) {
  UiScene scene;
  auto instance = std::make_unique<TextInput>(
      kFontHeightMeters, TextInput::OnInputEditedCallback());
  EventHandlers event_handlers;
  event_handlers.focus_change = TextInput::OnFocusChangedCallback();
  instance->set_event_handlers(event_handlers);
  instance->SetName(kOmniboxTextField);
  instance->SetSize(1, 0);
  TextInput* element = instance.get();
  scene.root_element().AddChild(std::move(instance));

  // The cursor should not be blinking or visible.
  float initial = element->get_cursor_element()->GetTargetOpacity();
  EXPECT_EQ(initial, 0.f);
  for (int ms = 0; ms <= 2000; ms += 100) {
    scene.OnBeginFrame(MsToTicks(ms), kStartHeadPose);
    EXPECT_EQ(initial, element->get_cursor_element()->GetTargetOpacity());
  }

  // When focused, the cursor should start blinking.
  element->OnFocusChanged(true);
  initial = element->get_cursor_element()->GetTargetOpacity();
  bool toggled = false;
  for (int ms = 0; ms <= 2000; ms += 100) {
    scene.OnBeginFrame(MsToTicks(ms), kStartHeadPose);
    if (initial != element->get_cursor_element()->GetTargetOpacity())
      toggled = true;
  }
  EXPECT_TRUE(toggled);

  // With a selection, the cursor should not be blinking or visible.
  EditedText info(base::UTF8ToUTF16("text"));
  info.current.selection_start = 0;
  info.current.selection_end = info.current.text.size();
  element->UpdateInput(info);
  EXPECT_EQ(0.f, element->get_cursor_element()->GetTargetOpacity());
  for (int ms = 0; ms <= 2000; ms += 100) {
    scene.OnBeginFrame(MsToTicks(ms), kStartHeadPose);
    EXPECT_EQ(0.f, element->get_cursor_element()->GetTargetOpacity());
  }
}

// TODO(cjgrant): Have this test, and others similar, check the actual position
// of the cursor element.  To make this work, the OnBeginFrame logic needs to be
// updated to perform more of the measurement and layout steps in a test
// environment.  As of now, much of this is skipped due to lack of a GL context.
TEST(TextInputTest, CursorPositionUpdatesOnKeyboardInput) {
  auto element = std::make_unique<TextInput>(
      kFontHeightMeters, TextInput::OnInputEditedCallback());
  EventHandlers event_handlers;
  event_handlers.focus_change = TextInput::OnFocusChangedCallback();
  element->set_event_handlers(event_handlers);
  element->SetSize(1, 0);

  EditedText info(base::UTF8ToUTF16("text"));
  info.current.selection_start = 0;
  info.current.selection_end = 0;
  element->UpdateInput(info);
  element->get_text_element()->PrepareToDrawForTest();
  int x1 = element->get_text_element()->GetRawCursorBounds().x();

  info.current.selection_start = 1;
  info.current.selection_end = 1;
  element->UpdateInput(info);
  element->get_text_element()->PrepareToDrawForTest();
  int x2 = element->get_text_element()->GetRawCursorBounds().x();

  EXPECT_LT(x1, x2);
}

TEST(TextInputTest, CursorPositionUpdatesOnClicks) {
  auto element = std::make_unique<TextInput>(
      kFontHeightMeters, TextInput::OnInputEditedCallback());
  EventHandlers event_handlers;
  event_handlers.focus_change = TextInput::OnFocusChangedCallback();
  element->set_event_handlers(event_handlers);
  element->SetSize(1, 0);

  EditedText info(base::UTF8ToUTF16("text"));
  element->UpdateInput(info);
  element->get_text_element()->PrepareToDrawForTest();

  // Click on the left edge of the field.
  element->OnButtonDown(gfx::PointF(0.0, 0.5), base::TimeTicks());
  element->OnButtonUp(gfx::PointF(0.0, 0.5), base::TimeTicks());
  element->get_text_element()->PrepareToDrawForTest();
  auto x1 = element->get_text_element()->GetRawCursorBounds().x();

  // Click on the right edge of the field.
  element->OnButtonDown(gfx::PointF(1.0, 0.5), base::TimeTicks());
  element->OnButtonUp(gfx::PointF(1.0, 0.5), base::TimeTicks());
  element->get_text_element()->PrepareToDrawForTest();
  auto x2 = element->get_text_element()->GetRawCursorBounds().x();

  EXPECT_EQ(x1, 0);
  EXPECT_GT(x2, 0);

  // Set a selection and ensure that a click clears it.
  info.current.selection_start = 0;
  info.current.selection_end = info.current.text.size();
  element->UpdateInput(info);
  EXPECT_GT(element->edited_text().current.SelectionSize(), 0u);
  element->OnButtonDown(gfx::PointF(0.5, 0.5), base::TimeTicks());
  element->OnButtonUp(gfx::PointF(0.5, 0.5), base::TimeTicks());
  EXPECT_EQ(element->edited_text().current.SelectionSize(), 0u);
}

TEST(TextInputTest, TextSelectionUpdatesOnTouchMove) {
  auto element = std::make_unique<TextInput>(
      kFontHeightMeters, TextInput::OnInputEditedCallback());
  element->SetSize(1.0, 0);

  EditedText info(TextInputInfo(
      base::UTF8ToUTF16("this is a long text with the cursor at the beginning"),
      0, 0));
  element->UpdateInput(info);
  element->get_text_element()->PrepareToDrawForTest();
  EXPECT_EQ(element->edited_text().current.SelectionSize(), 0u);

  // Click on the left edge of the field.
  element->OnButtonDown(gfx::PointF(0.0, 0.5), base::TimeTicks());
  EXPECT_EQ(element->edited_text().current.selection_start, 0);
  EXPECT_EQ(element->edited_text().current.selection_end, 0);

  element->OnTouchMove(gfx::PointF(0.5, 0.5), base::TimeTicks());
  EXPECT_EQ(element->edited_text().current.selection_start, 0);
  auto end1 = element->edited_text().current.selection_end;
  EXPECT_GT(end1, 0);

  // Move to the right edge of the field.
  element->OnTouchMove(gfx::PointF(1.0, 0.5), base::TimeTicks());
  EXPECT_EQ(element->edited_text().current.selection_start, 0);
  auto end2 = element->edited_text().current.selection_end;
  EXPECT_GT(end2, end1);

  // Move past the right edge of the field and release.
  element->OnTouchMove(gfx::PointF(2.0, 0.5), base::TimeTicks());
  element->OnButtonUp(gfx::PointF(2.0, 0.5), base::TimeTicks());
  auto end3 = element->edited_text().current.selection_end;
  EXPECT_GT(end3, end2);
}

}  // namespace vr
