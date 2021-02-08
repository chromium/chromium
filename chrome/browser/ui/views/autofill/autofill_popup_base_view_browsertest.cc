// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/border.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

using testing::Return;
using testing::ReturnRef;

class MockAutofillPopupViewDelegate : public AutofillPopupViewDelegate {
 public:
  MOCK_METHOD1(Hide, void(PopupHidingReason));
  MOCK_METHOD0(ViewDestroyed, void());
  MOCK_METHOD1(SetSelectionAtPoint, void(const gfx::Point&));
  MOCK_METHOD0(AcceptSelectedLine, bool());
  MOCK_METHOD0(SelectionCleared, void());
  MOCK_CONST_METHOD0(HasSelection, bool());

  // TODO(jdduke): Mock this method upon resolution of crbug.com/352463.
  MOCK_CONST_METHOD0(popup_bounds, gfx::Rect());
  MOCK_CONST_METHOD0(container_view, gfx::NativeView());
  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  MOCK_CONST_METHOD0(element_bounds, gfx::RectF&());
  MOCK_CONST_METHOD0(IsRTL, bool());
};

}  // namespace

class AutofillPopupBaseViewTest : public InProcessBrowserTest {
 public:
  AutofillPopupBaseViewTest() {}
  ~AutofillPopupBaseViewTest() override {}

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    gfx::NativeView native_view = web_contents->GetNativeView();
    EXPECT_CALL(mock_delegate_, container_view())
        .WillRepeatedly(Return(native_view));
    EXPECT_CALL(mock_delegate_, GetWebContents())
        .WillRepeatedly(Return(web_contents));
    EXPECT_CALL(mock_delegate_, ViewDestroyed());

    view_ = new AutofillPopupBaseView(
        &mock_delegate_, views::Widget::GetWidgetForNativeWindow(
                             browser()->window()->GetNativeWindow()));
  }

  void ShowView() { view_->DoShow(); }

  ui::GestureEvent CreateGestureEvent(ui::EventType type, gfx::Point point) {
    return ui::GestureEvent(point.x(), point.y(), 0, ui::EventTimeForNow(),
                            ui::GestureEventDetails(type));
  }

  void SimulateGesture(ui::GestureEvent* event) {
    view_->OnGestureEvent(event);
  }

 protected:
  testing::NiceMock<MockAutofillPopupViewDelegate> mock_delegate_;
  AutofillPopupBaseView* view_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupBaseViewTest);
};

// Regression test for crbug.com/391316
IN_PROC_BROWSER_TEST_F(AutofillPopupBaseViewTest, CorrectBoundsTest) {
  gfx::RectF bounds(100, 150, 5, 5);
  EXPECT_CALL(mock_delegate_, element_bounds())
      .WillRepeatedly(ReturnRef(bounds));

  ShowView();

  gfx::Point display_point = static_cast<views::View*>(view_)
                                 ->GetWidget()
                                 ->GetClientAreaBoundsInScreen()
                                 .origin();
  // The expected origin is shifted to accomodate the border of the bubble.
  gfx::Point expected_point = gfx::ToRoundedPoint(bounds.bottom_left());
  expected_point.Offset(0, AutofillPopupBaseView::kElementBorderPadding);
  gfx::Insets border = view_->GetWidget()->GetRootView()->border()->GetInsets();
  expected_point.Offset(-border.left(), -border.top());
  EXPECT_EQ(expected_point, display_point);
}

}  // namespace autofill
