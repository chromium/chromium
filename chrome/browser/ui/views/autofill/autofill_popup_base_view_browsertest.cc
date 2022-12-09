// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"
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

  base::WeakPtr<AutofillPopupViewDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAutofillPopupViewDelegate> weak_ptr_factory_{this};
};

}  // namespace

class AutofillPopupBaseViewTest : public InProcessBrowserTest {
 public:
  AutofillPopupBaseViewTest() = default;

  AutofillPopupBaseViewTest(const AutofillPopupBaseViewTest&) = delete;
  AutofillPopupBaseViewTest& operator=(const AutofillPopupBaseViewTest&) =
      delete;

  ~AutofillPopupBaseViewTest() override = default;

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    gfx::NativeView native_view = web_contents->GetNativeView();
    EXPECT_CALL(mock_delegate_, container_view())
        .WillRepeatedly(Return(native_view));
    EXPECT_CALL(mock_delegate_, GetWebContents())
        .WillRepeatedly(Return(web_contents));
    EXPECT_CALL(mock_delegate_, ViewDestroyed());

    view_ =
        new AutofillPopupBaseView(mock_delegate_.GetWeakPtr(),
                                  views::Widget::GetWidgetForNativeWindow(
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
  raw_ptr<AutofillPopupBaseView, DanglingUntriaged> view_;
};

IN_PROC_BROWSER_TEST_F(AutofillPopupBaseViewTest, CorrectBoundsTest) {
  gfx::Rect web_bounds = mock_delegate_.GetWebContents()->GetViewBounds();
  gfx::RectF bounds(web_bounds.x() + 100, web_bounds.y() + 150, 10, 10);
  EXPECT_CALL(mock_delegate_, element_bounds())
      .WillRepeatedly(ReturnRef(bounds));

  ShowView();

  gfx::Point display_point = static_cast<views::View*>(view_)
                                 ->GetWidget()
                                 ->GetClientAreaBoundsInScreen()
                                 .origin();
  // The expected origin is shifted to accommodate the border of the bubble, the
  // arrow, padding and the alignment to the center.
  gfx::Point expected_point = gfx::ToRoundedPoint(bounds.bottom_left());
  expected_point.Offset(6, -13);
  EXPECT_EQ(expected_point, display_point);
}

struct ProminentPopupTestParams {
  bool is_feature_enabled;
  int expected_left_offset;
};

class AutofillPopupBaseViewProminentStyleFeatureTest
    : public AutofillPopupBaseViewTest,
      public testing::WithParamInterface<ProminentPopupTestParams> {
 public:
  AutofillPopupBaseViewProminentStyleFeatureTest() {
    feature_list_.InitWithFeatureState(features::kAutofillMoreProminentPopup,
                                       GetParam().is_feature_enabled);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(AutofillPopupBaseViewProminentStyleFeatureTest,
                       LeftMaxOffset) {
  gfx::Rect web_bounds = mock_delegate_.GetWebContents()->GetViewBounds();
  gfx::RectF bounds(web_bounds.x() + 100, web_bounds.y() + 150, 1000, 20);
  EXPECT_CALL(mock_delegate_, element_bounds())
      .WillRepeatedly(ReturnRef(bounds));

  ShowView();

  gfx::Point display_point = static_cast<views::View*>(view_)
                                 ->GetWidget()
                                 ->GetClientAreaBoundsInScreen()
                                 .origin();

  // Shows the popup on a long (1000px) element and returns the offset
  // of the poopup's top left point to the bottom left point of the target:
  //     │      element     │
  //     └──────────────────┘
  //      |- offset -|┌──^───────────────┐
  //                  │       popup      │
  gfx::Vector2d offset =
      display_point - gfx::ToRoundedPoint(bounds.bottom_left());

  EXPECT_EQ(offset.x(), GetParam().expected_left_offset);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillPopupBaseViewProminentStyleFeatureTest,
    testing::Values(ProminentPopupTestParams{.is_feature_enabled = false,
                                             .expected_left_offset = 95},
                    ProminentPopupTestParams{.is_feature_enabled = true,
                                             .expected_left_offset = 55}));

}  // namespace autofill
