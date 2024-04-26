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
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::MockFunction;
}  // namespace

class PopupSearchBarViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(PopupSearchBarViewTest, SetsFocusOnTextfield) {
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*on_input_changed_callback=*/base::DoNothing(),
          /*on_focus_lost_callback=*/base::DoNothing()));
  widget().Show();
  view->Focus();

  views::View* focused_field = widget().GetFocusManager()->GetFocusedView();
  ASSERT_NE(focused_field, nullptr);
  EXPECT_EQ(focused_field->GetClassMetaData()->type_name(),
            std::string("Textfield"));
}

TEST_F(PopupSearchBarViewTest, OnFocusLostCalled) {
  base::MockRepeatingClosure mock_on_focus_lost_callback;
  PopupSearchBarView* view =
      widget().SetContentsView(std::make_unique<PopupSearchBarView>(
          u"placeholder", /*on_input_changed_callback=*/base::DoNothing(),
          mock_on_focus_lost_callback.Get()));
  widget().Show();
  view->Focus();
  ASSERT_NE(widget().GetFocusManager()->GetFocusedView(), nullptr);

  EXPECT_CALL(mock_on_focus_lost_callback, Run);
  widget().GetFocusManager()->SetFocusedView(nullptr);
}

TEST_F(PopupSearchBarViewTest, OnInputChangedIsCalledAfterDelay) {
  base::MockRepeatingCallback<void(const std::u16string&)> input_callback;
  auto view = std::make_unique<PopupSearchBarView>(
      u"placeholder", input_callback.Get(),
      /*on_focus_lost_callback=*/base::DoNothing());

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(input_callback, Run(Eq(u"input text")));
  }

  view->SetInputTextForTesting(u"input text");
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay / 2);
  check.Call();
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay / 2);
}

TEST_F(PopupSearchBarViewTest, OnInputChangedCallbackIsThrottled) {
  base::MockRepeatingCallback<void(const std::u16string&)> input_callback;
  auto view = std::make_unique<PopupSearchBarView>(
      u"placeholder", input_callback.Get(),
      /*on_focus_lost_callback=*/base::DoNothing());

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(input_callback, Run(std::u16string(u"input text 2")));
  }

  view->SetInputTextForTesting(u"input text");
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay / 2);
  check.Call();
  view->SetInputTextForTesting(u"input text 2");
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay);
}

}  // namespace autofill
