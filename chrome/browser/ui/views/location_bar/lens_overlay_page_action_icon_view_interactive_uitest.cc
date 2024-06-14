// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_page_action_icon_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/url_constants.h"

namespace {

class LensOverlayPageActionIconViewTest : public InProcessBrowserTest {
 public:
  LensOverlayPageActionIconViewTest() {
    scoped_feature_list_.InitWithFeatures({lens::features::kLensOverlay}, {});
  }
  LensOverlayPageActionIconViewTest(const LensOverlayPageActionIconViewTest&) =
      delete;
  LensOverlayPageActionIconViewTest& operator=(
      const LensOverlayPageActionIconViewTest&) = delete;
  ~LensOverlayPageActionIconViewTest() override = default;

  LensOverlayPageActionIconView* lens_overlay_icon_view() {
    views::View* const icon_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kLensOverlayPageActionIconElementId,
            browser()->window()->GetElementContext());
    return icon_view
               ? views::AsViewClass<LensOverlayPageActionIconView>(icon_view)
               : nullptr;
  }

  LocationBarView* location_bar() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return views::AsViewClass<LocationBarView>(
        browser_view->toolbar()->location_bar());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensOverlayPageActionIconViewTest,
                       ShowsWhenLocationBarFocused) {
  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  LensOverlayPageActionIconView* icon_view = lens_overlay_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());

  // Focus in the location bar should show the icon.
  base::RunLoop run_loop;
  icon_view->set_update_callback_for_testing(run_loop.QuitClosure());
  location_bar()->FocusLocation(false);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  run_loop.Run();
  EXPECT_TRUE(icon_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(LensOverlayPageActionIconViewTest, DoesNotShowOnNTP) {
  // Navigate to the NTP.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));

  LensOverlayPageActionIconView* icon_view = lens_overlay_icon_view();
  views::FocusManager* focus_manager = icon_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(icon_view->GetVisible());

  // The icon should remain hidden despite focus in the location bar.
  base::RunLoop run_loop;
  icon_view->set_update_callback_for_testing(run_loop.QuitClosure());
  location_bar()->FocusLocation(false);
  EXPECT_TRUE(focus_manager->GetFocusedView());
  run_loop.Run();
  EXPECT_FALSE(icon_view->GetVisible());
}

}  // namespace
