// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
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
              (const std::u16string& text),
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
  PopupSearchBarView* view = widget().SetContentsView(
      std::make_unique<PopupSearchBarView>(u"placeholder", delegate()));
  widget().Show();
  view->Focus();

  views::View* focused_field = widget().GetFocusManager()->GetFocusedView();
  ASSERT_NE(focused_field, nullptr);
  EXPECT_EQ(focused_field->GetClassMetaData()->type_name(),
            std::string("Textfield"));
}

TEST_F(PopupSearchBarViewTest, OnFocusLostCalled) {
  PopupSearchBarView* view = widget().SetContentsView(
      std::make_unique<PopupSearchBarView>(u"placeholder", delegate()));
  widget().Show();
  view->Focus();
  ASSERT_NE(widget().GetFocusManager()->GetFocusedView(), nullptr);

  EXPECT_CALL(delegate(), SearchBarOnFocusLost);
  widget().GetFocusManager()->SetFocusedView(nullptr);
}

TEST_F(PopupSearchBarViewTest, OnInputChangedIsCalledAfterDelay) {
  auto view = std::make_unique<PopupSearchBarView>(u"placeholder", delegate());

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

TEST_F(PopupSearchBarViewTest, OnInputChangedCallbackIsThrottled) {
  auto view = std::make_unique<PopupSearchBarView>(u"placeholder", delegate());

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
  PopupSearchBarView* view = widget().SetContentsView(
      std::make_unique<PopupSearchBarView>(u"placeholder", delegate()));
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
  PopupSearchBarView* view = widget().SetContentsView(
      std::make_unique<PopupSearchBarView>(u"placeholder", delegate()));
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

  generator().MoveMouseTo(view->GetClearButtonScreenCenterPointForTesting());
  generator().ClickLeftButton();
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay);
}

}  // namespace
}  // namespace autofill
