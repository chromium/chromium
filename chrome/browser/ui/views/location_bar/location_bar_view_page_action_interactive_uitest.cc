// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_page_action_icon_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "url/url_constants.h"

using ::testing::MatchesRegex;

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";

// The tests in this file verify the behavior of the page action icon. There can
// be multiple instances of page actions with different action items. When the
// feature `kPageActionsMigration` is disabled, the old implementation of the
// Page Action Icon, which was feature-specific, will be used. When the feature
// is enabled, the generic `PageActionView` will be utilized. The page action
// view instances include:
// 1. LensOverlay
// 2. [Add additional instances here as needed] ...
class CommonLocationBarViewPageActionTestBase : public InProcessBrowserTest {
 public:
  CommonLocationBarViewPageActionTestBase() = default;
  CommonLocationBarViewPageActionTestBase(
      const CommonLocationBarViewPageActionTestBase&) = delete;
  CommonLocationBarViewPageActionTestBase& operator=(
      const CommonLocationBarViewPageActionTestBase&) = delete;
  ~CommonLocationBarViewPageActionTestBase() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  LensOverlayPageActionIconView* lens_overlay_icon_view() {
    views::View* const icon_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kLensOverlayPageActionIconElementId,
            browser()->window()->GetElementContext());
    return icon_view
               ? views::AsViewClass<LensOverlayPageActionIconView>(icon_view)
               : nullptr;
  }

  page_actions::PageActionView* lens_overlay_page_action_view() {
    return static_cast<page_actions::PageActionView*>(
        views::test::AnyViewMatchingPredicate(
            location_bar(), [=](const views::View* candidate) -> bool {
              return IsViewClass<page_actions::PageActionView>(candidate) &&
                     static_cast<const page_actions::PageActionView*>(candidate)
                             ->GetActionId() ==
                         kActionSidePanelShowLensOverlayResults;
            }));
  }

  LocationBarView* location_bar() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return views::AsViewClass<LocationBarView>(
        browser_view->toolbar()->location_bar());
  }
};

// The parameter indicates whether the migration is enabled or not.
class LocationBarViewPageActionTest
    : public CommonLocationBarViewPageActionTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  LocationBarViewPageActionTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {lens::features::kLensOverlay, ::features::kPageActionsMigration},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {lens::features::kLensOverlay}, {::features::kPageActionsMigration});
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LocationBarViewPageActionTest,
                         ::testing::Values(false, true),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigrationEnabled"
                                             : "MigrationDisabled";
                         });

class LocationBarViewPageActionTestOmniboxEntryPointDisabled
    : public CommonLocationBarViewPageActionTestBase {
 public:
  LocationBarViewPageActionTestOmniboxEntryPointDisabled() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay, {{"omnibox-entry-point", "false"}});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(LocationBarViewPageActionTest,
                       ShowsWhenLocationBarFocused) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  if (GetParam()) {  // Migration Enabled..
    page_actions::PageActionView* page_action_view =
        lens_overlay_page_action_view();
    ASSERT_NE(nullptr, page_action_view);
    views::FocusManager* focus_manager = page_action_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(focus_manager->GetFocusedView());
    EXPECT_FALSE(page_action_view->GetVisible());

    location_bar()->FocusLocation(false);
    EXPECT_TRUE(focus_manager->GetFocusedView());
    EXPECT_TRUE(page_action_view->GetVisible());
  } else {  // Migration Disabled..
    LensOverlayPageActionIconView* icon_view = lens_overlay_icon_view();
    ASSERT_NE(nullptr, icon_view);
    views::FocusManager* focus_manager = icon_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(focus_manager->GetFocusedView());
    EXPECT_FALSE(icon_view->GetVisible());

    base::RunLoop run_loop;
    icon_view->set_update_callback_for_testing(run_loop.QuitClosure());
    location_bar()->FocusLocation(false);
    EXPECT_TRUE(focus_manager->GetFocusedView());
    run_loop.Run();
    EXPECT_TRUE(icon_view->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_P(LocationBarViewPageActionTest, DoesNotShowOnNTP) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));

  if (GetParam()) {  // Migration Enabled..
    page_actions::PageActionView* page_action_view =
        lens_overlay_page_action_view();
    ASSERT_NE(nullptr, page_action_view);
    views::FocusManager* focus_manager = page_action_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(focus_manager->GetFocusedView());
    EXPECT_FALSE(page_action_view->GetVisible());

    location_bar()->FocusLocation(false);
    EXPECT_TRUE(focus_manager->GetFocusedView());
    EXPECT_FALSE(page_action_view->GetVisible());
  } else {  // Migration Disabled..
    LensOverlayPageActionIconView* icon_view = lens_overlay_icon_view();
    ASSERT_NE(nullptr, icon_view);
    views::FocusManager* focus_manager = icon_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(focus_manager->GetFocusedView());
    EXPECT_FALSE(icon_view->GetVisible());

    base::RunLoop run_loop;
    icon_view->set_update_callback_for_testing(run_loop.QuitClosure());
    location_bar()->FocusLocation(false);
    EXPECT_TRUE(focus_manager->GetFocusedView());
    run_loop.Run();
    EXPECT_FALSE(icon_view->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_P(LocationBarViewPageActionTest,
                       OpensNewTabWhenEnteredThroughKeyboard) {
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));

  if (GetParam()) {  // Migration Enabled.
    page_actions::PageActionView* page_action_view =
        lens_overlay_page_action_view();
    ASSERT_NE(nullptr, page_action_view);
    views::FocusManager* focus_manager = page_action_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(focus_manager->GetFocusedView());
    EXPECT_FALSE(page_action_view->GetVisible());

    location_bar()->FocusLocation(false);
    EXPECT_TRUE(focus_manager->GetFocusedView());
    EXPECT_TRUE(page_action_view->GetVisible());
  } else {  // Migration Disabled.
    LensOverlayPageActionIconView* icon_view = lens_overlay_icon_view();
    ASSERT_NE(nullptr, icon_view);
    page_actions::PageActionView* page_action_view =
        lens_overlay_page_action_view();
    views::FocusManager* focus_manager = icon_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(focus_manager->GetFocusedView());
    EXPECT_FALSE(icon_view->GetVisible());
    EXPECT_FALSE(page_action_view->GetVisible());

    base::RunLoop run_loop;
    icon_view->set_update_callback_for_testing(run_loop.QuitClosure());
    location_bar()->FocusLocation(false);
    EXPECT_TRUE(focus_manager->GetFocusedView());
    run_loop.Run();
    EXPECT_TRUE(icon_view->GetVisible());
    EXPECT_FALSE(page_action_view->GetVisible());

    ui_test_utils::TabAddedWaiter tab_add(browser());
    icon_view->execute_with_keyboard_source_for_testing();
    auto* new_tab_contents = tab_add.Wait();

    EXPECT_TRUE(new_tab_contents);
    content::WaitForLoadStop(new_tab_contents);
    EXPECT_THAT(new_tab_contents->GetLastCommittedURL().query(),
                MatchesRegex("ep=crmntob&re=df&s=4&st=\\d+&lm=.+"));
  }
}

IN_PROC_BROWSER_TEST_P(LocationBarViewPageActionTest,
                       DoesNotShowWhenSettingDisabled) {
  // Disable the setting.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               false);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  if (GetParam()) {  // Migration Enabled.
    page_actions::PageActionView* page_action_view =
        lens_overlay_page_action_view();
    ASSERT_NE(nullptr, page_action_view);
    views::FocusManager* focus_manager = page_action_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(page_action_view->GetVisible());

    location_bar()->FocusLocation(false);
    EXPECT_TRUE(focus_manager->GetFocusedView());
    EXPECT_FALSE(page_action_view->GetVisible());
  } else {  // Migration Disabled.
    LensOverlayPageActionIconView* icon_view = lens_overlay_icon_view();
    ASSERT_NE(nullptr, icon_view);
    views::FocusManager* focus_manager = icon_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(icon_view->GetVisible());

    base::RunLoop run_loop;
    icon_view->set_update_callback_for_testing(run_loop.QuitClosure());
    location_bar()->FocusLocation(false);
    EXPECT_TRUE(focus_manager->GetFocusedView());
    run_loop.Run();
    EXPECT_FALSE(icon_view->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_P(LocationBarViewPageActionTest,
                       RespectsShowShortcutPreference) {
  // Ensure the shortcut pref starts enabled.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               true);

  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  if (GetParam()) {  // Migration Enabled.
    page_actions::PageActionView* page_action_view =
        lens_overlay_page_action_view();
    ASSERT_NE(nullptr, page_action_view);
    views::FocusManager* focus_manager = page_action_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(page_action_view->GetVisible());

    location_bar()->FocusLocation(false);
    EXPECT_TRUE(focus_manager->GetFocusedView());
    EXPECT_TRUE(page_action_view->GetVisible());

    browser()->profile()->GetPrefs()->SetBoolean(
        omnibox::kShowGoogleLensShortcut, false);
    EXPECT_FALSE(page_action_view->GetVisible());

    browser()->profile()->GetPrefs()->SetBoolean(
        omnibox::kShowGoogleLensShortcut, true);
    EXPECT_TRUE(page_action_view->GetVisible());
  } else {  // Migration Disabled.
    LensOverlayPageActionIconView* icon_view = lens_overlay_icon_view();
    page_actions::PageActionView* page_action_view =
        lens_overlay_page_action_view();
    ASSERT_NE(nullptr, icon_view);
    views::FocusManager* focus_manager = icon_view->GetFocusManager();
    focus_manager->ClearFocus();
    EXPECT_FALSE(icon_view->GetVisible());

    {
      base::RunLoop run_loop;
      icon_view->set_update_callback_for_testing(run_loop.QuitClosure());
      location_bar()->FocusLocation(false);
      EXPECT_TRUE(focus_manager->GetFocusedView());
      run_loop.Run();
    }

    EXPECT_TRUE(icon_view->GetVisible());
    EXPECT_FALSE(page_action_view->GetVisible());

    browser()->profile()->GetPrefs()->SetBoolean(
        omnibox::kShowGoogleLensShortcut, false);
    EXPECT_FALSE(icon_view->GetVisible());
    EXPECT_FALSE(page_action_view->GetVisible());

    browser()->profile()->GetPrefs()->SetBoolean(
        omnibox::kShowGoogleLensShortcut, true);
    EXPECT_TRUE(icon_view->GetVisible());
    EXPECT_FALSE(page_action_view->GetVisible());
  }
}

}  // namespace
