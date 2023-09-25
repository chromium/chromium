// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kBrowserContentAllowedMinimumWidth =
    BrowserViewLayout::kMainBrowserContentsMinimumWidth;
}  // namespace

class ToolbarControllerInteractiveTest : public InteractiveBrowserTest {
 public:
  ToolbarControllerInteractiveTest() {
    scoped_feature_list_.InitWithFeatures({features::kResponsiveToolbar}, {});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());
    toolbar_controller_ = const_cast<ToolbarController*>(
        browser_view_->toolbar()->toolbar_controller());
    toolbar_container_view_ = const_cast<views::View*>(
        toolbar_controller_->toolbar_container_view_.get());
    overflow_button_ = toolbar_controller_->overflow_button_;
    dummy_button_size_ = overflow_button_->GetPreferredSize();
    element_ids_ = toolbar_controller_->element_ids_;
    element_flex_order_start_ = toolbar_controller_->element_flex_order_start_;
    MaybeAddDummyButtonsToToolbarView();
    overflow_threshold_width_ = GetOverflowThresholdWidth();
  }

  void TearDownOnMainThread() override {
    toolbar_container_view_ = nullptr;
    overflow_button_ = nullptr;
    toolbar_controller_ = nullptr;
    browser_view_ = nullptr;
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  // Returns the minimum width the toolbar view can be without any elements
  // dropped out.
  int GetOverflowThresholdWidth() {
    int diff_sum = 0;
    for (auto* element : toolbar_container_view_->children()) {
      diff_sum += element->GetPreferredSize().width() -
                  element->GetMinimumSize().width();

      // TODO(crbug.com/1479588): Ignore containers till issue addressed.
      // This case only applies to containers. Now that containers are ignored
      // their main items (i.e. extensions button, side panel button) width is
      // excluded from the calculation too. So is the margin.
      if (element->GetMinimumSize().width() == 0 &&
          element->GetPreferredSize().width() > 0) {
        diff_sum += GetLayoutConstant(TOOLBAR_ICON_DEFAULT_MARGIN);
      }
    }
    return toolbar_container_view_->GetPreferredSize().width() - diff_sum;
  }

  // Because actual_browser_minimum_width == Max(toolbar_width,
  // kBrowserContentAllowedMinimumWidth) So if `overflow_threshold_width_` <
  // kBrowserContentAllowedMinimumWidth, then actual_browser_minimum_width ==
  // kBrowserContentAllowedMinimumWidth In this case we will never see any
  // overflow so stuff toolbar with some fixed dummy buttons till it's
  // guaranteed we can observe overflow with browser resized to its minimum
  // width.
  void MaybeAddDummyButtonsToToolbarView() {
    while (GetOverflowThresholdWidth() <= kBrowserContentAllowedMinimumWidth) {
      auto button = std::make_unique<ToolbarButton>();
      button->SetPreferredSize(dummy_button_size_);
      button->SetMinSize(dummy_button_size_);
      button->SetAccessibleName(u"dummybutton");
      button->SetVisible(true);
      toolbar_container_view_->AddChildView(std::move(button));
    }
  }

  // This checks menu model, not the actual menu that pops up.
  // TODO(pengchaocai): Explore a way to check the actual menu appearing.
  auto CheckMenuMatchesOverflowedElements() {
    return Steps(Check(base::BindLambdaForTesting([this]() {
      const ui::SimpleMenuModel* menu = GetOverflowMenu();
      std::vector<const views::View*> overflowed_elements =
          GetOverflowedElements();
      EXPECT_NE(menu, nullptr);
      EXPECT_GT(menu->GetItemCount(), size_t(0));
      EXPECT_EQ(menu->GetItemCount(), overflowed_elements.size());
      for (size_t i = 0; i < menu->GetItemCount(); ++i) {
        if (menu->GetLabelAt(i).compare(
                overflowed_elements[i]->GetAccessibleName()) != 0) {
          return false;
        }
      }
      return true;
    })));
  }

  void SetBrowserWidth(int width) {
    browser_view_->SetSize({width, browser_view_->size().height()});
  }

  const views::View* FindToolbarElementWithId(ui::ElementIdentifier id) const {
    return toolbar_controller_->FindToolbarElementWithId(id);
  }

  const views::View* overflow_button() const { return overflow_button_; }
  int element_flex_order_start() const { return element_flex_order_start_; }
  const std::vector<ui::ElementIdentifier>& element_ids() const {
    return element_ids_;
  }
  int overflow_threshold_width() const { return overflow_threshold_width_; }
  std::vector<const views::View*> GetOverflowedElements() {
    return toolbar_controller_->GetOverflowedElements();
  }
  const ui::SimpleMenuModel* GetOverflowMenu() {
    return static_cast<OverflowButton*>(overflow_button_)
        ->menu_model_for_testing();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<BrowserView> browser_view_;
  raw_ptr<ToolbarController> toolbar_controller_;
  raw_ptr<views::View> toolbar_container_view_;
  raw_ptr<views::View> overflow_button_;
  std::vector<ui::ElementIdentifier> element_ids_;
  int element_flex_order_start_;
  gfx::Size dummy_button_size_;

  // The minimum width the toolbar view can be without any elements dropped out.
  int overflow_threshold_width_;
};

IN_PROC_BROWSER_TEST_F(ToolbarControllerInteractiveTest, FlexOrderCorrect) {
  int order_start = element_flex_order_start();
  for (ui::ElementIdentifier id : element_ids()) {
    const views::View* toolbar_element = FindToolbarElementWithId(id);
    if (toolbar_element) {
      EXPECT_EQ(order_start++,
                toolbar_element->GetProperty(views::kFlexBehaviorKey)->order());
    }
  }
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerInteractiveTest,
                       StartBrowserWithThresholdWidth) {
  // Start browser with threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit wider. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser back to threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit narrower. Should see overflow.
  SetBrowserWidth(overflow_threshold_width() - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Resize browser back to threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerInteractiveTest,
                       StartBrowserWithWidthSmallerThanThreshold) {
  // Start browser with a smaller width than threshold. Should see overflow.
  SetBrowserWidth(overflow_threshold_width() - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Resize browser wider to threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit narrower. Should see overflow.
  SetBrowserWidth(overflow_threshold_width() - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Keep resizing browser narrower. Should see overflow.
  SetBrowserWidth(overflow_threshold_width() - 2);
  EXPECT_TRUE(overflow_button()->GetVisible());

  // Resize browser a bit wider. Should still see overflow.
  SetBrowserWidth(overflow_threshold_width() - 1);
  EXPECT_TRUE(overflow_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerInteractiveTest,
                       StartBrowserWithWidthLargerThanThreshold) {
  // Start browser with a larger width than threshold. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser wider. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 2);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit narrower. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser back to threshold width. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width());
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Resize browser a bit wider. Should not see overflow.
  SetBrowserWidth(overflow_threshold_width() + 1);
  EXPECT_FALSE(overflow_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ToolbarControllerInteractiveTest,
                       MenuMatchesOverflowedElements) {
  RunTestSequence(Do(base::BindLambdaForTesting([this]() {
                    SetBrowserWidth(overflow_threshold_width() - 1);
                  })),
                  WaitForShow(kToolbarOverflowButtonElementId),
                  PressButton(kToolbarOverflowButtonElementId),
                  WaitForActivate(kToolbarOverflowButtonElementId),
                  CheckMenuMatchesOverflowedElements());
}
