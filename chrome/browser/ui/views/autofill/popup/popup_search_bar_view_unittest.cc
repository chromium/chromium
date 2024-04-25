// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace autofill {

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
          u"placeholder", /*on_focus_lost_callback=*/base::DoNothing()));
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
          u"placeholder", mock_on_focus_lost_callback.Get()));
  widget().Show();
  view->Focus();
  ASSERT_NE(widget().GetFocusManager()->GetFocusedView(), nullptr);

  EXPECT_CALL(mock_on_focus_lost_callback, Run);
  widget().GetFocusManager()->SetFocusedView(nullptr);
}

}  // namespace autofill
