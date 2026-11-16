// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_location_bar_host.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

// With Omnibox_HostUnboundedInLocationBar enabled, LocationBarView builds
// OmniboxPopupViewFullWebUI with OmniboxPopupLocationBarHost (no popup
// presenter), and ShouldDrawFullPopupShadowInWebUI() is false even with
// kOmniboxFullWebUIShadow enabled.
class OmniboxPopupLocationBarHostBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxPopupLocationBarHostBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::internal::kWebUIOmniboxFullPopup,
          {{"Omnibox_HostUnboundedInLocationBar", "true"}}},
         {omnibox::internal::kWebUIOmniboxPopup, {}},
         {omnibox::kOmniboxFullWebUIShadow, {}}},
        {features::kWebUILocationBar, features::kWebUIToolbar});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostBrowserTest,
                       UsesLocationBarPopupHostWhenParamEnabled) {
  LocationBarView* location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->location_bar_view();
  ASSERT_TRUE(location_bar);
  EXPECT_TRUE(omnibox::ShouldHostUnboundedOmniboxInLocationBar());
  EXPECT_FALSE(omnibox::ShouldDrawFullPopupShadowInWebUI());
  EXPECT_TRUE(location_bar->is_full_webui_omnibox());
  EXPECT_FALSE(location_bar->GetOmniboxPopupPresenterForTesting());
  OmniboxPopupView* popup_view = location_bar->GetOmniboxPopupView();
  ASSERT_TRUE(popup_view);
  EXPECT_TRUE(popup_view->popup_host());
}

// With Omnibox_HostUnboundedInLocationBar off, the Full WebUI popup uses its
// popup widget presenter, and ShouldDrawFullPopupShadowInWebUI() follows
// kOmniboxFullWebUIShadow.
class OmniboxPopupLocationBarHostParamOffBrowserTest
    : public InProcessBrowserTest {
 public:
  OmniboxPopupLocationBarHostParamOffBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::internal::kWebUIOmniboxFullPopup,
          {{"Omnibox_HostUnboundedInLocationBar", "false"}}},
         {omnibox::internal::kWebUIOmniboxPopup, {}},
         {omnibox::kOmniboxFullWebUIShadow, {}}},
        {features::kWebUILocationBar, features::kWebUIToolbar});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostParamOffBrowserTest,
                       UsesPopupPresenterWhenParamDisabled) {
  LocationBarView* location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->location_bar_view();
  ASSERT_TRUE(location_bar);
  EXPECT_FALSE(omnibox::ShouldHostUnboundedOmniboxInLocationBar());
  EXPECT_TRUE(omnibox::ShouldDrawFullPopupShadowInWebUI());
  EXPECT_TRUE(location_bar->is_full_webui_omnibox());
  OmniboxPopupPresenterBase* presenter =
      location_bar->GetOmniboxPopupPresenterForTesting();
  ASSERT_TRUE(presenter);
  EXPECT_EQ(presenter, location_bar->GetOmniboxPopupView()->popup_host());
}
