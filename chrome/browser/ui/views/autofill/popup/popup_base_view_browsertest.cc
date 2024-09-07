// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

using testing::Return;
using testing::ReturnRef;

class MockAutofillPopupViewDelegate : public AutofillPopupViewDelegate {
 public:
  MockAutofillPopupViewDelegate() = default;
  ~MockAutofillPopupViewDelegate() override = default;

  MOCK_METHOD(void, Hide, (SuggestionHidingReason), (override));
  MOCK_METHOD(void, ViewDestroyed, (), (override));

  MOCK_METHOD(gfx::NativeView, container_view, (), (const override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (const override));
  MOCK_METHOD(const gfx::RectF&, element_bounds, (), (const override));
  MOCK_METHOD(PopupAnchorType, anchor_type, (), (const override));
  MOCK_METHOD(base::i18n::TextDirection,
              GetElementTextDirection,
              (),
              (const override));

  base::WeakPtr<AutofillPopupViewDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAutofillPopupViewDelegate> weak_ptr_factory_{this};
};

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class PopupBaseViewBrowsertest : public InProcessBrowserTest {
 public:
  PopupBaseViewBrowsertest() {
    feature_list_.InitAndDisableFeature(features::kAutofillMoreProminentPopup);
  }

  PopupBaseViewBrowsertest(const PopupBaseViewBrowsertest&) = delete;
  PopupBaseViewBrowsertest& operator=(const PopupBaseViewBrowsertest&) = delete;

  ~PopupBaseViewBrowsertest() override = default;

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    gfx::NativeView native_view = web_contents->GetNativeView();
    EXPECT_CALL(mock_delegate_, container_view())
        .WillRepeatedly(Return(native_view));
    EXPECT_CALL(mock_delegate_, GetWebContents())
        .WillRepeatedly(Return(web_contents));
    EXPECT_CALL(mock_delegate_, ViewDestroyed());

    view_ = new PopupBaseView(mock_delegate_.GetWeakPtr(),
                              views::Widget::GetWidgetForNativeWindow(
                                  browser()->window()->GetNativeWindow()));
  }

  void TearDownOnMainThread() override { view_ = nullptr; }

  void ShowView() { view_->DoShow(); }

 protected:
  testing::NiceMock<MockAutofillPopupViewDelegate> mock_delegate_;
  raw_ptr<PopupBaseView> view_ = nullptr;

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PopupBaseViewBrowsertest, CorrectBoundsTest) {
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

IN_PROC_BROWSER_TEST_F(PopupBaseViewBrowsertest, AccessibleProperties) {
  gfx::Rect web_bounds = mock_delegate_.GetWebContents()->GetViewBounds();
  gfx::RectF bounds(web_bounds.x() + 100, web_bounds.y() + 150, 10, 10);
  EXPECT_CALL(mock_delegate_, element_bounds())
      .WillRepeatedly(ReturnRef(bounds));
  ShowView();
  ui::AXNodeData data;

  view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kPane, data.role);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

struct ProminentPopupTestParams {
  bool is_feature_enabled;
  int expected_left_offset;
};

class PopupBaseViewProminentStyleFeatureTest
    : public PopupBaseViewBrowsertest,
      public testing::WithParamInterface<ProminentPopupTestParams> {
 public:
  PopupBaseViewProminentStyleFeatureTest() {
    feature_list_.InitWithFeatureState(features::kAutofillMoreProminentPopup,
                                       GetParam().is_feature_enabled);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PopupBaseViewProminentStyleFeatureTest, LeftMaxOffset) {
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
    PopupBaseViewProminentStyleFeatureTest,
    testing::Values(ProminentPopupTestParams{.is_feature_enabled = false,
                                             .expected_left_offset = 95},
                    ProminentPopupTestParams{.is_feature_enabled = true,
                                             .expected_left_offset = 55}));

}  // namespace autofill
