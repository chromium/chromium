// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_service.h"

#include <memory>
#include <set>
#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/mojom/ime.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/dummy_input_method.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace arc {

namespace {

class FakeArcImeBridge : public ArcImeBridge {
 public:
  FakeArcImeBridge()
      : count_send_insert_text_(0), last_keyboard_availability_(false) {}

  void SendSetCompositionText(const ui::CompositionText& composition) override {
  }
  void SendConfirmCompositionText() override {
  }
  void SendSelectionRange(const gfx::Range& selection_range) override {
    selection_range_ = selection_range;
  }
  void SendInsertText(const base::string16& text) override {
    count_send_insert_text_++;
  }
  void SendExtendSelectionAndDelete(size_t before, size_t after) override {
  }
  void SendOnKeyboardAppearanceChanging(const gfx::Rect& new_bounds,
                                        bool is_available) override {
    last_keyboard_bounds_ = new_bounds;
    last_keyboard_availability_ = is_available;
  }
  void SendSetComposingRegion(const gfx::Range& composing_range) override {
    composing_range_ = composing_range;
  }

  int count_send_insert_text() const { return count_send_insert_text_; }
  const gfx::Rect& last_keyboard_bounds() const {
    return last_keyboard_bounds_;
  }
  bool last_keyboard_availability() const {
    return last_keyboard_availability_;
  }
  gfx::Range selection_range() { return selection_range_; }
  gfx::Range composing_range() { return composing_range_; }

 private:
  int count_send_insert_text_;
  gfx::Rect last_keyboard_bounds_;
  bool last_keyboard_availability_;
  gfx::Range selection_range_;
  gfx::Range composing_range_;
};

class FakeInputMethod : public ui::DummyInputMethod {
 public:
  FakeInputMethod()
      : client_(nullptr),
        count_show_ime_if_needed_(0),
        count_cancel_composition_(0),
        count_set_focused_text_input_client_(0),
        count_on_text_input_type_changed_(0),
        count_on_caret_bounds_changed_(0) {}

  void SetFocusedTextInputClient(ui::TextInputClient* client) override {
    count_set_focused_text_input_client_++;
    client_ = client;
  }

  ui::TextInputClient* GetTextInputClient() const override {
    return client_;
  }

  void ShowVirtualKeyboardIfEnabled() override { count_show_ime_if_needed_++; }

  void CancelComposition(const ui::TextInputClient* client) override {
    if (client == client_)
      count_cancel_composition_++;
  }

  void DetachTextInputClient(ui::TextInputClient* client) override {
    if (client_ == client)
      client_ = nullptr;
  }

  void OnTextInputTypeChanged(const ui::TextInputClient* client) override {
    count_on_text_input_type_changed_++;
  }

  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {
    count_on_caret_bounds_changed_++;
  }

  int count_show_ime_if_needed() const {
    return count_show_ime_if_needed_;
  }

  int count_cancel_composition() const {
    return count_cancel_composition_;
  }

  int count_set_focused_text_input_client() const {
    return count_set_focused_text_input_client_;
  }

  int count_on_text_input_type_changed() const {
    return count_on_text_input_type_changed_;
  }

  int count_on_caret_bounds_changed() const {
    return count_on_caret_bounds_changed_;
  }

 private:
  ui::TextInputClient* client_;
  int count_show_ime_if_needed_;
  int count_cancel_composition_;
  int count_set_focused_text_input_client_;
  int count_on_text_input_type_changed_;
  int count_on_caret_bounds_changed_;
};

// Helper class for testing the window focus tracking feature of ArcImeService,
// not depending on the full setup of Exo and Ash.
class FakeArcWindowDelegate : public ArcImeService::ArcWindowDelegate {
 public:
  explicit FakeArcWindowDelegate(ui::InputMethod* input_method)
      : next_id_(0), test_input_method_(input_method) {}

  bool IsInArcAppWindow(const aura::Window* window) const override {
    if (!window)
      return false;
    return arc_window_id_.count(window->id());
  }

  void RegisterFocusObserver() override {}
  void UnregisterFocusObserver() override {}

  ui::InputMethod* GetInputMethodForWindow(
      aura::Window* window) const override {
    return window ? test_input_method_ : nullptr;
  }

  bool IsImeBlocked(aura::Window* window) const override {
    return ime_blocked_;
  }

  std::unique_ptr<aura::Window> CreateFakeArcWindow() {
    const int id = next_id_++;
    arc_window_id_.insert(id);
    return base::WrapUnique(aura::test::CreateTestWindowWithDelegate(
        &dummy_delegate_, id, gfx::Rect(), nullptr));
  }

  std::unique_ptr<aura::Window> CreateFakeNonArcWindow() {
    const int id = next_id_++;
    return base::WrapUnique(aura::test::CreateTestWindowWithDelegate(
        &dummy_delegate_, id, gfx::Rect(), nullptr));
  }

  void set_ime_blocked(bool ime_blocked) { ime_blocked_ = ime_blocked; }

 private:
  aura::test::TestWindowDelegate dummy_delegate_;
  int next_id_;
  std::set<int> arc_window_id_;
  ui::InputMethod* test_input_method_;
  bool ime_blocked_ = false;
};

}  // namespace

class ArcImeServiceTest : public testing::Test {
 public:
  ArcImeServiceTest() = default;

 protected:
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<FakeInputMethod> fake_input_method_;
  std::unique_ptr<ArcImeService> instance_;
  FakeArcImeBridge* fake_arc_ime_bridge_;  // Owned by |instance_|

  FakeArcWindowDelegate* fake_window_delegate_;  // Owned by |instance_|
  std::unique_ptr<aura::Window> arc_win_;

  // Needed by ArcImeService.
  keyboard::KeyboardUIController keyboard_ui_controller_;

 private:
  void SetUp() override {
    arc_bridge_service_ = std::make_unique<ArcBridgeService>();

    fake_input_method_ = std::make_unique<FakeInputMethod>();
    auto delegate =
        std::make_unique<FakeArcWindowDelegate>(fake_input_method_.get());
    fake_window_delegate_ = delegate.get();

    instance_ = base::WrapUnique(new ArcImeService(
        nullptr, arc_bridge_service_.get(), std::move(delegate)));
    fake_arc_ime_bridge_ = new FakeArcImeBridge();
    instance_->SetImeBridgeForTesting(base::WrapUnique(fake_arc_ime_bridge_));

    arc_win_ = fake_window_delegate_->CreateFakeArcWindow();
  }

  void TearDown() override {
    ArcImeService::SetOverrideDefaultDeviceScaleFactorForTesting(base::nullopt);
    arc_win_.reset();
    fake_window_delegate_ = nullptr;
    fake_arc_ime_bridge_ = nullptr;
    instance_.reset();
    fake_input_method_.reset();
    arc_bridge_service_.reset();
  }
};

TEST_F(ArcImeServiceTest, HasCompositionText) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  ui::CompositionText composition;
  composition.text = base::UTF8ToUTF16("nonempty text");

  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->ClearCompositionText();
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->ConfirmCompositionText(/* keep_selection */ false);
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->InsertText(base::UTF8ToUTF16("another text"));
  EXPECT_FALSE(instance_->HasCompositionText());

  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());
  instance_->SetCompositionText(ui::CompositionText());
  EXPECT_FALSE(instance_->HasCompositionText());
}

TEST_F(ArcImeServiceTest, SetEditableSelectionRange) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);
  ui::CompositionText composition;
  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->SetEditableSelectionRange(gfx::Range(3, 8)));
  gfx::Range selection;
  instance_->GetEditableSelectionRange(&selection);
  EXPECT_EQ(gfx::Range(3, 8), selection);

  EXPECT_TRUE(instance_->SetEditableSelectionRange(gfx::Range(2, 4)));
  instance_->GetEditableSelectionRange(&selection);
  EXPECT_EQ(gfx::Range(2, 4), selection);
}

TEST_F(ArcImeServiceTest, ConfirmCompositionText) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  ui::CompositionText composition;
  composition.text = base::UTF8ToUTF16("nonempty text");
  EXPECT_FALSE(instance_->HasCompositionText());
  instance_->SetCompositionText(composition);
  EXPECT_TRUE(instance_->HasCompositionText());

  instance_->SetEditableSelectionRange(gfx::Range(3, 8));

  gfx::Range selection;
  instance_->GetEditableSelectionRange(&selection);
  EXPECT_EQ(gfx::Range(3, 8), selection);
  instance_->ConfirmCompositionText(/* keep_selection */ true);
  selection = gfx::Range();
  instance_->GetEditableSelectionRange(&selection);
  EXPECT_EQ(gfx::Range(3, 8), selection);
}

TEST_F(ArcImeServiceTest, ShowVirtualKeyboardIfEnabled) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE, false,
                                    mojom::TEXT_INPUT_FLAG_NONE);
  ASSERT_EQ(0, fake_input_method_->count_show_ime_if_needed());

  // Text input type change does not imply the show ime request.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT, true,
                                    mojom::TEXT_INPUT_FLAG_NONE);
  EXPECT_EQ(0, fake_input_method_->count_show_ime_if_needed());

  instance_->ShowVirtualKeyboardIfEnabled();
  EXPECT_EQ(1, fake_input_method_->count_show_ime_if_needed());
}

TEST_F(ArcImeServiceTest, CancelComposition) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  // The bridge should forward the cancel event to the input method.
  instance_->OnCancelComposition();
  EXPECT_EQ(1, fake_input_method_->count_cancel_composition());
}

TEST_F(ArcImeServiceTest, InsertChar) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  // When text input type is NONE, the event is not forwarded.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_NONE, false,
                                    mojom::TEXT_INPUT_FLAG_NONE);
  instance_->InsertChar(ui::KeyEvent('a', ui::VKEY_A, ui::DomCode::NONE, 0));
  EXPECT_EQ(0, fake_arc_ime_bridge_->count_send_insert_text());

  // When the bridge is accepting text inputs, forward the event.
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT, true,
                                    mojom::TEXT_INPUT_FLAG_NONE);
  instance_->InsertChar(ui::KeyEvent('a', ui::VKEY_A, ui::DomCode::NONE, 0));
  EXPECT_EQ(1, fake_arc_ime_bridge_->count_send_insert_text());

  // When IME is blocked, the event is not forwarded.
  fake_window_delegate_->set_ime_blocked(true);
  instance_->InsertChar(ui::KeyEvent('a', ui::VKEY_A, ui::DomCode::NONE, 0));
  EXPECT_EQ(1, fake_arc_ime_bridge_->count_send_insert_text());
}

TEST_F(ArcImeServiceTest, WindowFocusTracking) {
  std::unique_ptr<aura::Window> arc_win2 =
      fake_window_delegate_->CreateFakeArcWindow();
  std::unique_ptr<aura::Window> nonarc_win =
      fake_window_delegate_->CreateFakeNonArcWindow();

  // ARC window is focused. ArcImeService is set as the text input client.
  instance_->OnWindowFocused(arc_win_.get(), nullptr);
  EXPECT_EQ(instance_.get(), fake_input_method_->GetTextInputClient());
  EXPECT_EQ(1, fake_input_method_->count_set_focused_text_input_client());

  // Focus is moving between ARC windows. No state change should happen.
  instance_->OnWindowFocused(arc_win2.get(), arc_win_.get());
  EXPECT_EQ(instance_.get(), fake_input_method_->GetTextInputClient());
  EXPECT_EQ(1, fake_input_method_->count_set_focused_text_input_client());

  // Focus moved to a non-ARC window. ArcImeService is detached.
  instance_->OnWindowFocused(nonarc_win.get(), arc_win2.get());
  EXPECT_EQ(nullptr, fake_input_method_->GetTextInputClient());
  EXPECT_EQ(1, fake_input_method_->count_set_focused_text_input_client());

  // Focus came back to an ARC window. ArcImeService is re-attached.
  instance_->OnWindowFocused(arc_win_.get(), nonarc_win.get());
  EXPECT_EQ(instance_.get(), fake_input_method_->GetTextInputClient());
  EXPECT_EQ(2, fake_input_method_->count_set_focused_text_input_client());

  // Focus is moving out.
  instance_->OnWindowFocused(nullptr, arc_win_.get());
  EXPECT_EQ(nullptr, fake_input_method_->GetTextInputClient());
  EXPECT_EQ(2, fake_input_method_->count_set_focused_text_input_client());
}

TEST_F(ArcImeServiceTest, RootWindowChange) {
  std::unique_ptr<aura::Window> dummy_root =
      fake_window_delegate_->CreateFakeNonArcWindow();

  instance_->OnWindowFocused(arc_win_.get(), nullptr);
  EXPECT_EQ(instance_.get(), fake_input_method_->GetTextInputClient());

  // Moving to another root window with that shares the same input method.
  // ArcImeService should keep attached to the IME.
  instance_->OnWindowRemovingFromRootWindow(arc_win_.get(), dummy_root.get());
  EXPECT_EQ(instance_.get(), fake_input_method_->GetTextInputClient());

  // Removed from a root window. It should be detached.
  instance_->OnWindowRemovingFromRootWindow(arc_win_.get(), nullptr);
  EXPECT_NE(instance_.get(), fake_input_method_->GetTextInputClient());

  // Unfocusing afterwards should not cause any trouble like crashing.
  instance_->OnWindowFocused(nullptr, arc_win_.get());
}

TEST_F(ArcImeServiceTest, GetTextFromRange) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  const base::string16 text = base::ASCIIToUTF16("abcdefghijklmn");
  // Assume the cursor is between 'c' and 'd'.
  const uint32_t cursor_pos = 3;
  const gfx::Range text_range(cursor_pos - 1, cursor_pos + 1);
  const base::string16 text_in_range = text.substr(cursor_pos - 1, 2);
  const gfx::Range selection_range(cursor_pos, cursor_pos);

  instance_->OnCursorRectChangedWithSurroundingText(
      gfx::Rect(0, 0, 1, 1), text_range, text_in_range, selection_range,
      true /* is_screen_coordinates */);

  gfx::Range temp;
  instance_->GetTextRange(&temp);
  EXPECT_EQ(text_range, temp);

  base::string16 temp_str;
  instance_->GetTextFromRange(text_range, &temp_str);
  EXPECT_EQ(text_in_range, temp_str);

  instance_->GetEditableSelectionRange(&temp);
  EXPECT_EQ(selection_range, temp);
}

TEST_F(ArcImeServiceTest, OnKeyboardAppearanceChanged) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);
  EXPECT_EQ(gfx::Rect(), fake_arc_ime_bridge_->last_keyboard_bounds());
  EXPECT_FALSE(fake_arc_ime_bridge_->last_keyboard_availability());

  const gfx::Rect keyboard_bounds(0, 480, 1200, 320);
  ash::KeyboardStateDescriptor desc{true, keyboard_bounds, keyboard_bounds,
                                    keyboard_bounds};
  instance_->OnKeyboardAppearanceChanged(desc);
  EXPECT_EQ(keyboard_bounds, fake_arc_ime_bridge_->last_keyboard_bounds());
  EXPECT_TRUE(fake_arc_ime_bridge_->last_keyboard_availability());

  // Change the default scale factor of the internal display.
  const double new_scale_factor = 10.0;
  const gfx::Rect new_keyboard_bounds(
      0 * new_scale_factor, 480 * new_scale_factor, 1200 * new_scale_factor,
      320 * new_scale_factor);
  instance_->SetOverrideDefaultDeviceScaleFactorForTesting(new_scale_factor);

  // Keyboard bounds passed to Android should be changed.
  instance_->OnKeyboardAppearanceChanged(desc);
  EXPECT_EQ(new_keyboard_bounds, fake_arc_ime_bridge_->last_keyboard_bounds());
  EXPECT_TRUE(fake_arc_ime_bridge_->last_keyboard_availability());
}

TEST_F(ArcImeServiceTest, GetCaretBounds) {
  EXPECT_EQ(gfx::Rect(), instance_->GetCaretBounds());

  const gfx::Rect window_rect(123, 321, 100, 100);
  arc_win_->SetBounds(window_rect);
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  const gfx::Rect cursor_rect(10, 12, 2, 8);
  instance_->OnCursorRectChanged(cursor_rect, true);  // screen coordinates
  EXPECT_EQ(cursor_rect, instance_->GetCaretBounds());

  instance_->OnCursorRectChanged(cursor_rect, false);  // window coordinates
  EXPECT_EQ(cursor_rect + window_rect.OffsetFromOrigin(),
            instance_->GetCaretBounds());

  const double new_scale_factor = 10.0;
  const gfx::Rect new_cursor_rect(10 * new_scale_factor, 12 * new_scale_factor,
                                  2 * new_scale_factor, 8 * new_scale_factor);
  instance_->SetOverrideDefaultDeviceScaleFactorForTesting(new_scale_factor);
  instance_->OnCursorRectChanged(new_cursor_rect, true);  // screen coordinates
  EXPECT_EQ(cursor_rect, instance_->GetCaretBounds());

  instance_->OnCursorRectChanged(new_cursor_rect, false);  // window coordinates
  EXPECT_EQ(cursor_rect + window_rect.OffsetFromOrigin(),
            instance_->GetCaretBounds());
}

TEST_F(ArcImeServiceTest, ShouldDoLearning) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  ASSERT_NE(ui::TEXT_INPUT_TYPE_TEXT, instance_->GetTextInputType());
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT, true,
                                    mojom::TEXT_INPUT_FLAG_NONE);
  EXPECT_TRUE(instance_->ShouldDoLearning());
  EXPECT_EQ(1, fake_input_method_->count_on_text_input_type_changed());

  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT, false,
                                    mojom::TEXT_INPUT_FLAG_NONE);
  EXPECT_FALSE(instance_->ShouldDoLearning());
  EXPECT_EQ(2, fake_input_method_->count_on_text_input_type_changed());

  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_URL, false,
                                    mojom::TEXT_INPUT_FLAG_NONE);
  EXPECT_FALSE(instance_->ShouldDoLearning());
  EXPECT_EQ(3, fake_input_method_->count_on_text_input_type_changed());
}

TEST_F(ArcImeServiceTest, DoNothingIfArcWindowIsNotFocused) {
  ASSERT_EQ(0, fake_input_method_->count_show_ime_if_needed());
  ASSERT_EQ(0, fake_input_method_->count_on_text_input_type_changed());
  ASSERT_EQ(0, fake_input_method_->count_on_caret_bounds_changed());
  ASSERT_EQ(0, fake_input_method_->count_cancel_composition());

  instance_->OnWindowFocused(nullptr, nullptr);

  const gfx::Rect cursor_rect(10, 20, 30, 40);
  instance_->OnTextInputTypeChanged(ui::TEXT_INPUT_TYPE_TEXT, true,
                                    mojom::TEXT_INPUT_FLAG_NONE);
  instance_->OnCursorRectChanged(cursor_rect, true);
  instance_->OnCancelComposition();

  EXPECT_EQ(0, fake_input_method_->count_show_ime_if_needed());
  EXPECT_EQ(0, fake_input_method_->count_on_text_input_type_changed());
  EXPECT_EQ(0, fake_input_method_->count_on_caret_bounds_changed());
  EXPECT_EQ(0, fake_input_method_->count_cancel_composition());
}

TEST_F(ArcImeServiceTest, SetComposingRegion) {
  instance_->OnWindowFocused(arc_win_.get(), nullptr);

  const gfx::Range composing_range(1, 3);

  // Ignore it if the range is outside of text range.
  instance_->SetCompositionFromExistingText(composing_range, {});
  EXPECT_EQ(gfx::Range(), fake_arc_ime_bridge_->composing_range());

  instance_->OnCursorRectChangedWithSurroundingText(
      gfx::Rect(), gfx::Range(0, 100), base::string16(100, 'a'),
      gfx::Range(0, 0), false);
  instance_->SetCompositionFromExistingText(composing_range, {});
  EXPECT_EQ(composing_range, fake_arc_ime_bridge_->composing_range());

  // Ignore it if the range is outside of text range.
  instance_->OnCursorRectChangedWithSurroundingText(
      gfx::Rect(), gfx::Range(0, 100), base::string16(100, 'a'),
      gfx::Range(0, 0), false);
  instance_->SetCompositionFromExistingText(gfx::Range(50, 101), {});
  EXPECT_EQ(composing_range, fake_arc_ime_bridge_->composing_range());
}

}  // namespace arc
