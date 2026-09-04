// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/376283383): This file should be moved closer to the
// `LensOverlayEntryPointController` once the page actions migration is
// complete.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  LocationBar* location_bar() { return GetBrowserView()->GetLocationBar(); }

  page_actions::PageActionTestAccessor PageActionAccessor() {
    return page_actions::PageActionTestAccessor(
        browser(), kActionSidePanelShowLensOverlayResults);
  }

  void CheckVisibility(bool expected) {
    EXPECT_EQ(expected, PageActionAccessor().GetVisible());
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
    : public LensOverlayPageActionIconViewTestBase {
 public:
  LensOverlayPageActionIconViewTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {lens::features::kLensOverlay, {}},
            {lens::features::kLensOverlayOmniboxEntryPoint, {}},
        },
        {lens::features::kLensOverlayKeyboardSelection});
  }

  void FocusLocationBarAndWaitForUpdate() {
    location_bar()->FocusLocation(/*is_user_initiated=*/false,
                                  /*clear_focus_if_failed=*/false);
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return location_bar()->IsFocusWithin(); }));
    EXPECT_TRUE(GetBrowserView()->GetFocusManager()->GetFocusedView());
  }

  void WaitForNoLocationBarFocus() {
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return location_bar()->IsFocusWithin() == false; }));
  }
};

class LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled
    : public LensOverlayPageActionIconViewTest {
 public:
  LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(lens::features::kLensOverlay,
                                         {{"omnibox-entry-point", "false"}})},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayPageActionIconViewTest,
                       ShowsWhenLocationBarFocused) {
  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  views::FocusManager* focus_manager = GetBrowserView()->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  WaitForNoLocationBarFocus();
  CheckVisibility(false);

  // Focus in the location bar should show the icon.
  FocusLocationBarAndWaitForUpdate();
  CheckVisibility(true);
}

IN_PROC_BROWSER_TEST_F(LensOverlayPageActionIconViewTest,
                       OpensNewTabWhenEnteredThroughKeyboard) {
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  // Navigate to a non-NTP page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
  // We need to wait for paint in order to take a screenshot of the page.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()
        ->GetTabStripModel()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));

  views::FocusManager* focus_manager = GetBrowserView()->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  WaitForNoLocationBarFocus();
  CheckVisibility(false);

  // Focus in the location bar should show the icon.
  FocusLocationBarAndWaitForUpdate();
  CheckVisibility(true);

  // Executing the lens overlay icon view with keyboard source should open a
  // new tab.
  ui_test_utils::TabAddedWaiter tab_add(browser());
  PageActionAccessor().Click(page_actions::PageActionTrigger::kKeyboard);

  auto* new_tab_contents = tab_add.Wait();

  EXPECT_TRUE(new_tab_contents);
  content::WaitForLoadStop(new_tab_contents);
  EXPECT_THAT(new_tab_contents->GetLastCommittedURL().GetQuery(),
              MatchesRegex("ep=crmntob&re=df&s=4&st=\\d+&lm=.+"));
}

IN_PROC_BROWSER_TEST_F(LensOverlayPageActionIconViewTest,
                       DoesNotShowWhenSettingDisabled) {
  // Disable the setting.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      omnibox::kShowGoogleLensShortcut, false);

  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  views::FocusManager* focus_manager = GetBrowserView()->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  WaitForNoLocationBarFocus();
  CheckVisibility(false);

  // The icon should remain hidden despite focus in the location bar.
  FocusLocationBarAndWaitForUpdate();
  CheckVisibility(false);
}

IN_PROC_BROWSER_TEST_F(LensOverlayPageActionIconViewTest, DoesNotShowOnNTP) {
  // Navigate to the NTP.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), chrome::ChromeUINewTabPageURLAsGURL()));

  auto page_action_accessor = PageActionAccessor();
  views::FocusManager* focus_manager = GetBrowserView()->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  WaitForNoLocationBarFocus();
  CheckVisibility(false);

  // The icon should remain hidden despite focus in the location bar.
  FocusLocationBarAndWaitForUpdate();
  CheckVisibility(false);
}

IN_PROC_BROWSER_TEST_F(
    LensOverlayPageActionIconViewTestOmniboxEntryPointDisabled,
    DoesNotShowWhenOmniboxFeatureParamDisabled) {
  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  FocusLocationBarAndWaitForUpdate();
  CheckVisibility(false);
}

IN_PROC_BROWSER_TEST_F(LensOverlayPageActionIconViewTest,
                       RespectsShowShortcutPreference) {
  // Ensure the shortcut pref starts enabled.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      omnibox::kShowGoogleLensShortcut, true);

  // Navigate to a non-NTP page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  views::FocusManager* focus_manager = GetBrowserView()->GetFocusManager();
  focus_manager->ClearFocus();
  EXPECT_FALSE(focus_manager->GetFocusedView());
  WaitForNoLocationBarFocus();
  CheckVisibility(false);

  // Focus in the location bar should show the icon.
  FocusLocationBarAndWaitForUpdate();

  // Disable the preference, the entrypoint should immediately disappear.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      omnibox::kShowGoogleLensShortcut, false);
  CheckVisibility(false);

  // Re-enable the preference, the entrypoint should immediately become
  // visible.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      omnibox::kShowGoogleLensShortcut, true);
  CheckVisibility(true);
}

}  // namespace
