// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"
#include "url/gurl.h"

namespace {

class TestLocationBarObserver : public LocationBar::Observer {
 public:
  explicit TestLocationBarObserver(base::OnceClosure on_bounds_changed)
      : on_bounds_changed_(std::move(on_bounds_changed)) {}

  void OnLocationBarBoundsChanged() override {
    ASSERT_FALSE(on_bounds_changed_.is_null());
    std::move(on_bounds_changed_).Run();
  }

 private:
  base::OnceClosure on_bounds_changed_;
};

class WebUILocationBarBrowserTest : public InProcessBrowserTest {
 public:
  WebUILocationBarBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUILocationBar},
        {});
  }

  LocationBar* GetLocationBar() {
    return BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBar();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, GetAnchor) {
  WaitForInitialWebUIToolbar(browser());

  auto* location_bar = GetLocationBar();

  // Wait until visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return location_bar->GetAnchorOrNull(); }));
  ui::TrackedElement* anchor = location_bar->GetAnchorOrNull();
  ASSERT_TRUE(anchor);
  ASSERT_TRUE(anchor->IsA<ui::TrackedElementWebUI>());
  gfx::Rect location_bar_rect = anchor->GetScreenBounds();
  // Check that the anchor is the expected height, and closely to the right of
  // the reload button, with the expected margin.
  EXPECT_EQ(location_bar_rect.height(),
            GetLayoutConstant(LayoutConstant::kLocationBarHeight))
      << location_bar_rect.ToString();

  ui::TrackedElement* reload =
      BrowserElements::From(browser())->GetElement(kReloadButtonElementId);
  gfx::Rect reload_rect = reload->GetScreenBounds();
  EXPECT_EQ(location_bar_rect.x() - reload_rect.right(),
            GetLayoutConstant(LayoutConstant::kLocationBarMargin))
      << location_bar_rect.ToString() << " " << reload_rect.ToString();
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, Accessors) {
  auto* location_bar = GetLocationBar();
  EXPECT_EQ(browser(), location_bar->GetBrowser());
  EXPECT_EQ(browser()->GetProfile(), location_bar->GetProfile());
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, Bounds) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* location_bar = GetLocationBar();

  WaitForInitialWebUIToolbar(browser());

  // Wait until visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return location_bar->GetAnchorOrNull(); }));

  // Since GetAnchor() is tested, we can partly rely on it.
  gfx::Rect screen_bounds = location_bar->BoundsInScreen();
  EXPECT_EQ(screen_bounds, location_bar->GetAnchorOrNull()->GetScreenBounds());
  // Also should be kinda wide.
  EXPECT_GT(screen_bounds.width(), browser_view->width() / 2);

  gfx::Rect relative_bounds = location_bar->Bounds();
  EXPECT_EQ(relative_bounds.size(), screen_bounds.size());

  ToolbarButtonProvider* toolbar_button_provider = browser_view->toolbar();
  auto* webview = toolbar_button_provider->GetWebUIToolbarViewForTesting();

  gfx::Vector2d offset =
      screen_bounds.origin() - webview->GetBoundsInScreen().origin();
  EXPECT_EQ(offset.x(), relative_bounds.x());
  EXPECT_EQ(offset.y(), relative_bounds.y());

  // Make sure that bounds change observer gets notified.
  base::RunLoop run_loop;
  TestLocationBarObserver bounds_observer(run_loop.QuitClosure());
  base::ScopedObservation<LocationBar, LocationBar::Observer> obs(
      &bounds_observer);
  obs.Observe(location_bar);
  browser_view->SetSize(
      gfx::Size(browser_view->width() - 100, browser_view->height()));
  run_loop.Run();
}

// Test that basic state management of the omnibox works --- e.g. it gets
// the URL as its state when navigating and switching tabs.
IN_PROC_BROWSER_TEST_F(WebUILocationBarBrowserTest, BasicOmniboxState) {
  WaitForInitialWebUIToolbar(browser());
  LocationBar* location_bar = GetLocationBar();
  auto* tab_strip_model = browser()->tab_strip_model();

  auto* omnibox = location_bar->GetOmniboxView();
  ASSERT_TRUE(omnibox);
  EXPECT_EQ("about:blank", base::UTF16ToUTF8(omnibox->GetText()));

  chrome::NewTab(browser());
  tab_strip_model->SelectTabAt(1);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));
  EXPECT_EQ("chrome://version", base::UTF16ToUTF8(omnibox->GetText()));

  tab_strip_model->SelectTabAt(0);
  EXPECT_EQ("about:blank", base::UTF16ToUTF8(omnibox->GetText()));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://chrome-urls")));
  EXPECT_EQ("chrome://chrome-urls", base::UTF16ToUTF8(omnibox->GetText()));

  tab_strip_model->SelectTabAt(1);
  EXPECT_EQ("chrome://version", base::UTF16ToUTF8(omnibox->GetText()));
}

}  // namespace
