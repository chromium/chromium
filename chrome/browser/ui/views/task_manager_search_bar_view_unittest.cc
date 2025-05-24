// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/task_manager_search_bar_view.h"

#include <memory>
#include <string_view>

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

namespace task_manager {

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::NiceMock;

class MockDelegate : public TaskManagerSearchBarView::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;
  MOCK_METHOD(void,
              SearchBarOnInputChanged,
              (std::u16string_view text),
              (override));
};
class TaskManagerSearchBarViewTest : public ChromeViewsTestBase {
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

TEST_F(TaskManagerSearchBarViewTest, SetsFocusOnTextfield) {
  auto* view =
      widget().SetContentsView(std::make_unique<TaskManagerSearchBarView>(
          u"placeholder", gfx::Insets::TLBR(0, 0, 0, 0), delegate()));
  widget().Show();
  view->Focus();

  views::View* focused_field = widget().GetFocusManager()->GetFocusedView();
  ASSERT_NE(focused_field, nullptr);
  EXPECT_EQ(focused_field->GetClassName(), "Textfield");
}

TEST_F(TaskManagerSearchBarViewTest, KeyPressedFromTextfield) {
  auto* view =
      widget().SetContentsView(std::make_unique<TaskManagerSearchBarView>(
          u"placeholder", gfx::Insets::TLBR(0, 0, 0, 0), delegate()));
  EXPECT_FALSE(view->GetClearButtonVisibleStatusForTesting());
  widget().Show();
  view->Focus();

  generator().PressAndReleaseKey(ui::VKEY_A);
  generator().PressAndReleaseKey(ui::VKEY_B);
  generator().PressAndReleaseKey(ui::VKEY_C);
  EXPECT_TRUE(view->GetClearButtonVisibleStatusForTesting());
}

TEST_F(TaskManagerSearchBarViewTest, OnInputChangedIsCalledAfterDelay) {
  auto view = std::make_unique<TaskManagerSearchBarView>(
      u"placeholder", gfx::Insets::TLBR(0, 0, 0, 0), delegate());

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(delegate(), SearchBarOnInputChanged(Eq(u"input text")));
  }

  view->SetInputTextForTesting(u"input text");
  task_environment()->FastForwardBy(
      TaskManagerSearchBarView::kInputChangeCallbackDelay / 2);
  check.Call();
  task_environment()->FastForwardBy(
      TaskManagerSearchBarView::kInputChangeCallbackDelay / 2);
}

TEST_F(TaskManagerSearchBarViewTest, OnInputChangedCallbackIsThrottled) {
  auto view = std::make_unique<TaskManagerSearchBarView>(
      u"placeholder", gfx::Insets::TLBR(0, 0, 0, 0), delegate());

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(delegate(), SearchBarOnInputChanged(Eq(u"input text 2")));
  }

  view->SetInputTextForTesting(u"input text");
  task_environment()->FastForwardBy(
      TaskManagerSearchBarView::kInputChangeCallbackDelay / 2);
  check.Call();
  view->SetInputTextForTesting(u"input text 2");
  task_environment()->FastForwardBy(
      TaskManagerSearchBarView::kInputChangeCallbackDelay);
}

TEST_F(TaskManagerSearchBarViewTest, ClearButton) {
  auto* view =
      widget().SetContentsView(std::make_unique<TaskManagerSearchBarView>(
          u"placeholder", gfx::Insets::TLBR(0, 0, 0, 0), delegate()));
  widget().Show();
  view->Focus();

  view->SetInputTextForTesting(u"abc");

  generator().MoveMouseTo(view->GetClearButtonScreenCenterPointForTesting());
  generator().ClickLeftButton();
  EXPECT_FALSE(view->GetClearButtonVisibleStatusForTesting());
}

}  // namespace task_manager
