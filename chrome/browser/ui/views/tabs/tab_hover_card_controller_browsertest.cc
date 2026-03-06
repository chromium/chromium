// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"

#include "base/byte_size.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/hover_card_anchor_target.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_thumbnail_observer.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

// These are regression tests for possible crashes.

class TabHoverCardControllerTest : public InProcessBrowserTest {
 public:
  TabHoverCardControllerTest() {
    feature_list_.InitAndEnableFeature(features::kTabHoverCardImages);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    controller_ = GetBrowserView()
                      ->horizontal_tab_strip_for_testing()
                      ->hover_card_controller_for_testing();
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
        GetBrowserView()->tab_strip_view()->GetTabAnchorViewAt(index);

    if (is_vertical) {
      return AsViewClass<VerticalTabView>(tab_view);
    } else {
      return AsViewClass<Tab>(tab_view);
    }
  }

  TabHoverCardController* controller() { return controller_.get(); }

 private:
  raw_ptr<TabHoverCardController> controller_;
  base::test::ScopedFeatureList feature_list_;
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
  browser()->tab_strip_model()->ActivateTabAt(0);

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
  browser()->tab_strip_model()->ActivateTabAt(0);

  HoverCardAnchorTarget* target_tab = GetHoverCardAnchorTargetAt(1);
  controller()->target_tab_ = target_tab;

  controller()->CreateHoverCard(target_tab);
  EXPECT_FALSE(controller()->ArePreviewsEnabled());
}

IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest,
                       HidePreviewsForDiscardedTab) {
  chrome::AddTabAt(browser(), GURL("http://foo1.com"), 0, false);
  chrome::AddTabAt(browser(), GURL("http://foo2.com"), 1, false);
  browser()->tab_strip_model()->ActivateTabAt(0);

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
  browser()->tab_strip_model()->ActivateTabAt(0);

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
  browser()->tab_strip_model()->ActivateTabAt(0);

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
// TODO(crbug.com/481392191): Crash the anchor tab directly as part of the test
// instead of manipulating the tabs::TabData.
IN_PROC_BROWSER_TEST_F(TabHoverCardControllerTest, ShowPreviewsForCrashedTab) {
  chrome::AddTabAt(browser(), GURL("http://foo1.com"), 0, false);
  chrome::AddTabAt(browser(), GURL("http://foo2.com"), 1, false);
  browser()->tab_strip_model()->ActivateTabAt(0);

  HoverCardAnchorTarget* const target_tab = GetHoverCardAnchorTargetAt(1);
  tabs::TabData data;
  data.is_crashed = true;
  TestThumbnailImageDelegate delegate;
  auto image = base::MakeRefCounted<ThumbnailImage>(&delegate);
  data.thumbnail = image;
  views::View* tab_view =
      GetBrowserView()->tab_strip_view()->GetTabAnchorViewAt(1);
  AsViewClass<Tab>(tab_view)->SetData(std::move(data));

  controller()->CreateHoverCard(target_tab);
  controller()->UpdateCardContent(target_tab);

  // When crashed, we should not observe any thumbnail, even if one exists.
  EXPECT_EQ(controller()->thumbnail_observer_.get()->current_image(), nullptr);
  // And we should not be waiting for one.
  EXPECT_EQ(controller()->thumbnail_wait_state_,
            TabHoverCardController::kNotWaiting);
}

class TabHoverCardPreviewsEnabledPrefTest : public TabHoverCardControllerTest {
 public:
  TabHoverCardPreviewsEnabledPrefTest() {
    feature_list_.InitAndDisableFeature(features::kTabHoverCardImages);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabHoverCardPreviewsEnabledPrefTest, DefaultState) {
  EXPECT_FALSE(TabHoverCardController::AreHoverCardImagesEnabled());
}
