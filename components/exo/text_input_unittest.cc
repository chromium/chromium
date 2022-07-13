// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/text_input.h"

#include <memory>
#include <string>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/buffer.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/widget/widget.h"

using testing::_;

namespace exo {

namespace {

ui::CompositionText GenerateCompositionText(const std::u16string& text) {
  ui::CompositionText t;
  t.text = text;
  t.selection = gfx::Range(1u);
  t.ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, t.text.size(),
                      ui::ImeTextSpan::Thickness::kThick));
  return t;
}

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
  MOCK_METHOD(void,
              ClearGrammarFragments,
              (base::StringPiece16, const gfx::Range&),
              ());
  MOCK_METHOD(void,
              AddGrammarFragment,
              (base::StringPiece16, const ui::GrammarFragment&),
              ());
  MOCK_METHOD(void,
              SetAutocorrectRange,
              (base::StringPiece16, const gfx::Range&),
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
  class TestSurface {
   public:
    void SetUp(test::ExoTestHelper* exo_test_helper);
    void TearDown();
    Surface* surface() { return surface_.get(); }

   private:
    std::unique_ptr<Buffer> buffer_;
    std::unique_ptr<Surface> surface_;
    std::unique_ptr<ShellSurface> shell_surface_;
  };

  TextInputTest() = default;

  TextInputTest(const TextInputTest&) = delete;
  TextInputTest& operator=(const TextInputTest&) = delete;

  void SetUp() override {
    test::ExoTestBase::SetUp();
    text_input_ =
        std::make_unique<TextInput>(std::make_unique<MockTextInputDelegate>());
    seat_ = std::make_unique<Seat>();
    test_surface_.SetUp(exo_test_helper());
  }

  void TearDown() override {
    test_surface_.TearDown();
    seat_.reset();
    text_input_.reset();
    test::ExoTestBase::TearDown();
  }

 protected:
  TextInput* text_input() { return text_input_.get(); }
  void DestroyTextInput() { text_input_.reset(); }
  MockTextInputDelegate* delegate() {
    return static_cast<MockTextInputDelegate*>(text_input_->delegate());
  }
  Surface* surface() { return test_surface_.surface(); }
  Seat* seat() { return seat_.get(); }

  ui::InputMethod* GetInputMethod() {
    return surface()->window()->GetHost()->GetInputMethod();
  }

  void SetCompositionText(const std::u16string& utf16) {
    ui::CompositionText t = GenerateCompositionText(utf16);
    EXPECT_CALL(*delegate(), SetCompositionText(t)).Times(1);
    text_input()->SetCompositionText(t);
  }

 private:
  std::unique_ptr<TextInput> text_input_;

  std::unique_ptr<Seat> seat_;
  TestSurface test_surface_;
};

void TextInputTest::TestSurface::SetUp(test::ExoTestHelper* exo_test_helper) {
  gfx::Size buffer_size(32, 32);
  buffer_ = std::make_unique<Buffer>(
      exo_test_helper->CreateGpuMemoryBuffer(buffer_size));
  surface_ = std::make_unique<Surface>();
  shell_surface_ = std::make_unique<ShellSurface>(surface_.get());

  surface_->Attach(buffer_.get());
  surface_->Commit();

  gfx::Point origin(100, 100);
  shell_surface_->SetGeometry(gfx::Rect(origin, buffer_size));
}

void TextInputTest::TestSurface::TearDown() {
  shell_surface_.reset();
  surface_.reset();
  buffer_.reset();
}

TEST_F(TextInputTest, Activate) {
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_DEFAULT, text_input()->GetTextInputMode());

  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface());
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

TEST_F(TextInputTest, ActivationRequiresFocus) {
  TestingInputMethodObserver observer(GetInputMethod());

  // Activation doesn't occur until the surface (window) is actually focused.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);
  EXPECT_CALL(observer, OnTextInputStateChanged(_)).Times(0);
  EXPECT_CALL(*delegate(), Activated).Times(0);
  text_input()->Activate(seat(), surface());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  focus_client->FocusWindow(surface()->window());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Deactivation occurs on blur even if TextInput::Deactivate() isn't called.
  EXPECT_CALL(observer, OnTextInputStateChanged(nullptr)).Times(1);
  EXPECT_CALL(*delegate(), Deactivated).Times(1);
  focus_client->FocusWindow(nullptr);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(observer, OnTextInputStateChanged(nullptr)).Times(0);
  EXPECT_CALL(*delegate(), Deactivated).Times(0);
  text_input()->Deactivate();
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(TextInputTest, MultipleActivations) {
  TestingInputMethodObserver observer(GetInputMethod());
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  TestSurface surface2;
  surface2.SetUp(exo_test_helper());

  // Activate surface 1.
  focus_client->FocusWindow(surface()->window());
  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Attempting to activate the same surface is a no-op.
  EXPECT_CALL(*delegate(), Activated).Times(0);
  text_input()->Activate(seat(), surface());
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Activating a non-focused surface causes deactivation until focus.
  EXPECT_CALL(observer, OnTextInputStateChanged(nullptr)).Times(1);
  EXPECT_CALL(*delegate(), Deactivated).Times(1);
  text_input()->Activate(seat(), surface2.surface());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  focus_client->FocusWindow(surface2.surface()->window());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(TextInputTest, ShowVirtualKeyboardIfEnabled) {
  TestingInputMethodObserver observer(GetInputMethod());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface());
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
  text_input()->Activate(seat(), surface());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
}

TEST_F(TextInputTest, VirtualKeyboardObserver) {
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_DEFAULT, text_input()->GetTextInputMode());

  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface());
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Disable virtual keyboard so that GetVirtualKeyboardController() starts
  // to return nullptr.
  auto* input_method_manager =
      static_cast<ash::input_method::MockInputMethodManager*>(
          ash::input_method::InputMethodManager::Get());
  input_method_manager->SetVirtualKeyboardEnabled(false);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_TEXT, text_input()->GetTextInputMode());
  EXPECT_EQ(0, text_input()->GetTextInputFlags());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
  text_input()->Deactivate();
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_DEFAULT, text_input()->GetTextInputMode());

  // Destroy the text_input.
  // Because text_input used not to be removed from VirtualKeyboardController
  // as its observer, this used to cause a dangling pointer problem, so
  // caused the crash in the following DismissVirtualKeyboard.
  DestroyTextInput();
  input_method_manager->DismissVirtualKeyboard();
}

TEST_F(TextInputTest, SetTypeModeFlag) {
  TestingInputMethodObserver observer(GetInputMethod());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface());
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
  text_input()->Activate(seat(), surface());
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
  EXPECT_FALSE(text_input()->HasCompositionText());
}

TEST_F(TextInputTest, ResetCompositionText) {
  SetCompositionText(u"composition");

  text_input()->Reset();
  EXPECT_EQ(0u, text_input()->ConfirmCompositionText(/*keep_selection=*/false));
  EXPECT_FALSE(text_input()->HasCompositionText());
}

TEST_F(TextInputTest, Commit) {
  std::u16string s = u"commit text";

  EXPECT_CALL(*delegate(), Commit(s)).Times(1);
  text_input()->InsertText(
      s, ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_FALSE(text_input()->HasCompositionText());
}

TEST_F(TextInputTest, InsertChar) {
  text_input()->Activate(seat(), surface());

  ui::KeyEvent ev(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0);

  EXPECT_CALL(*delegate(), SendKey(testing::Ref(ev))).Times(1);
  text_input()->InsertChar(ev);
}

TEST_F(TextInputTest, InsertCharCtrlV) {
  text_input()->Activate(seat(), surface());

  // CTRL+V is interpreted as non-IME consumed KeyEvent, so should
  // not be sent.
  ui::KeyEvent ev(ui::ET_KEY_PRESSED, ui::VKEY_V, ui::EF_CONTROL_DOWN);
  EXPECT_CALL(*delegate(), SendKey(_)).Times(0);
  text_input()->InsertChar(ev);
}

TEST_F(TextInputTest, InsertCharNormalKey) {
  text_input()->Activate(seat(), surface());

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

  text_input()->Activate(seat(), surface());

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
  EXPECT_TRUE(text_input()->GetEditableSelectionRange(&range));
  EXPECT_EQ(gfx::Range(11, 12).ToString(), range.ToString());
}

TEST_F(TextInputTest, GetTextFromRange) {
  std::u16string text = u"surrounding text";
  text_input()->SetSurroundingText(text, gfx::Range(11, 12));

  const struct {
    gfx::Range range;
    std::u16string expected;
  } kTestCases[] = {
      {gfx::Range(0, 3), u"sur"},
      {gfx::Range(10, 16), u"g text"},
      {gfx::Range(6, 9), u"ndi"},
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

TEST_F(TextInputTest,
       CompositionRangeSetFromCursorWhenSetCompositionTextCalled) {
  text_input()->SetSurroundingText(u"surrounding text", gfx::Range(5, 5));

  std::u16string composition_text = u"composing";
  SetCompositionText(composition_text);

  gfx::Range composition_range;
  EXPECT_TRUE(text_input()->HasCompositionText());
  EXPECT_TRUE(text_input()->GetCompositionTextRange(&composition_range));
  EXPECT_EQ(composition_range, gfx::Range(5, 5 + composition_text.length()));
}

TEST_F(TextInputTest,
       CompositionRangeSetWhenSetCompositionFromExistingTextCalled) {
  text_input()->SetSurroundingText(u"surrounding text", gfx::Range(5, 5));

  text_input()->SetCompositionFromExistingText(gfx::Range(3, 6),
                                               std::vector<ui::ImeTextSpan>{});

  gfx::Range composition_range;
  EXPECT_TRUE(text_input()->HasCompositionText());
  EXPECT_TRUE(text_input()->GetCompositionTextRange(&composition_range));
  EXPECT_EQ(composition_range, gfx::Range(3, 6));
}

TEST_F(TextInputTest, CorrectTextReturnedAfterSetCompositionTextCalled) {
  gfx::Range cursor_pos = gfx::Range(11, 11);
  std::u16string surrounding_text = u"surrounding text";
  std::u16string composition_text = u" and composition";

  ui::CompositionText t = GenerateCompositionText(composition_text);
  EXPECT_CALL(*delegate(), SetCompositionText)
      .WillOnce(
          testing::Invoke([this, cursor_pos, surrounding_text,
                           composition_text](const ui::CompositionText& t) {
            EXPECT_EQ(t.text, composition_text);
            // Simulate surrounding text update from wayland.
            auto before = surrounding_text.substr(0, cursor_pos.GetMin());
            auto after = surrounding_text.substr(cursor_pos.GetMin());
            auto new_surrounding = before + t.text + after;
            auto new_cursor_pos = cursor_pos.GetMin() + t.text.length();
            text_input()->SetSurroundingText(
                new_surrounding, gfx::Range(new_cursor_pos, new_cursor_pos));
          }));

  text_input()->SetSurroundingText(surrounding_text, cursor_pos);
  text_input()->SetCompositionText(t);

  gfx::Range text_range;
  std::u16string text;
  EXPECT_TRUE(text_input()->GetTextRange(&text_range));
  EXPECT_TRUE(text_input()->GetTextFromRange(text_range, &text));
  EXPECT_EQ(text, u"surrounding and composition text");

  gfx::Range composing_text_range;
  std::u16string composing_text;
  EXPECT_TRUE(text_input()->HasCompositionText());
  EXPECT_TRUE(text_input()->GetCompositionTextRange(&composing_text_range));
  EXPECT_TRUE(
      text_input()->GetTextFromRange(composing_text_range, &composing_text));
  EXPECT_EQ(composing_text, u" and composition");
}

TEST_F(TextInputTest, SetsAndGetsGrammarFragmentAtCursor) {
  ui::GrammarFragment sample_fragment(gfx::Range(1, 5), "sample-suggestion");

  text_input()->SetGrammarFragmentAtCursor(absl::nullopt);
  EXPECT_EQ(text_input()->GetGrammarFragmentAtCursor(), absl::nullopt);

  text_input()->SetGrammarFragmentAtCursor(sample_fragment);
  text_input()->SetSurroundingText(u"Sample surrouding text.",
                                   gfx::Range(2, 2));
  EXPECT_EQ(text_input()->GetGrammarFragmentAtCursor(), sample_fragment);
}

TEST_F(TextInputTest, ClearGrammarFragments) {
  std::u16string surrounding_text = u"Sample surrouding text.";
  text_input()->SetSurroundingText(surrounding_text, gfx::Range(2, 2));
  gfx::Range range(3, 8);
  EXPECT_CALL(*delegate(), ClearGrammarFragments(
                               base::StringPiece16(surrounding_text), range))
      .Times(1);
  text_input()->ClearGrammarFragments(range);
}

TEST_F(TextInputTest, AddGrammarFragments) {
  std::u16string surrounding_text = u"Sample surrouding text.";
  text_input()->SetSurroundingText(surrounding_text, gfx::Range(2, 2));
  std::vector<ui::GrammarFragment> fragments = {
      ui::GrammarFragment(gfx::Range(0, 5), "one"),
      ui::GrammarFragment(gfx::Range(10, 16), "two"),
  };
  EXPECT_CALL(
      *delegate(),
      AddGrammarFragment(base::StringPiece16(surrounding_text), fragments[0]))
      .Times(1);
  EXPECT_CALL(
      *delegate(),
      AddGrammarFragment(base::StringPiece16(surrounding_text), fragments[1]))
      .Times(1);
  text_input()->AddGrammarFragments(fragments);
}

TEST_F(TextInputTest, GetAutocorrect) {
  std::u16string surrounding_text = u"Sample surrouding text.";
  text_input()->SetSurroundingText(surrounding_text, gfx::Range(2, 2));
  std::vector<ui::GrammarFragment> fragments = {
      ui::GrammarFragment(gfx::Range(0, 5), "one"),
      ui::GrammarFragment(gfx::Range(10, 16), "two"),
  };
  EXPECT_CALL(
      *delegate(),
      AddGrammarFragment(base::StringPiece16(surrounding_text), fragments[0]))
      .Times(1);
  EXPECT_CALL(
      *delegate(),
      AddGrammarFragment(base::StringPiece16(surrounding_text), fragments[1]))
      .Times(1);
  text_input()->AddGrammarFragments(fragments);
}

}  // anonymous namespace
}  // namespace exo
