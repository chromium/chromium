// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"

#include <memory>
#include <string_view>

#include "base/time/time.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {
namespace {

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::NiceMock;

class MockDelegate : public PopupSearchBarView::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;
  MOCK_METHOD(void,
              SearchBarOnInputChanged,
              (std::u16string_view text),
              (override));
  MOCK_METHOD(void, SearchBarOnFocusLost, (), (override));
  MOCK_METHOD(bool,
              SearchBarHandleKeyPressed,
              (const ui::KeyEvent& event),
              (override));
};

class PopupSearchBarViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(widget_.get()));
  }

  void TearDown() override {
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::Widget& widget() { return *widget_; }
  ui::test::EventGenerator& generator() { return *generator_; }
  MockDelegate& delegate() { return delegate_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  NiceMock<MockDelegate> delegate_;
};

TEST_F(PopupSearchBarViewTest, SetsFocusOnTextfield) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();
  view->Focus();

  views::View* focused_field = widget().GetFocusManager()->GetFocusedView();
  ASSERT_NE(focused_field, nullptr);
  EXPECT_EQ(focused_field->GetClassName(), "SearchBarTextfield");
}

TEST_F(PopupSearchBarViewTest, OnFocusLostCalled) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();
  view->Focus();
  ASSERT_NE(widget().GetFocusManager()->GetFocusedView(), nullptr);

  EXPECT_CALL(delegate(), SearchBarOnFocusLost);
  widget().GetFocusManager()->SetFocusedView(nullptr);
}

TEST_F(PopupSearchBarViewTest, OnInputChangedIsCalledAfterDelay) {
  auto view = std::make_unique<PopupSearchBarView>(
      u"placeholder", /*initial_value=*/u"", delegate());

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(delegate(), SearchBarOnInputChanged(Eq(u"input text")));
  }

  view->SetInputTextForTesting(u"input text");
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay / 2);
  check.Call();
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay / 2);
}

// Verifies that when `debounce_delay` is zero, text input changes notify the
// delegate on the current tick without advancing mock time.
TEST_F(PopupSearchBarViewTest, OnInputChangedIsCalledImmediatelyWithZeroDelay) {
  auto view = std::make_unique<PopupSearchBarView>(
      u"placeholder", /*initial_value=*/u"", delegate(),
      /*show_indicator=*/false,
      /*show_search_icon_sparkle=*/false, /*debounce_delay=*/base::TimeDelta());

  EXPECT_CALL(delegate(), SearchBarOnInputChanged(Eq(u"input text")));
  view->SetInputTextForTesting(u"input text");
  task_environment()->RunUntilIdle();
}

TEST_F(PopupSearchBarViewTest, OnInputChangedCallbackIsThrottled) {
  auto view = std::make_unique<PopupSearchBarView>(
      u"placeholder", /*initial_value=*/u"", delegate());

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(delegate(), SearchBarOnInputChanged(Eq(u"input text 2")));
  }

  view->SetInputTextForTesting(u"input text");
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay / 2);
  check.Call();
  view->SetInputTextForTesting(u"input text 2");
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay);
}

// TODO(crbug.com/338934966): Enable when key events suppressing in tests is
// fixed.
#if !BUILDFLAG(IS_WIN)
TEST_F(PopupSearchBarViewTest, KeyPressedFromTextfieldPassedToDelegateFirst) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();
  view->Focus();

  // Set up "a" suppressing handler.
  ON_CALL(delegate(), SearchBarHandleKeyPressed)
      .WillByDefault([](const ui::KeyEvent& event) {
        return event.key_code() == ui::VKEY_A ? true : false;
      });
  // As "a" is suppressed, only "bc" is expected.
  EXPECT_CALL(delegate(), SearchBarOnInputChanged(Eq(u"bc")));

  generator().PressAndReleaseKey(ui::VKEY_A);
  generator().PressAndReleaseKey(ui::VKEY_B);
  generator().PressAndReleaseKey(ui::VKEY_C);

  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay);
}
#endif  // !BUILDFLAG(IS_WIN)

TEST_F(PopupSearchBarViewTest, ClearButton) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();
  view->Focus();

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(delegate(), SearchBarOnInputChanged(Eq(u"abc")));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(delegate(), SearchBarOnInputChanged(Eq(u"")));
  }

  view->SetInputTextForTesting(u"abc");
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay);

  check.Call();

  widget().LayoutRootViewIfNecessary();
  generator().MoveMouseTo(view->GetClearButtonScreenCenterPointForTesting());
  generator().ClickLeftButton();
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay);
}

TEST_F(PopupSearchBarViewTest, ClearButtonVisibility) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();

  EXPECT_FALSE(view->IsClearButtonVisibleForTesting());

  view->SetInputTextForTesting(u"a");
  EXPECT_TRUE(view->IsClearButtonVisibleForTesting());

  view->SetInputTextForTesting(u"");
  EXPECT_FALSE(view->IsClearButtonVisibleForTesting());
}

TEST_F(PopupSearchBarViewTest, IndicatorVisibility_Enabled) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate(),
          /*show_indicator=*/true));
  widget().Show();

  EXPECT_TRUE(view->IsIndicatorVisibleForTesting());

  view->SetInputTextForTesting(u"a");
  EXPECT_FALSE(view->IsIndicatorVisibleForTesting());

  view->SetInputTextForTesting(u"");
  EXPECT_TRUE(view->IsIndicatorVisibleForTesting());
}

TEST_F(PopupSearchBarViewTest, IndicatorVisibility_Disabled) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate(),
          /*show_indicator=*/false));
  widget().Show();

  EXPECT_FALSE(view->IsIndicatorVisibleForTesting());

  view->SetInputTextForTesting(u"a");
  EXPECT_FALSE(view->IsIndicatorVisibleForTesting());

  view->SetInputTextForTesting(u"");
  EXPECT_FALSE(view->IsIndicatorVisibleForTesting());
}

TEST_F(PopupSearchBarViewTest, InitialText) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", u"initial query", delegate(),
          /*show_indicator=*/false,
          /*show_search_icon_sparkle=*/false,
          /*debounce_delay=*/PopupSearchBarView::kInputChangeCallbackDelay));
  widget().Show();

  EXPECT_EQ(view->GetText(), u"initial query");
}

TEST_F(PopupSearchBarViewTest, SetLoading) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();

  view->SetLoading(true);
  EXPECT_TRUE(view->GetThrobberForTesting()->GetVisible());
  EXPECT_FALSE(view->GetSearchIconForTesting()->GetVisible());

  view->SetLoading(false);
  EXPECT_FALSE(view->GetThrobberForTesting()->GetVisible());
  EXPECT_TRUE(view->GetSearchIconForTesting()->GetVisible());
}

// Tests that pressing the Enter (VKEY_RETURN) key synchronously stops the
// debounced input changed timer, preventing any trailing incremental queries
// from executing after a full search is submitted.
TEST_F(PopupSearchBarViewTest, PressingEnterStopsInputChangedTimer) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();
  view->Focus();

  // We expect the Enter key to be passed to the delegate, and we return true.
  EXPECT_CALL(delegate(), SearchBarHandleKeyPressed)
      .WillOnce([](const ui::KeyEvent& event) {
        return event.key_code() == ui::VKEY_RETURN;
      });

  // Because Enter stops the timer, SearchBarOnInputChanged should never be
  // called.
  EXPECT_CALL(delegate(), SearchBarOnInputChanged).Times(0);

  // Set input text, starting the debouncing timer.
  view->SetInputTextForTesting(u"input text");

  // Simulate pressing Enter on the focused textfield via event generator.
  generator().PressAndReleaseKey(ui::VKEY_RETURN);

  // Fast forward by the full delay, and verify that the callback was not
  // called.
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay);
}

// Tests that pressing TAB cycles focus between the textfield and clear button
// when the clear button is visible.
TEST_F(PopupSearchBarViewTest, TabKeyCyclesToClearButtonWhenVisible) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();
  view->Focus();

  EXPECT_EQ(widget().GetFocusManager()->GetFocusedView()->GetClassName(),
            "SearchBarTextfield");

  // Make clear button visible.
  view->SetInputTextForTesting(u"abc");
  EXPECT_TRUE(view->IsClearButtonVisibleForTesting());

  // SearchBarOnFocusLost should never be called when tabbing between controls.
  EXPECT_CALL(delegate(), SearchBarOnFocusLost).Times(0);

  // Press TAB: focus moves from textfield to clear button.
  generator().PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(widget().GetFocusManager()->GetFocusedView()->GetClassName(),
            "SearchBarClearButton");

  // Press TAB again: focus moves back to textfield.
  generator().PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(widget().GetFocusManager()->GetFocusedView()->GetClassName(),
            "SearchBarTextfield");

  Mock::VerifyAndClearExpectations(&delegate());
}

// Tests that pressing Shift+TAB cycles focus between the textfield and clear
// button when the clear button is visible.
TEST_F(PopupSearchBarViewTest, ShiftTabKeyCyclesToClearButtonWhenVisible) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();
  view->Focus();

  view->SetInputTextForTesting(u"abc");
  EXPECT_TRUE(view->IsClearButtonVisibleForTesting());

  EXPECT_CALL(delegate(), SearchBarOnFocusLost).Times(0);

  // Press Shift+TAB: focus moves from textfield to clear button.
  generator().PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(widget().GetFocusManager()->GetFocusedView()->GetClassName(),
            "SearchBarClearButton");

  // Press Shift+TAB again: focus moves back to textfield.
  generator().PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(widget().GetFocusManager()->GetFocusedView()->GetClassName(),
            "SearchBarTextfield");

  Mock::VerifyAndClearExpectations(&delegate());
}

// Tests that pressing TAB keeps focus on the textfield when the clear button is
// hidden.
TEST_F(PopupSearchBarViewTest, TabKeyStaysOnTextfieldWhenClearButtonHidden) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();
  view->Focus();

  EXPECT_FALSE(view->IsClearButtonVisibleForTesting());
  EXPECT_CALL(delegate(), SearchBarOnFocusLost).Times(0);

  // Press TAB: focus stays on textfield because clear button is hidden.
  generator().PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(widget().GetFocusManager()->GetFocusedView()->GetClassName(),
            "SearchBarTextfield");

  Mock::VerifyAndClearExpectations(&delegate());
}

// Tests that delegate handling of the TAB key takes priority over search bar
// focus cycling.
TEST_F(PopupSearchBarViewTest, TabKeyHandledByDelegateFirst) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*initial_value=*/u"", delegate()));
  widget().Show();
  view->Focus();

  view->SetInputTextForTesting(u"abc");

  // Delegate handles TAB key.
  EXPECT_CALL(delegate(), SearchBarHandleKeyPressed)
      .WillOnce([](const ui::KeyEvent& event) {
        return event.key_code() == ui::VKEY_TAB;
      });

  generator().PressAndReleaseKey(ui::VKEY_TAB);

  // Focus remains on Textfield because delegate handled the key press.
  EXPECT_EQ(widget().GetFocusManager()->GetFocusedView()->GetClassName(),
            "SearchBarTextfield");

  Mock::VerifyAndClearExpectations(&delegate());
}

}  // namespace
}  // namespace autofill
