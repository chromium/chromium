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
#include "components/omnibox/browser/omnibox_prefs.h"
#include "content/public/test/browser_test.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/url_constants.h"

namespace {

class LensOverlayPageActionIconViewTestBase : public InProcessBrowserTest {
 public:
  LensOverlayPageActionIconViewTestBase() = default;
  LensOverlayPageActionIconViewTestBase(
      const LensOverlayPageActionIconViewTestBase&) = delete;
  LensOverlayPageActionIconViewTestBase& operator=(
      const LensOverlayPageActionIconViewTestBase&) = delete;
  ~LensOverlayPageActionIconViewTestBase() override = default;

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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class LensOverlayPageActionIconViewTest
    : public LensOverlayPageActionIconViewTestBase {
 public:
  LensOverlayPageActionIconViewTest() {
    scoped_feature_list_.InitWithFeatures({lens::features::kLensOverlay}, {});
  }
};

class LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled
    : public LensOverlayPageActionIconViewTestBase {
 public:
  LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay, {{"omnibox-entry-point", "false"}});
  }
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

IN_PROC_BROWSER_TEST_F(LensOverlayPageActionIconViewTest,
                       DoesNotShowWhenSettingDisabled) {
  // Disable the setting.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               false);

  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

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

IN_PROC_BROWSER_TEST_F(
    LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled,
    DoesNotExistWhenOmniboxFeatureParamDisabled) {
  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  LensOverlayPageActionIconView* icon_view = lens_overlay_icon_view();
  EXPECT_EQ(nullptr, icon_view);
}

IN_PROC_BROWSER_TEST_F(LensOverlayPageActionIconViewTest,
                       RespectsShowShortcutPreference) {
  // Ensure the shortcut pref starts enabled.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               true);

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

  // Disable the preference, the entrypoint should immediately disappear.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               false);
  EXPECT_FALSE(icon_view->GetVisible());

  // Re-enable the preference, the entrypoint should immediately become visible.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               true);
  EXPECT_TRUE(icon_view->GetVisible());
}

}  // namespace
