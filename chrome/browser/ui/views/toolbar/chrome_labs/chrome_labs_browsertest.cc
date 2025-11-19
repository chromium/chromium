// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/version_info/channel.h"
#include "components/webui/flags/feature_entry_macros.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/button_test_api.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/test/base/scoped_channel_override.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {
const char kFirstTestFeatureId[] = "feature-1";
BASE_FEATURE(kTestFeature1, "FeatureName1", base::FEATURE_ENABLED_BY_DEFAULT);

// Helper class for setting up Chrome Labs in browser tests. This class
// handles the necessary setup for the Chrome Labs feature to be active and
// provides methods for interacting with its UI.
class ChromeLabsTestHelper {
 public:
  explicit ChromeLabsTestHelper(std::vector<LabInfo> feature_info)
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        // If the channel name is empty on branded builds, STABLE is returned.
        // Force the channel to be a non-stable channel otherwise Chrome Labs
        // will not be shown.
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_({{kFirstTestFeatureId, "", "",
                                  flags_ui::FlagsState::GetCurrentPlatform(),
                                  FEATURE_VALUE_TYPE(kTestFeature1)}}) {
    scoped_chrome_labs_model_data_.SetModelDataForTesting(
        std::move(feature_info));
    ForceChromeLabsActivationForTesting();
  }

  // Pins the Chrome Labs button to the toolbar. Must be called from
  // SetUpOnMainThread().
  void PinChromeLabsButton(Browser* browser) {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser->profile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs, true);
    views::test::WaitForAnimatingLayoutManager(
        BrowserView::GetBrowserViewForBrowser(browser)
            ->toolbar()
            ->pinned_toolbar_actions_container());
  }

  // Clicks the Chrome Labs button to show the bubble.
  void ShowChromeLabsBubble(Browser* browser) {
    views::Button* chrome_labs_button =
        BrowserView::GetBrowserViewForBrowser(browser)
            ->toolbar()
            ->GetChromeLabsButton();
    views::test::ButtonTestApi(chrome_labs_button).NotifyDefaultMouseClick();
    EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
        kToolbarChromeLabsBubbleElementId));
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

}  // namespace

class ChromeLabsBrowserTest : public InProcessBrowserTest {
 public:
  ChromeLabsBrowserTest() {
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"Feature 1", u"Feature description", "",
         version_info::Channel::STABLE}};
    helper_ =
        std::make_unique<ChromeLabsTestHelper>(std::move(test_feature_info));
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    helper_->PinChromeLabsButton(browser());
  }

  void ShowBubble() { helper_->ShowChromeLabsBubble(browser()); }

 private:
  std::unique_ptr<ChromeLabsTestHelper> helper_;
};

// Asserts the browser process does not crash if the browser window is closed
// while the labs bubble is open.
IN_PROC_BROWSER_TEST_F(ChromeLabsBrowserTest, ClosesWithoutCrashing) {
  ShowBubble();
  CloseBrowserSynchronously(browser());
}

class ChromeLabsUiTest : public DialogBrowserTest {
 public:
  ChromeLabsUiTest() {
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"Feature 1", u"Feature description", "",
         version_info::Channel::STABLE}};
    helper_ =
        std::make_unique<ChromeLabsTestHelper>(std::move(test_feature_info));
  }

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    helper_->PinChromeLabsButton(browser());
  }
  void ShowUi(const std::string& name) override {
    // Bubble bounds may exceed display's work area.
    // https://crbug.com/893292
    set_should_verify_dialog_bounds(false);
    helper_->ShowChromeLabsBubble(browser());
  }

 private:
  std::unique_ptr<ChromeLabsTestHelper> helper_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsUiTest, InvokeUi_default) {
  set_baseline("2810222");
  ShowAndVerifyUi();
}

class ChromeLabsMultipleFeaturesUiTest : public DialogBrowserTest {
 public:
  ChromeLabsMultipleFeaturesUiTest() {
    // Add a lot of features to trigger the scrolling functionality.
    // All the entries are linked to the same feature using kFirstTestFeatureId
    // since it doesn't matter what feature is linked.
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"Feature 1", u"Feature description 1", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 2", u"Feature description 2", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 3", u"Feature description 3", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 4", u"Feature description 4", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 5", u"Feature description 5", "",
         version_info::Channel::STABLE},
        {kFirstTestFeatureId, u"Feature 6", u"Feature description 6", "",
         version_info::Channel::STABLE},
    };
    helper_ =
        std::make_unique<ChromeLabsTestHelper>(std::move(test_feature_info));
  }

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    helper_->PinChromeLabsButton(browser());
  }
  void ShowUi(const std::string& name) override {
    // Bubble bounds may exceed display's work area.
    // https://crbug.com/893292
    set_should_verify_dialog_bounds(false);
    helper_->ShowChromeLabsBubble(browser());

    // Scroll to a little after the dialog inset to ensure that scrolling does
    // not make the contents too close to the title.
    browser()
        ->GetFeatures()
        .chrome_labs_coordinator()
        ->GetChromeLabsBubbleView()
        ->GetScrollViewForTesting()
        ->ScrollByOffset(
            gfx::PointF(0, views::LayoutProvider::Get()
                                   ->GetInsetsMetric(views::INSETS_DIALOG)
                                   .top() +
                               2));
  }

 private:
  std::unique_ptr<ChromeLabsTestHelper> helper_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsMultipleFeaturesUiTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

#endif  // !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)
