// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"
#include "chrome/browser/ui/performance_controls/tab_discard_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/performance_manager/public/features.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/button_test_api.h"

class DiscardMockNavigationHandle : public content::MockNavigationHandle {
 public:
  void SetWasDiscarded(bool was_discarded) { was_discarded_ = was_discarded; }
  bool ExistingDocumentWasDiscarded() const override { return was_discarded_; }

 private:
  bool was_discarded_ = false;
};

class HighEfficiencyChipViewTest : public TestWithBrowserView {
 public:
 protected:
  HighEfficiencyChipViewTest() = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        performance_manager::features::kHighEfficiencyModeAvailable);

    TestWithBrowserView::SetUp();

    AddTab(browser(), GURL("http://foo"));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    TabDiscardTabHelper::CreateForWebContents(contents);
  }

  void TearDown() override { TestWithBrowserView::TearDown(); }

  void SetTabDiscardState(bool is_discarded) {
    TabDiscardTabHelper* tab_helper = TabDiscardTabHelper::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(0));
    std::unique_ptr<DiscardMockNavigationHandle> navigation_handle =
        std::make_unique<DiscardMockNavigationHandle>();
    navigation_handle.get()->SetWasDiscarded(is_discarded);
    tab_helper->DidStartNavigation(navigation_handle.get());

    browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->UpdateAll();
  }

  PageActionIconView* GetPageActionIconView() {
    return browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kHighEfficiency);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple local_state_;
};

// When the previous page has a tab discard state of true, when the icon is
// updated it should be visible.
TEST_F(HighEfficiencyChipViewTest, ShouldShowForDiscardedPage) {
  SetTabDiscardState(true);

  PageActionIconView* view = GetPageActionIconView();

  EXPECT_TRUE(view->GetVisible());
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(HighEfficiencyChipViewTest, ShouldNotShowForRegularPage) {
  SetTabDiscardState(false);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_FALSE(view->GetVisible());
}

// When the page action chip is clicked, the dialog should open.
TEST_F(HighEfficiencyChipViewTest, ShouldOpenDialogOnClick) {
  SetTabDiscardState(true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_EQ(view->GetBubble(), nullptr);

  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(view);
  test_api.NotifyClick(e);

  EXPECT_NE(view->GetBubble(), nullptr);
}

// A link should be rendered within the dialog.
TEST_F(HighEfficiencyChipViewTest, ShouldRenderLinkInDialog) {
  SetTabDiscardState(true);

  PageActionIconView* view = GetPageActionIconView();

  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(view);
  test_api.NotifyClick(e);

  views::View* extra_view = view->GetBubble()->GetExtraView();
  ASSERT_EQ(std::string(extra_view->GetClassName()), "Link");
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(HighEfficiencyChipViewTest, ShouldHideLabelAfterThreeTimes) {
  // Open the tab 3 times with the label being visible.
  for (int i = 0; i < 3; i++) {
    SetTabDiscardState(true);
    EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());
    SetTabDiscardState(false);
  }

  // On the 4th time, the label should be hidden.
  SetTabDiscardState(true);
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());
}
