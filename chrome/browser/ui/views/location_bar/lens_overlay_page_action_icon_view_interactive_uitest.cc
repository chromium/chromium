// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/376283383): This file should be moved closer to the
// `LensOverlayEntryPointController` once the page actions migration is
// complete.

#include <memory>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
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
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/test/test_event.h"
#include "ui/views/test/widget_test.h"
#include "url/url_constants.h"

using ::testing::MatchesRegex;

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";

class LensOverlayPageActionIconViewTestBase : public InProcessBrowserTest {
 public:
  LensOverlayPageActionIconViewTestBase() = default;
  LensOverlayPageActionIconViewTestBase(
      const LensOverlayPageActionIconViewTestBase&) = delete;
  LensOverlayPageActionIconViewTestBase& operator=(
      const LensOverlayPageActionIconViewTestBase&) = delete;
  ~LensOverlayPageActionIconViewTestBase() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  LensOverlayPageActionIconView* lens_overlay_icon_view() {
    return BrowserElementsViews::From(browser())
        ->GetViewAs<LensOverlayPageActionIconView>(
            kLensOverlayPageActionIconElementId);
  }

  LocationBarView* location_bar() {
    return BrowserElementsViews::From(browser())->GetViewAs<LocationBarView>(
        kLocationBarElementId);
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

  page_actions::PageActionController* page_action_controller() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->page_action_controller();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The parameter indicates whether the page action migration is enabled or not.
class LensOverlayPageActionIconViewTest
    : public LensOverlayPageActionIconViewTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  LensOverlayPageActionIconViewTest() {
    if (IsMigrationEnabled()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {
              {lens::features::kLensOverlay, {}},
              {lens::features::kLensOverlayOmniboxEntryPoint, {}},
              {
                  ::features::kPageActionsMigration,
                  {
                      {::features::kPageActionsMigrationLensOverlay.name,
                       "true"},
                  },
              },
          },
          {lens::features::kLensOverlayKeyboardSelection,
           omnibox::kAiModeOmniboxEntryPoint});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {lens::features::kLensOverlay,
           lens::features::kLensOverlayOmniboxEntryPoint},
          {lens::features::kLensOverlayKeyboardSelection,
           ::features::kPageActionsMigration,
           omnibox::kAiModeOmniboxEntryPoint});
    }
  }

  // Returns the page action view that should be enabled for the current
  // feature flag state.
  views::LabelButton* PageActionView() {
    if (IsMigrationEnabled()) {
      return lens_overlay_page_action_view();
    }
    return lens_overlay_icon_view();
  }

  void FocusLocationBarAndWaitForUpdate() {
    // Focus updates are posted as a task for the legacy path.
    base::RunLoop run_loop;
    if (!IsMigrationEnabled()) {
      lens_overlay_icon_view()->set_update_callback_for_testing(
          run_loop.QuitClosure());
    }
    location_bar()->FocusLocation(false);
    EXPECT_TRUE(PageActionView()->GetFocusManager()->GetFocusedView());
    if (!IsMigrationEnabled()) {
      run_loop.Run();
    }
  }

 protected:
  bool IsMigrationEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         LensOverlayPageActionIconViewTest,
                         ::testing::Values(false, true),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigrationEnabled"
                                             : "MigrationDisabled";
                         });

class LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled
    : public LensOverlayPageActionIconViewTest {
 public:
  LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled() {
    scoped_feature_list_.Reset();
    if (IsMigrationEnabled()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {base::test::FeatureRefAndParams(lens::features::kLensOverlay,
                                           {{"omnibox-entry-point", "false"}}),
           base::test::FeatureRefAndParams(
               ::features::kPageActionsMigration,
               {{::features::kPageActionsMigrationLensOverlay.name, "true"}})},
          {});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {base::test::FeatureRefAndParams(lens::features::kLensOverlay,
                                           {{"omnibox-entry-point", "false"}})},
          {::features::kPageActionsMigration});
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<bool>& info) {
      return info.param ? "MigrationEnabled" : "MigrationDisabled";
    });

IN_PROC_BROWSER_TEST_P(LensOverlayPageActionIconViewTest,
                       ShowsWhenLocationBarFocused) {
  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  views::View* page_action_view = PageActionView();
  views::FocusManager* focus_manager = page_action_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(page_action_view->GetVisible());

  // Focus in the location bar should show the icon.
  FocusLocationBarAndWaitForUpdate();
  EXPECT_TRUE(page_action_view->GetVisible());
}

IN_PROC_BROWSER_TEST_P(LensOverlayPageActionIconViewTest,
                       OpensNewTabWhenEnteredThroughKeyboard) {
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  // Navigate to a non-NTP page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
  // We need to wait for paint in order to take a screenshot of the page.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));

  views::View* page_action_view = PageActionView();
  views::FocusManager* focus_manager = page_action_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(page_action_view->GetVisible());

  // Focus in the location bar should show the icon.
  FocusLocationBarAndWaitForUpdate();
  EXPECT_TRUE(page_action_view->GetVisible());

  // Executing the lens overlay icon view with keyboard source should open a new
  // tab.
  ui_test_utils::TabAddedWaiter tab_add(browser());
  if (IsMigrationEnabled()) {
    lens_overlay_page_action_view()->NotifyClick(
        ui::test::TestEvent(ui::EventType::kKeyPressed));
  } else {
    lens_overlay_icon_view()->execute_with_keyboard_source_for_testing();
  }
  auto* new_tab_contents = tab_add.Wait();

  EXPECT_TRUE(new_tab_contents);
  content::WaitForLoadStop(new_tab_contents);
  EXPECT_THAT(new_tab_contents->GetLastCommittedURL().GetQuery(),
              MatchesRegex("ep=crmntob&re=df&s=4&st=\\d+&lm=.+"));
}

IN_PROC_BROWSER_TEST_P(LensOverlayPageActionIconViewTest,
                       DoesNotShowWhenSettingDisabled) {
  // Disable the setting.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               false);

  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  views::View* page_action_view = PageActionView();
  views::FocusManager* focus_manager = page_action_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(page_action_view->GetVisible());

  // The icon should remain hidden despite focus in the location bar.
  FocusLocationBarAndWaitForUpdate();
  EXPECT_FALSE(page_action_view->GetVisible());
}

IN_PROC_BROWSER_TEST_P(LensOverlayPageActionIconViewTest, DoesNotShowOnNTP) {
  // Navigate to the NTP.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));

  views::View* page_action_view = PageActionView();
  views::FocusManager* focus_manager = page_action_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(page_action_view->GetVisible());

  // The icon should remain hidden despite focus in the location bar.
  FocusLocationBarAndWaitForUpdate();

  EXPECT_FALSE(page_action_view->GetVisible());
}

IN_PROC_BROWSER_TEST_P(
    LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled,
    DoesNotShowWhenOmniboxFeatureParamDisabled) {
  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  location_bar()->FocusLocation(false);
  if (IsMigrationEnabled()) {
    views::View* page_action_view = PageActionView();
    ASSERT_NE(nullptr, page_action_view);
    EXPECT_FALSE(page_action_view->GetVisible());
  } else {
    EXPECT_EQ(nullptr, PageActionView());
  }
}

IN_PROC_BROWSER_TEST_P(LensOverlayPageActionIconViewTest,
                       RespectsShowShortcutPreference) {
  // Ensure the shortcut pref starts enabled.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               true);

  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  views::View* page_action_view = PageActionView();
  views::FocusManager* focus_manager = page_action_view->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  EXPECT_FALSE(page_action_view->GetVisible());

  // Focus in the location bar should show the icon.
  FocusLocationBarAndWaitForUpdate();

  // Disable the preference, the entrypoint should immediately disappear.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               false);
  EXPECT_FALSE(page_action_view->GetVisible());

  // Re-enable the preference, the entrypoint should immediately become
  // visible.
  browser()->profile()->GetPrefs()->SetBoolean(omnibox::kShowGoogleLensShortcut,
                                               true);
  EXPECT_TRUE(page_action_view->GetVisible());
}

}  // namespace
