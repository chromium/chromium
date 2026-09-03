// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"

#include "base/byte_size.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/hover_card_anchor_target.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_test_util.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_thumbnail_observer.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"

// These are regression tests for possible crashes.

class TabHoverCardControllerTest : public InProcessBrowserTest {
 public:
  TabHoverCardControllerTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    controller_ = test::TabHoverCardTestUtil::GetHoverCardController(browser());
    g_browser_process->local_state()->SetBoolean(prefs::kHoverCardImagesEnabled,
                                                 true);
  }
  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    controller_ = nullptr;
  }

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  HoverCardAnchorTarget* GetHoverCardAnchorTargetAt(int index) {
    bool is_vertical = GetBrowserView()->ShouldDrawVerticalTabStrip();
    views::View* tab_view =
        GetBrowserView()->tab_strip_view()->GetTabAnchorView(
            browser()->GetTabStripModel()->GetTabAtIndex(index)->GetHandle());

    if (is_vertical ||
        base::FeatureList::IsEnabled(tabs::kTabStripUnification)) {
      return AsViewClass<TabView>(tab_view);
    } else {
      return AsViewClass<Tab>(tab_view);
    }
  }

  TabHoverCardController* controller() { return controller_.get(); }

 private:
  raw_ptr<TabHoverCardController> controller_;
  base::test::ScopedFeatureList feature_list_{features::kTabHoverCardImages};
};

IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest,
                       SetPreviewWithNoHoverCardDoesntCrash) {
  // If the safeguard is not in place, this could crash in either metrics
  // collection *or* in trying to set the actual thumbnail image on the card.
  controller()->OnPreviewImageAvailable(controller()->thumbnail_observer_.get(),
                                        gfx::ImageSkia());
}

IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest, ShowPreviewsForTab) {
  chrome::AddTabAt(browser(), GURL("http://foo1.com"), 0, false);
  chrome::AddTabAt(browser(), GURL("http://foo2.com"), 1, false);
  browser()->GetTabStripModel()->ActivateTabAt(0);

  HoverCardAnchorTarget* target_tab = GetHoverCardAnchorTargetAt(1);
  controller()->target_tab_ = target_tab;

  controller()->CreateHoverCard(target_tab);
  EXPECT_TRUE(controller()->ArePreviewsEnabled());
}

IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest, DisablePreviewsForTab) {
  g_browser_process->local_state()->SetBoolean(prefs::kHoverCardImagesEnabled,
                                               false);

  chrome::AddTabAt(browser(), GURL("http://foo1.com"), 0, false);
  chrome::AddTabAt(browser(), GURL("http://foo2.com"), 1, false);
  browser()->GetTabStripModel()->ActivateTabAt(0);

  HoverCardAnchorTarget* target_tab = GetHoverCardAnchorTargetAt(1);
  controller()->target_tab_ = target_tab;

  controller()->CreateHoverCard(target_tab);
  EXPECT_FALSE(controller()->ArePreviewsEnabled());
}

IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest,
                       HidePreviewsForDiscardedTab) {
  chrome::AddTabAt(browser(), GURL("http://foo1.com"), 0, false);
  chrome::AddTabAt(browser(), GURL("http://foo2.com"), 1, false);
  browser()->GetTabStripModel()->ActivateTabAt(0);

  HoverCardAnchorTarget* target_tab = GetHoverCardAnchorTargetAt(1);
  controller()->target_tab_ = target_tab;

  controller()->CreateHoverCard(target_tab);
  controller()->UpdateCardContent(target_tab);

  EXPECT_EQ(controller()->thumbnail_observer_.get()->current_image(), nullptr);
  EXPECT_EQ(controller()->thumbnail_wait_state_,
            TabHoverCardController::kNotWaiting);
}

IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest, DisableMemoryUsageForTab) {
  g_browser_process->local_state()->SetBoolean(
      prefs::kHoverCardMemoryUsageEnabled, false);

  chrome::AddTabAt(browser(), GURL("http://foo1.com"), 0, false);
  chrome::AddTabAt(browser(), GURL("http://foo2.com"), 1, false);
  browser()->GetTabStripModel()->ActivateTabAt(0);

  HoverCardAnchorTarget* target_tab = GetHoverCardAnchorTargetAt(1);
  controller()->target_tab_ = target_tab;

  controller()->CreateHoverCard(target_tab);
  EXPECT_FALSE(controller()->hover_card_memory_usage_enabled_);
}

class TestThumbnailImageDelegate : public ThumbnailImage::Delegate {
 public:
  TestThumbnailImageDelegate() = default;
  ~TestThumbnailImageDelegate() override = default;

  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {
    is_being_observed_ = is_being_observed;
  }

  bool is_being_observed() const { return is_being_observed_; }

 private:
  bool is_being_observed_ = false;
};

IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest,
                       ShowPreviewsForDiscardedTabWithThumbnail) {
  chrome::AddTabAt(browser(), GURL("http://foo1.com"), 0, false);
  chrome::AddTabAt(browser(), GURL("http://foo2.com"), 1, false);
  browser()->GetTabStripModel()->ActivateTabAt(0);

  HoverCardAnchorTarget* target_tab = GetHoverCardAnchorTargetAt(1);
  controller()->target_tab_ = target_tab;

  TestThumbnailImageDelegate delegate;
  auto image = base::MakeRefCounted<ThumbnailImage>(&delegate);
  controller()->CreateHoverCard(target_tab);
  controller()->thumbnail_observer_.get()->Observe(image);

  EXPECT_NE(controller()->thumbnail_observer_.get()->current_image(), nullptr);
  EXPECT_EQ(controller()->thumbnail_wait_state_,
            TabHoverCardController::kNotWaiting);
}

IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest, ShowPreviewsForCrashedTab) {
  chrome::AddTabAt(browser(), GURL("http://foo1.com"), 0, false);
  chrome::AddTabAt(browser(), GURL("http://foo2.com"), 1, false);
  content::WaitForLoadStop(browser()->GetTabStripModel()->GetWebContentsAt(1));
  browser()->GetTabStripModel()->ActivateTabAt(0);

  HoverCardAnchorTarget* const target_tab = GetHoverCardAnchorTargetAt(1);
  content::CrashTab(browser()->GetTabStripModel()->GetWebContentsAt(1));

  controller()->CreateHoverCard(target_tab);
  controller()->UpdateCardContent(target_tab);

  // When crashed, we should not observe any thumbnail, even if one exists.
  EXPECT_EQ(controller()->thumbnail_observer_.get()->current_image(), nullptr);
  // And we should not be waiting for one.
  EXPECT_EQ(controller()->thumbnail_wait_state_,
            TabHoverCardController::kNotWaiting);
}

IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest, HoverCardLabel_DomainIsUrl) {
  test::TabHoverCardTestUtil hover_card_test_util;

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://example.com"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  HoverCardAnchorTarget* const target_tab = GetHoverCardAnchorTargetAt(1);
  controller()->UpdateHoverCard(target_tab,
                                TabSlotController::HoverCardUpdateType::kHover);

  TabHoverCardBubbleView* hover_card = controller()->hover_card_for_testing();
  ASSERT_TRUE(hover_card);

  FadeLabelView* domain_view = hover_card->GetDomainViewForTesting();
  FadeLabel* primary_label = domain_view->GetPrimaryViewForTesting();
  EXPECT_EQ(gfx::DirectionalityMode::DIRECTIONALITY_AS_URL,
            primary_label->GetDirectionalityMode());
}

class TabHoverCardPreviewsEnabledPrefTest : public TabHoverCardControllerTest {
 public:
  TabHoverCardPreviewsEnabledPrefTest() = default;

 private:
  base::test::ScopedFeatureList feature_list_ = []() {
    base::test::ScopedFeatureList list;
    list.InitAndDisableFeature(features::kTabHoverCardImages);
    return list;
  }();
};

IN_PROC_BROWSER_TEST_F(TabHoverCardPreviewsEnabledPrefTest, DefaultState) {
  EXPECT_FALSE(TabHoverCardController::AreHoverCardImagesEnabled());
}
