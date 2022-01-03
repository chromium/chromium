// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/text_input.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/widget/widget.h"

using testing::_;

namespace exo {

namespace {

class MockTextInputDelegate : public TextInput::Delegate {
 public:
  MockTextInputDelegate() = default;

  MockTextInputDelegate(const MockTextInputDelegate&) = delete;
  MockTextInputDelegate& operator=(const MockTextInputDelegate&) = delete;

  // TextInput::Delegate:
  MOCK_METHOD(void, Activated, (), ());
  MOCK_METHOD(void, Deactivated, (), ());
  MOCK_METHOD(void, OnVirtualKeyboardVisibilityChanged, (bool), ());
  MOCK_METHOD(void, SetCompositionText, (const ui::CompositionText&), ());
  MOCK_METHOD(void, Commit, (const std::u16string&), ());
  MOCK_METHOD(void, SetCursor, (base::StringPiece16, const gfx::Range&), ());
  MOCK_METHOD(void,
              DeleteSurroundingText,
              (base::StringPiece16, const gfx::Range&),
              ());
  MOCK_METHOD(void, SendKey, (const ui::KeyEvent&), ());
  MOCK_METHOD(void, OnLanguageChanged, (const std::string&), ());
  MOCK_METHOD(void,
              OnTextDirectionChanged,
              (base::i18n::TextDirection direction),
              ());
  MOCK_METHOD(void,
              SetCompositionFromExistingText,
              (base::StringPiece16,
               const gfx::Range&,
               const gfx::Range&,
               const std::vector<ui::ImeTextSpan>& ui_ime_text_spans),
              ());
};

class TestingInputMethodObserver : public ui::InputMethodObserver {
 public:
  explicit TestingInputMethodObserver(ui::InputMethod* input_method)
      : input_method_(input_method) {
    input_method_->AddObserver(this);
  }

  TestingInputMethodObserver(const TestingInputMethodObserver&) = delete;
  TestingInputMethodObserver& operator=(const TestingInputMethodObserver&) =
      delete;

  ~TestingInputMethodObserver() override {
    input_method_->RemoveObserver(this);
  }

  // ui::InputMethodObserver
  MOCK_METHOD(void, OnFocus, (), ());
  MOCK_METHOD(void, OnBlur, (), ());
  MOCK_METHOD(void, OnCaretBoundsChanged, (const ui::TextInputClient*), ());
  MOCK_METHOD(void, OnTextInputStateChanged, (const ui::TextInputClient*), ());
  MOCK_METHOD(void, OnInputMethodDestroyed, (const ui::InputMethod*), ());
  MOCK_METHOD(void, OnVirtualKeyboardVisibilityChangedIfEnabled, (bool), ());

 private:
  ui::InputMethod* input_method_ = nullptr;
};

class TextInputTest : public test::ExoTestBase {
 public:
  TextInputTest() = default;

  TextInputTest(const TextInputTest&) = delete;
  TextInputTest& operator=(const TextInputTest&) = delete;

  void SetUp() override {
    test::ExoTestBase::SetUp();
    text_input_ =
        std::make_unique<TextInput>(std::make_unique<MockTextInputDelegate>());
    SetupSurface();
  }

  void TearDown() override {
    TearDownSurface();
    text_input_.reset();
    test::ExoTestBase::TearDown();
  }

  void SetupSurface() {
    gfx::Size buffer_size(32, 32);
    buffer_ = std::make_unique<Buffer>(
        exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
    surface_ = std::make_unique<Surface>();
    shell_surface_ = std::make_unique<ShellSurface>(surface_.get());

    surface_->Attach(buffer_.get());
    surface_->Commit();

    gfx::Point origin(100, 100);
    shell_surface_->SetGeometry(gfx::Rect(origin, buffer_size));
  }

  void TearDownSurface() {
    shell_surface_.reset();
    surface_.reset();
    buffer_.reset();
  }

 protected:
  TextInput* text_input() { return text_input_.get(); }
  MockTextInputDelegate* delegate() {
    return static_cast<MockTextInputDelegate*>(text_input_->delegate());
  }
  Surface* surface() { return surface_.get(); }

  ui::InputMethod* GetInputMethod() {
    return surface_->window()->GetHost()->GetInputMethod();
  }

  void SetCompositionText(const std::u16string& utf16) {
    ui::CompositionText t;
    t.text = utf16;
    t.selection = gfx::Range(1u);
    t.ime_text_spans.push_back(
        ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, t.text.size(),
                        ui::ImeTextSpan::Thickness::kThick));
    EXPECT_CALL(*delegate(), SetCompositionText(t)).Times(1);
    text_input()->SetCompositionText(t);
  }

 private:
  std::unique_ptr<TextInput> text_input_;

  std::unique_ptr<Buffer> buffer_;
  std::unique_ptr<Surface> surface_;
  std::unique_ptr<ShellSurface> shell_surface_;
};

TEST_F(TextInputTest, Activate) {
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_DEFAULT, text_input()->GetTextInputMode());

  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(surface());
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_TEXT, text_input()->GetTextInputMode());
  EXPECT_EQ(0, text_input()->GetTextInputFlags());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
  text_input()->Deactivate();
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_DEFAULT, text_input()->GetTextInputMode());
}

TEST_F(TextInputTest, ShowVirtualKeyboardIfEnabled) {
  TestingInputMethodObserver observer(GetInputMethod());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(surface());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Currently, Virtual Keyboard Controller is not set up, and so
  // the virtual keyboard events are gone. Here, we capture the callback
  // from the observer and translate it to the ones of
  // VirtualKeyboardControllerObserver event as if it is done via
  // real VirtualKeyboardController implementation.
  EXPECT_CALL(observer, OnVirtualKeyboardVisibilityChangedIfEnabled)
      .WillOnce(testing::Invoke([this](bool should_show) {
        if (should_show)
          text_input()->OnKeyboardVisible(gfx::Rect());
        else
          text_input()->OnKeyboardHidden();
      }));
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged(true)).Times(1);
  text_input()->ShowVirtualKeyboardIfEnabled();
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(observer, OnTextInputStateChanged(nullptr)).Times(1);
  EXPECT_CALL(*delegate(), Deactivated).Times(1);
  text_input()->Deactivate();
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(TextInputTest, ShowVirtualKeyboardIfEnabledBeforeActivated) {
  TestingInputMethodObserver observer(GetInputMethod());

  // ShowVirtualKeyboardIfEnabled before activation.
  text_input()->ShowVirtualKeyboardIfEnabled();

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);

  // Currently, Virtual Keyboard Controller is not set up, and so
  // the virtual keyboard events are gone. Here, we capture the callback
  // from the observer and translate it to the ones of
  // VirtualKeyboardControllerObserver event as if it is done via
  // real VirtualKeyboardController implementation.
  EXPECT_CALL(observer, OnVirtualKeyboardVisibilityChangedIfEnabled)
      .WillOnce(testing::Invoke([this](bool should_show) {
        if (should_show)
          text_input()->OnKeyboardVisible(gfx::Rect());
        else
          text_input()->OnKeyboardHidden();
      }));
  EXPECT_CALL(*delegate(), Activated).Times(1);
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged(true)).Times(1);
  text_input()->Activate(surface());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
}

TEST_F(TextInputTest, SetTypeModeFlag) {
  TestingInputMethodObserver observer(GetInputMethod());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(surface());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_TEXT, text_input()->GetTextInputMode());
  EXPECT_EQ(0, text_input()->GetTextInputFlags());
  EXPECT_TRUE(text_input()->ShouldDoLearning());

  int flags = ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_OFF |
              ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE;
  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  text_input()->SetTypeModeFlags(ui::TEXT_INPUT_TYPE_URL,
                                 ui::TEXT_INPUT_MODE_URL, flags, false);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_URL, text_input()->GetTextInputMode());
  EXPECT_EQ(flags, text_input()->GetTextInputFlags());
  EXPECT_FALSE(text_input()->ShouldDoLearning());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
}

TEST_F(TextInputTest, CaretBounds) {
  TestingInputMethodObserver observer(GetInputMethod());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(surface());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  gfx::Rect bounds(10, 10, 0, 16);
  EXPECT_CALL(observer, OnCaretBoundsChanged(text_input())).Times(1);
  text_input()->SetCaretBounds(bounds);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_EQ(bounds.size().ToString(),
            text_input()->GetCaretBounds().size().ToString());
  gfx::Point origin = surface()->window()->GetBoundsInScreen().origin();
  origin += bounds.OffsetFromOrigin();
  EXPECT_EQ(origin.ToString(),
            text_input()->GetCaretBounds().origin().ToString());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
}

TEST_F(TextInputTest, CompositionText) {
  EXPECT_FALSE(text_input()->HasCompositionText());
  SetCompositionText(u"composition");
  EXPECT_TRUE(text_input()->HasCompositionText());

  ui::CompositionText empty;
  EXPECT_CALL(*delegate(), SetCompositionText(empty)).Times(1);
  text_input()->ClearCompositionText();
  EXPECT_FALSE(text_input()->HasCompositionText());
}

TEST_F(TextInputTest, CompositionTextEmpty) {
  SetCompositionText(u"");

  EXPECT_CALL(*delegate(), SetCompositionText(_)).Times(0);
  text_input()->ClearCompositionText();
}

TEST_F(TextInputTest, CommitCompositionText) {
  SetCompositionText(u"composition");

  EXPECT_CALL(*delegate(), Commit(std::u16string(u"composition"))).Times(1);
  const uint32_t composition_text_length =
      text_input()->ConfirmCompositionText(/*keep_selection=*/false);
  EXPECT_EQ(composition_text_length, static_cast<uint32_t>(11));
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Second call should be the empty commit string.
  EXPECT_EQ(0u, text_input()->ConfirmCompositionText(/*keep_selection=*/false));
}

TEST_F(TextInputTest, ResetCompositionText) {
  SetCompositionText(u"composition");

  text_input()->Reset();
  EXPECT_EQ(0u, text_input()->ConfirmCompositionText(/*keep_selection=*/false));
}

TEST_F(TextInputTest, Commit) {
  std::u16string s = u"commit text";

  EXPECT_CALL(*delegate(), Commit(s)).Times(1);
  text_input()->InsertText(
      s, ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_FALSE(text_input()->HasCompositionText());
}

TEST_F(TextInputTest, InsertChar) {
  text_input()->Activate(surface());

  ui::KeyEvent ev(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0);

  EXPECT_CALL(*delegate(), SendKey(testing::Ref(ev))).Times(1);
  text_input()->InsertChar(ev);
}

TEST_F(TextInputTest, InsertCharCtrlV) {
  text_input()->Activate(surface());

  // CTRL+V is interpreted as non-IME consumed KeyEvent, so should
  // not be sent.
  ui::KeyEvent ev(ui::ET_KEY_PRESSED, ui::VKEY_V, ui::EF_CONTROL_DOWN);
  EXPECT_CALL(*delegate(), SendKey(_)).Times(0);
  text_input()->InsertChar(ev);
}

TEST_F(TextInputTest, InsertCharNormalKey) {
  text_input()->Activate(surface());

  char16_t ch = 'x';
  ui::KeyEvent ev(ch, ui::VKEY_X, ui::DomCode::NONE, 0);

  EXPECT_CALL(*delegate(), SendKey(testing::Ref(ev))).Times(1);
  text_input()->InsertChar(ev);
}

TEST_F(TextInputTest, SurroundingText) {
  TestingInputMethodObserver observer(GetInputMethod());

  gfx::Range range;
  EXPECT_FALSE(text_input()->GetTextRange(&range));
  EXPECT_FALSE(text_input()->GetCompositionTextRange(&range));
  EXPECT_FALSE(text_input()->GetEditableSelectionRange(&range));
  std::u16string got_text;
  EXPECT_FALSE(text_input()->GetTextFromRange(gfx::Range(0, 1), &got_text));

  text_input()->Activate(surface());

  EXPECT_CALL(observer, OnCaretBoundsChanged(text_input())).Times(1);
  std::u16string text = u"surrounding\u3000text";
  text_input()->SetSurroundingText(text, gfx::Range(11, 12));
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(text_input()->GetTextRange(&range));
  EXPECT_EQ(gfx::Range(0, text.size()).ToString(), range.ToString());

  EXPECT_FALSE(text_input()->GetCompositionTextRange(&range));
  EXPECT_TRUE(text_input()->GetEditableSelectionRange(&range));
  EXPECT_EQ(gfx::Range(11, 12).ToString(), range.ToString());
  EXPECT_TRUE(text_input()->GetTextFromRange(gfx::Range(11, 12), &got_text));
  EXPECT_EQ(text.substr(11, 1), got_text);

  EXPECT_CALL(*delegate(), DeleteSurroundingText(base::StringPiece16(text),
                                                 gfx::Range(11, 12)))
      .Times(1);
  text_input()->ExtendSelectionAndDelete(0, 0);
  testing::Mock::VerifyAndClearExpectations(delegate());

  size_t composition_size = std::string("composition").size();
  SetCompositionText(u"composition");
  EXPECT_TRUE(text_input()->GetCompositionTextRange(&range));
  EXPECT_EQ(gfx::Range(11, 11 + composition_size).ToString(), range.ToString());
  EXPECT_TRUE(text_input()->GetTextRange(&range));
  EXPECT_EQ(gfx::Range(0, text.size() - 1 + composition_size).ToString(),
            range.ToString());
  EXPECT_TRUE(text_input()->GetEditableSelectionRange(&range));
  EXPECT_EQ(gfx::Range(11, 12).ToString(), range.ToString());
}

TEST_F(TextInputTest, GetTextRange) {
  std::u16string text = u"surrounding text";
  text_input()->SetSurroundingText(text, gfx::Range(11, 12));

  SetCompositionText(u"composition");

  const struct {
    gfx::Range range;
    std::u16string expected;
  } kTestCases[] = {
      {gfx::Range(0, 3), u"sur"},
      {gfx::Range(10, 13), u"gco"},
      {gfx::Range(10, 23), u"gcompositiont"},
      {gfx::Range(12, 15), u"omp"},
      {gfx::Range(12, 23), u"ompositiont"},
      {gfx::Range(22, 25), u"tex"},
  };
  for (auto& c : kTestCases) {
    std::u16string result;
    EXPECT_TRUE(text_input()->GetTextFromRange(c.range, &result))
        << c.range.ToString();
    EXPECT_EQ(c.expected, result) << c.range.ToString();
  }
}

TEST_F(TextInputTest, SetCompositionFromExistingText) {
  // Try invalid cases fist. No delegate invocation is expected.
  EXPECT_CALL(*delegate(), SetCompositionFromExistingText(_, _, _, _)).Times(0);

  // Not set up surrounding text yet, so any request should fail.
  EXPECT_FALSE(text_input()->SetCompositionFromExistingText(
      gfx::Range::InvalidRange(), {}));
  EXPECT_FALSE(
      text_input()->SetCompositionFromExistingText(gfx::Range(0, 1), {}));

  text_input()->SetSurroundingText(u"surrounding text", gfx::Range(5, 5));

  // Invalid range.
  EXPECT_FALSE(text_input()->SetCompositionFromExistingText(
      gfx::Range::InvalidRange(), {}));
  // Outside of surrounding text.
  EXPECT_FALSE(
      text_input()->SetCompositionFromExistingText(gfx::Range(100, 200), {}));
  // Crossing the boundary of surrounding text.
  EXPECT_FALSE(
      text_input()->SetCompositionFromExistingText(gfx::Range(5, 100), {}));
  // Span has the range outside of the new composition.
  EXPECT_FALSE(text_input()->SetCompositionFromExistingText(
      gfx::Range(3, 10),
      {ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 7, 10)}));
  // Span has the range crossing the composition boundary.
  EXPECT_FALSE(text_input()->SetCompositionFromExistingText(
      gfx::Range(3, 10),
      {ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 2, 10)}));

  // Verify mock behavior. No delegate call is expected until now.
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Checking a simple valid case.
  EXPECT_CALL(*delegate(), SetCompositionFromExistingText(_, _, _, _)).Times(1);
  EXPECT_TRUE(
      text_input()->SetCompositionFromExistingText(gfx::Range(3, 10), {}));
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Anothe valid case with span.
  EXPECT_CALL(*delegate(), SetCompositionFromExistingText(_, _, _, _)).Times(1);
  EXPECT_TRUE(text_input()->SetCompositionFromExistingText(
      gfx::Range(3, 10),
      {ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 1, 5)}));
  testing::Mock::VerifyAndClearExpectations(delegate());
}

}  // anonymous namespace
}  // namespace exo
