// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"

// These are regression tests for possible crashes.

class TabHoverCardControllerTest : public TestWithBrowserView {
 public:
  TabHoverCardControllerTest() {
    feature_list_.InitAndEnableFeature(features::kTabHoverCardImages);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TabHoverCardControllerTest, ShowWrongTabDoesntCrash) {
  auto controller =
      std::make_unique<TabHoverCardController>(browser_view()->tabstrip());
  // Create some completely invalid pointer values (these should never be
  // dereferenced).
  Tab* const tab1 = reinterpret_cast<Tab*>(3);
  Tab* const tab2 = reinterpret_cast<Tab*>(7);
  controller->target_tab_ = tab1;
  // If the safeguard is not in place, this will crash because the target tab is
  // not a valid pointer.
  controller->ShowHoverCard(false, tab2);
}

TEST_F(TabHoverCardControllerTest, SetPreviewWithNoHoverCardDoesntCrash) {
  auto controller =
      std::make_unique<TabHoverCardController>(browser_view()->tabstrip());
  // If the safeguard is not in place, this could crash in either metrics
  // collection *or* in trying to set the actual thumbnail image on the card.
  controller->OnPreviewImageAvailable(controller->thumbnail_observer_.get(),
                                      gfx::ImageSkia());
}

TEST_F(TabHoverCardControllerTest, ShowPreviewsForTab) {
  g_browser_process->local_state()->SetBoolean(prefs::kHoverCardImagesEnabled,
                                               true);

  AddTab(browser_view()->browser(), GURL("http://foo1.com"));
  AddTab(browser_view()->browser(), GURL("http://foo2.com"));
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);

  auto controller =
      std::make_unique<TabHoverCardController>(browser_view()->tabstrip());

  Tab* const target_tab = browser_view()->tabstrip()->tab_at(1);
  controller->target_tab_ = target_tab;

  controller->CreateHoverCard(target_tab);
  EXPECT_TRUE(controller->ArePreviewsEnabled());
}

TEST_F(TabHoverCardControllerTest, DisablePreviewsForTab) {
  g_browser_process->local_state()->SetBoolean(prefs::kHoverCardImagesEnabled,
                                               false);

  AddTab(browser_view()->browser(), GURL("http://foo1.com"));
  AddTab(browser_view()->browser(), GURL("http://foo2.com"));
  browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);

  auto controller =
      std::make_unique<TabHoverCardController>(browser_view()->tabstrip());

  Tab* const target_tab = browser_view()->tabstrip()->tab_at(1);
  controller->target_tab_ = target_tab;

  controller->CreateHoverCard(target_tab);
  EXPECT_FALSE(controller->ArePreviewsEnabled());
}

class TabHoverCardPreviewsEnabledPrefTest : public TestWithBrowserView {
 public:
  TabHoverCardPreviewsEnabledPrefTest() {
    feature_list_.InitAndDisableFeature(features::kTabHoverCardImages);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TabHoverCardPreviewsEnabledPrefTest, DefaultState) {
  EXPECT_FALSE(TabHoverCardController::AreHoverCardImagesEnabled());
}
