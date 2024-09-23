// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/text_input.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/test/ash_test_helper.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/exo/buffer.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager_impl.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/views/widget/widget.h"

using testing::_;

namespace exo {

namespace {

ui::CompositionText GenerateCompositionText(const std::u16string& text) {
  ui::CompositionText t;
  t.text = text;
  t.selection = gfx::Range(text.size());
  t.ime_text_spans.emplace_back(ui::ImeTextSpan::Type::kComposition, 0,
                                t.text.size(),
                                ui::ImeTextSpan::Thickness::kThick);
  return t;
}

class MockTextInputDelegate : public TextInput::Delegate {
 public:
  MockTextInputDelegate() = default;

  MockTextInputDelegate(const MockTextInputDelegate&) = delete;
  MockTextInputDelegate& operator=(const MockTextInputDelegate&) = delete;

  ~MockTextInputDelegate() override = default;

  // TextInput::Delegate:
  MOCK_METHOD(void, Activated, (), (override));
  MOCK_METHOD(void, Deactivated, (), (override));
  MOCK_METHOD(void, OnVirtualKeyboardVisibilityChanged, (bool), (override));
  MOCK_METHOD(void,
              OnVirtualKeyboardOccludedBoundsChanged,
              (const gfx::Rect&),
              (override));
  MOCK_METHOD(bool, SupportsFinalizeVirtualKeyboardChanges, (), (override));
  MOCK_METHOD(void,
              SetCompositionText,
              (const ui::CompositionText&),
              (override));
  MOCK_METHOD(void, Commit, (std::u16string_view), (override));
  MOCK_METHOD(void,
              SetCursor,
              (std::u16string_view, const gfx::Range&),
              (override));
  MOCK_METHOD(void,
              DeleteSurroundingText,
              (std::u16string_view, const gfx::Range&),
              (override));
  MOCK_METHOD(void, SendKey, (const ui::KeyEvent&), (override));
  MOCK_METHOD(void,
              OnTextDirectionChanged,
              (base::i18n::TextDirection direction),
              (override));
  MOCK_METHOD(void,
              SetCompositionFromExistingText,
              (std::u16string_view,
               const gfx::Range&,
               const gfx::Range&,
               const std::vector<ui::ImeTextSpan>& ui_ime_text_spans),
              (override));
  MOCK_METHOD(void,
              ClearGrammarFragments,
              (std::u16string_view, const gfx::Range&),
              (override));
  MOCK_METHOD(void,
              AddGrammarFragment,
              (std::u16string_view, const ui::GrammarFragment&),
              (override));
  MOCK_METHOD(void,
              SetAutocorrectRange,
              (std::u16string_view, const gfx::Range&),
              (override));
  MOCK_METHOD(bool, ConfirmComposition, (bool), (override));
  MOCK_METHOD(bool, SupportsConfirmPreedit, (), (override));
  MOCK_METHOD(bool, HasImageInsertSupport, (), (override));
  MOCK_METHOD(void, InsertImage, (const GURL& src), (override));
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
  MOCK_METHOD(void, OnFocus, (), (override));
  MOCK_METHOD(void, OnBlur, (), (override));
  MOCK_METHOD(void,
              OnCaretBoundsChanged,
              (const ui::TextInputClient*),
              (override));
  MOCK_METHOD(void,
              OnTextInputStateChanged,
              (const ui::TextInputClient*),
              (override));
  MOCK_METHOD(void,
              OnInputMethodDestroyed,
              (const ui::InputMethod*),
              (override));
  MOCK_METHOD(void,
              OnVirtualKeyboardVisibilityChangedIfEnabled,
              (bool),
              (override));

 private:
  raw_ptr<ui::InputMethod> input_method_ = nullptr;
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
    text_input_ = std::make_unique<TextInput>(
        std::make_unique<testing::NiceMock<MockTextInputDelegate>>());
    seat_ = std::make_unique<Seat>();
    test_surface_.SetUp(exo_test_helper());

    ON_CALL(*delegate(), SupportsFinalizeVirtualKeyboardChanges())
        .WillByDefault(testing::Return(false));
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
  buffer_ = exo_test_helper->CreateBuffer(buffer_size);
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
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
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
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
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
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Attempting to activate the same surface is a no-op.
  EXPECT_CALL(*delegate(), Activated).Times(0);
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Activating a non-focused surface causes deactivation until focus.
  EXPECT_CALL(observer, OnTextInputStateChanged(nullptr)).Times(1);
  EXPECT_CALL(*delegate(), Deactivated).Times(1);
  text_input()->Activate(seat(), surface2.surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
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
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
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
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
}

TEST_F(TextInputTest, VirtualKeyboardObserver) {
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_DEFAULT, text_input()->GetTextInputMode());

  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_TEXT, text_input()->GetTextInputMode());
  EXPECT_EQ(0, text_input()->GetTextInputFlags());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
  text_input()->Deactivate();
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_DEFAULT, text_input()->GetTextInputMode());

  // Destroy the text_input.
  DestroyTextInput();
}

TEST_F(TextInputTest, SetTypeModeFlag) {
  TestingInputMethodObserver observer(GetInputMethod());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_TEXT, text_input()->GetTextInputMode());
  EXPECT_EQ(0, text_input()->GetTextInputFlags());
  EXPECT_TRUE(text_input()->ShouldDoLearning());
  EXPECT_TRUE(text_input()->CanComposeInline());

  int flags = ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_OFF |
              ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE;
  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  text_input()->SetTypeModeFlags(
      ui::TEXT_INPUT_TYPE_URL, ui::TEXT_INPUT_MODE_URL, flags,
      /*should_do_learning=*/false, /*can_compose_inline=*/false,
      /*surrounding_text_supported=*/true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, text_input()->GetTextInputType());
  EXPECT_EQ(ui::TEXT_INPUT_MODE_URL, text_input()->GetTextInputMode());
  EXPECT_EQ(flags, text_input()->GetTextInputFlags());
  EXPECT_FALSE(text_input()->ShouldDoLearning());
  EXPECT_FALSE(text_input()->CanComposeInline());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
}

TEST_F(TextInputTest, FocusReason) {
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            text_input()->GetFocusReason());

  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_OTHER,
            text_input()->GetFocusReason());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
  text_input()->Deactivate();
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_PEN);
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_PEN,
            text_input()->GetFocusReason());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
}

TEST_F(TextInputTest, CaretBounds) {
  TestingInputMethodObserver observer(GetInputMethod());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
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

TEST_F(TextInputTest, ConfirmCompositionTextDontKeepSelection) {
  SetCompositionText(u"composition");

  EXPECT_CALL(*delegate(), ConfirmComposition(/*keep_selection=*/false))
      .Times(1);

  const size_t composition_text_length =
      text_input()->ConfirmCompositionText(/*keep_selection=*/false);
  EXPECT_EQ(composition_text_length, 11u);
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Second call should be the empty commit string.
  EXPECT_EQ(0u, text_input()->ConfirmCompositionText(/*keep_selection=*/false));
  EXPECT_FALSE(text_input()->HasCompositionText());
}

TEST_F(TextInputTest, ConfirmCompositionTextKeepSelection) {
  constexpr char16_t kCompositionText[] = u"composition";
  SetCompositionText(kCompositionText);
  text_input()->SetEditableSelectionRange(gfx::Range(2, 3));
  text_input()->SetSurroundingText(kCompositionText, 0u, gfx::Range(2, 3),
                                   std::nullopt, std::nullopt);

  EXPECT_CALL(*delegate(), ConfirmComposition(/*keep_selection=*/true))
      .Times(1);

  const uint32_t composition_text_length =
      text_input()->ConfirmCompositionText(/*keep_selection=*/true);
  EXPECT_EQ(composition_text_length, static_cast<uint32_t>(11));
  testing::Mock::VerifyAndClearExpectations(delegate());

  // Second call should be the empty commit string.
  EXPECT_EQ(0u, text_input()->ConfirmCompositionText(/*keep_selection=*/true));
  EXPECT_FALSE(text_input()->HasCompositionText());
}

TEST_F(TextInputTest, ResetCompositionText) {
  SetCompositionText(u"composition");

  text_input()->Reset();
  EXPECT_EQ(0u, text_input()->ConfirmCompositionText(/*keep_selection=*/false));
  EXPECT_FALSE(text_input()->HasCompositionText());
}

TEST_F(TextInputTest, Commit) {
  constexpr char16_t s[] = u"commit text";

  EXPECT_CALL(*delegate(), Commit(std::u16string_view(s))).Times(1);
  text_input()->InsertText(
      s, ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_FALSE(text_input()->HasCompositionText());
}

TEST_F(TextInputTest, InsertChar) {
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);

  ui::KeyEvent ev(ui::EventType::kKeyPressed, ui::VKEY_RETURN, 0);
  ui::SetKeyboardImeFlags(&ev, ui::kPropertyKeyboardImeHandledFlag);

  EXPECT_CALL(*delegate(), SendKey(testing::Ref(ev))).Times(1);
  text_input()->InsertChar(ev);
}

TEST_F(TextInputTest, InsertCharCtrlV) {
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);

  // CTRL+V is interpreted as non-IME consumed KeyEvent, so should
  // not be sent.
  ui::KeyEvent ev(ui::EventType::kKeyPressed, ui::VKEY_V, ui::EF_CONTROL_DOWN);
  EXPECT_CALL(*delegate(), SendKey(_)).Times(0);
  text_input()->InsertChar(ev);
}

TEST_F(TextInputTest, InsertCharNormalKey) {
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);

  char16_t ch = 'x';
  ui::KeyEvent ev =
      ui::KeyEvent::FromCharacter(ch, ui::VKEY_X, ui::DomCode::US_X, 0);
  ui::SetKeyboardImeFlags(&ev, ui::kPropertyKeyboardImeHandledFlag);

  EXPECT_CALL(*delegate(), SendKey(testing::Ref(ev))).Times(1);
  text_input()->InsertChar(ev);
}

TEST_F(TextInputTest, InsertCharNumpadEqual) {
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);

  // NUMPAD_EQUAL is set key_code to VKEY_UNKNOWN, but code t- NUMPAD_EQUAL.
  ui::KeyEvent ev(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN,
                  ui::DomCode::NUMPAD_EQUAL, /*flags=*/0, /*key=*/0,
                  base::TimeTicks());
  ev.set_character(u'=');

  // InsertChar should ignore it (because it is not consumed by IME),
  // and exo::Keyboard is expected to handle the case.
  EXPECT_CALL(*delegate(), SendKey(_)).Times(0);
  EXPECT_CALL(*delegate(), Commit(_)).Times(0);
  text_input()->InsertChar(ev);
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(TextInputTest, SurroundingText) {
  TestingInputMethodObserver observer(GetInputMethod());

  gfx::Range range;
  EXPECT_TRUE(text_input()->GetTextRange(&range));
  EXPECT_EQ(gfx::Range(0, 0), range);
  EXPECT_FALSE(text_input()->GetCompositionTextRange(&range));
  EXPECT_TRUE(text_input()->GetEditableSelectionRange(&range));
  EXPECT_EQ(gfx::Range(0, 0), range);
  std::u16string got_text;
  EXPECT_FALSE(text_input()->GetTextFromRange(gfx::Range(0, 1), &got_text));

  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);

  EXPECT_CALL(observer, OnCaretBoundsChanged(text_input())).Times(1);
  std::u16string text = u"surrounding\u3000text";
  text_input()->SetSurroundingText(text, 0u, gfx::Range(11, 12), std::nullopt,
                                   std::nullopt);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(text_input()->GetTextRange(&range));
  EXPECT_EQ(gfx::Range(0, text.size()).ToString(), range.ToString());

  EXPECT_FALSE(text_input()->GetCompositionTextRange(&range));
  EXPECT_TRUE(text_input()->GetEditableSelectionRange(&range));
  EXPECT_EQ(gfx::Range(11, 12).ToString(), range.ToString());
  EXPECT_TRUE(text_input()->GetTextFromRange(gfx::Range(11, 12), &got_text));
  EXPECT_EQ(text.substr(11, 1), got_text);

  EXPECT_CALL(*delegate(), DeleteSurroundingText(std::u16string_view(text),
                                                 gfx::Range(11, 12)))
      .Times(1);
  text_input()->ExtendSelectionAndDelete(0, 0);
  testing::Mock::VerifyAndClearExpectations(delegate());

  size_t composition_size = std::string("composition").size();
  SetCompositionText(u"composition");
  EXPECT_TRUE(text_input()->GetCompositionTextRange(&range));
  EXPECT_EQ(gfx::Range(11, 11 + composition_size), range);
  EXPECT_TRUE(text_input()->GetEditableSelectionRange(&range));
  EXPECT_EQ(gfx::Range(11 + composition_size), range);
}

TEST_F(TextInputTest, SetEditableSelectionRange) {
  SetCompositionText(u"text");
  text_input()->SetSurroundingText(u"text", 0u, gfx::Range(4, 4), std::nullopt,
                                   std::nullopt);

  // Should commit composition text and set selection range.
  EXPECT_CALL(*delegate(),
              SetCursor(std::u16string_view(u"text"), gfx::Range(0, 3)))
      .Times(1);
  EXPECT_CALL(*delegate(), Commit(std::u16string_view(u"text"))).Times(1);
  EXPECT_TRUE(text_input()->SetEditableSelectionRange(gfx::Range(0, 3)));
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(TextInputTest, GetTextFromRange) {
  std::u16string text = u"surrounding text";
  text_input()->SetEditableSelectionRange(gfx::Range(11, 12));
  text_input()->SetSurroundingText(text, 0u, gfx::Range(11, 12), std::nullopt,
                                   std::nullopt);

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

TEST_F(TextInputTest, GetTextFromRangeWithOffset) {
  std::u16string text = u"surrounding text";
  text_input()->SetEditableSelectionRange(gfx::Range(11, 12));
  text_input()->SetSurroundingText(text, 5u, gfx::Range(11, 12), std::nullopt,
                                   std::nullopt);

  gfx::Range text_range;
  ASSERT_TRUE(text_input()->GetTextRange(&text_range));
  EXPECT_EQ(gfx::Range(5, 21), text_range);

  const struct {
    gfx::Range range;
    std::u16string expected;
  } kTestCases[] = {
      {gfx::Range(5, 8), u"sur"},
      {gfx::Range(15, 21), u"g text"},
      {gfx::Range(11, 14), u"ndi"},
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

  text_input()->SetSurroundingText(u"surrounding text", 0u, gfx::Range(5, 5),
                                   std::nullopt, std::nullopt);

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
  text_input()->SetSurroundingText(u"surrounding text", 0u, gfx::Range(5, 5),
                                   std::nullopt, std::nullopt);

  std::u16string composition_text = u"composing";
  SetCompositionText(composition_text);

  gfx::Range composition_range;
  EXPECT_TRUE(text_input()->HasCompositionText());
  EXPECT_TRUE(text_input()->GetCompositionTextRange(&composition_range));
  EXPECT_EQ(composition_range, gfx::Range(5, 5 + composition_text.length()));
}

TEST_F(TextInputTest,
       CompositionRangeSetWhenSetCompositionFromExistingTextCalled) {
  text_input()->SetSurroundingText(u"surrounding text", 0u, gfx::Range(5, 5),
                                   std::nullopt, std::nullopt);

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
  EXPECT_CALL(*delegate(), SetCompositionText(_)).Times(1);

  text_input()->SetSurroundingText(surrounding_text, 0u, cursor_pos,
                                   std::nullopt, std::nullopt);
  text_input()->SetCompositionText(t);

  // Simulate surrounding text update from wayland.
  auto before = surrounding_text.substr(0, cursor_pos.GetMin());
  auto after = surrounding_text.substr(cursor_pos.GetMin());
  auto new_surrounding = before + t.text + after;
  auto new_cursor_pos = cursor_pos.GetMin() + t.text.length();
  text_input()->SetSurroundingText(new_surrounding, 0u,
                                   gfx::Range(new_cursor_pos, new_cursor_pos),
                                   std::nullopt, std::nullopt);

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

  EXPECT_EQ(text_input()->GetGrammarFragmentAtCursor(), std::nullopt);
  text_input()->SetSurroundingText(u"Sample surrouding text.", 0u,
                                   gfx::Range(2, 2), sample_fragment,
                                   std::nullopt);
  EXPECT_EQ(text_input()->GetGrammarFragmentAtCursor(), sample_fragment);
}

TEST_F(TextInputTest, ClearGrammarFragments) {
  std::u16string surrounding_text = u"Sample surrouding text.";
  text_input()->SetSurroundingText(surrounding_text, 0u, gfx::Range(2, 2),
                                   std::nullopt, std::nullopt);
  gfx::Range range(3, 8);
  EXPECT_CALL(*delegate(), ClearGrammarFragments(
                               std::u16string_view(surrounding_text), range))
      .Times(1);
  text_input()->ClearGrammarFragments(range);
}

TEST_F(TextInputTest, AddGrammarFragments) {
  std::u16string surrounding_text = u"Sample surrouding text.";
  text_input()->SetSurroundingText(surrounding_text, 0u, gfx::Range(2, 2),
                                   std::nullopt, std::nullopt);
  std::vector<ui::GrammarFragment> fragments = {
      ui::GrammarFragment(gfx::Range(0, 5), "one"),
      ui::GrammarFragment(gfx::Range(10, 16), "two"),
  };
  EXPECT_CALL(
      *delegate(),
      AddGrammarFragment(std::u16string_view(surrounding_text), fragments[0]))
      .Times(1);
  EXPECT_CALL(
      *delegate(),
      AddGrammarFragment(std::u16string_view(surrounding_text), fragments[1]))
      .Times(1);
  text_input()->AddGrammarFragments(fragments);
}

TEST_F(TextInputTest, GetAutocorrect) {
  std::u16string surrounding_text = u"Sample surrouding text.";
  text_input()->SetSurroundingText(surrounding_text, 0u, gfx::Range(2, 2),
                                   std::nullopt, std::nullopt);
  std::vector<ui::GrammarFragment> fragments = {
      ui::GrammarFragment(gfx::Range(0, 5), "one"),
      ui::GrammarFragment(gfx::Range(10, 16), "two"),
  };
  EXPECT_CALL(
      *delegate(),
      AddGrammarFragment(std::u16string_view(surrounding_text), fragments[0]))
      .Times(1);
  EXPECT_CALL(
      *delegate(),
      AddGrammarFragment(std::u16string_view(surrounding_text), fragments[1]))
      .Times(1);
  text_input()->AddGrammarFragments(fragments);
}

TEST_F(TextInputTest, EnsureCaretNotInRect) {
  const gfx::Rect bounds(10, 20, 300, 400);
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged(bounds));
  text_input()->EnsureCaretNotInRect(bounds);
}

TEST_F(TextInputTest, OnKeyboardHidden) {
  const gfx::Rect bounds;
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged(bounds));
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged(false));
  text_input()->OnKeyboardHidden();
}

TEST_F(TextInputTest, FinalizeVirtualKeyboardChangesNotSupported) {
  EXPECT_CALL(*delegate(), SupportsFinalizeVirtualKeyboardChanges())
      .WillRepeatedly(testing::Return(false));

  const gfx::Rect kBounds(10, 20, 300, 400);

  testing::InSequence s;
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged(true));
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged(kBounds));
  text_input()->OnKeyboardVisible(gfx::Rect());
  text_input()->EnsureCaretNotInRect(kBounds);

  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged(gfx::Rect()));
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged(false));
  text_input()->OnKeyboardHidden();
}

TEST_F(TextInputTest, FinalizeVirtualKeyboardChanges) {
  EXPECT_CALL(*delegate(), SupportsFinalizeVirtualKeyboardChanges())
      .WillRepeatedly(testing::Return(true));

  const gfx::Rect kBounds(10, 20, 300, 400);
  const gfx::Rect kBounds2(20, 40, 500, 600);

  testing::InSequence s;
  // After the client requests a vk change, the server buffers vk updates.
  text_input()->ShowVirtualKeyboardIfEnabled();
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged).Times(0);
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged).Times(0);
  text_input()->OnKeyboardVisible(gfx::Rect());
  text_input()->EnsureCaretNotInRect(kBounds);

  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged(true));
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged(kBounds));
  text_input()->FinalizeVirtualKeyboardChanges();

  // The server can update the client immediately if the client hasn't requested
  // any new changes.
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged).Times(0);
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged(kBounds2));
  text_input()->EnsureCaretNotInRect(kBounds2);

  // The client requests to hide vk.
  text_input()->HideVirtualKeyboard();
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged).Times(0);
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged).Times(0);
  text_input()->OnKeyboardHidden();

  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged(gfx::Rect()));
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged(false));
  text_input()->FinalizeVirtualKeyboardChanges();
}

TEST_F(TextInputTest, FinalizeVirtualKeyboardChangesWithMultipleChanges) {
  EXPECT_CALL(*delegate(), SupportsFinalizeVirtualKeyboardChanges())
      .WillRepeatedly(testing::Return(true));

  const gfx::Rect kBounds(10, 20, 300, 400);
  const gfx::Rect kBounds2(20, 40, 500, 600);
  const gfx::Rect kBounds3(30, 50, 200, 100);

  testing::InSequence s;
  // After the client requests a vk change, the server buffers vk updates.
  text_input()->ShowVirtualKeyboardIfEnabled();
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged).Times(0);
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged).Times(0);
  text_input()->OnKeyboardVisible(gfx::Rect());
  text_input()->EnsureCaretNotInRect(kBounds);
  text_input()->EnsureCaretNotInRect(kBounds2);
  text_input()->OnKeyboardHidden();
  text_input()->OnKeyboardVisible(gfx::Rect());
  text_input()->EnsureCaretNotInRect(kBounds3);

  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged(true));
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged(kBounds3));
  text_input()->FinalizeVirtualKeyboardChanges();
}

TEST_F(TextInputTest, FinalizeVirtualKeyboardChangesDoesntSendStaleBounds) {
  EXPECT_CALL(*delegate(), SupportsFinalizeVirtualKeyboardChanges())
      .WillRepeatedly(testing::Return(true));

  const gfx::Rect kBounds(10, 20, 300, 400);
  const gfx::Rect kBounds2(20, 40, 500, 600);

  testing::InSequence s;
  // After the client requests a vk change, the server buffers vk updates.
  text_input()->ShowVirtualKeyboardIfEnabled();
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged).Times(0);
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged).Times(0);
  text_input()->OnKeyboardVisible(gfx::Rect());
  text_input()->EnsureCaretNotInRect(kBounds);
  text_input()->OnKeyboardHidden();
  text_input()->OnKeyboardVisible(gfx::Rect());

  // Showing vk invalidates any previously staged bounds.
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged(true));
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged).Times(0);
  text_input()->FinalizeVirtualKeyboardChanges();

  // Bounds update doesn't change vk visibility.
  EXPECT_CALL(*delegate(), OnVirtualKeyboardVisibilityChanged).Times(0);
  EXPECT_CALL(*delegate(), OnVirtualKeyboardOccludedBoundsChanged(kBounds2));
  text_input()->EnsureCaretNotInRect(kBounds2);
}

TEST_F(TextInputTest, SetSurroundingTextSupport) {
  testing::NiceMock<TestingInputMethodObserver> observer(GetInputMethod());

  constexpr bool kShouldDoLearning = true;
  constexpr bool kCanComposeInline = true;

  ash::input_method::MockInputMethodManagerImpl* input_method_manager =
      ash_test_helper()->input_method_manager();

  constexpr char kEnglishId[] =
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng";
  constexpr char kJapaneseId[] =
      "_comp_ime_jkghodnilhceideoidjikpgommlajknknacl_mozc_jp";

  input_method_manager->SetCurrentInputMethodId(kEnglishId);

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  EXPECT_CALL(*delegate(), Activated).Times(1);
  text_input()->Activate(seat(), surface(),
                         ui::TextInputClient::FOCUS_REASON_OTHER);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate());

  // When a client does not support surrounding text, we force a NULL input type
  // for xkb input methods like English.

  text_input()->SetTypeModeFlags(
      ui::TEXT_INPUT_TYPE_SEARCH, ui::TEXT_INPUT_MODE_SEARCH,
      ui::TEXT_INPUT_FLAG_NONE, kShouldDoLearning, kCanComposeInline,
      /*surrounding_text_supported=*/false);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NULL, text_input()->GetTextInputType());

  // When switching to a non-xkb input method, we restore the original input
  // type requested.

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  input_method_manager->SetCurrentInputMethodId(kJapaneseId);
  // The MockInputMethodManagerImpl doesn't currently fire this observer
  // callback, so do it explicitly.
  text_input()->InputMethodChanged(nullptr, nullptr, false);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_SEARCH, text_input()->GetTextInputType());

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  input_method_manager->SetCurrentInputMethodId(kEnglishId);
  text_input()->InputMethodChanged(nullptr, nullptr, false);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NULL, text_input()->GetTextInputType());

  // When surrounding text is supported, the requested input type is left
  // untouched.

  EXPECT_CALL(observer, OnTextInputStateChanged(text_input())).Times(1);
  text_input()->SetTypeModeFlags(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_TEXT,
      ui::TEXT_INPUT_FLAG_NONE, kShouldDoLearning, kCanComposeInline,
      /*surrounding_text_supported=*/true);
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, text_input()->GetTextInputType());

  EXPECT_CALL(*delegate(), Deactivated).Times(1);
}

}  // anonymous namespace
}  // namespace exo
