// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/button_test_api.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/test/base/scoped_channel_override.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {
const char kFirstTestFeatureId[] = "feature-1";
BASE_FEATURE(kTestFeature1, "FeatureName1", base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

class ChromeLabsUiTest : public DialogBrowserTest {
 public:
  ChromeLabsUiTest()
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
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChromeLabs,
        {{features::kChromeLabsActivationPercentage.name, "100"}});
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"Feature 1", u"Feature description", "",
         version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);
  }

  void SetUpOnMainThread() override {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser()->profile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs,
                                     features::IsToolbarPinningEnabled());
    views::test::WaitForAnimatingLayoutManager(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->pinned_toolbar_actions_container());
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble bounds may exceed display's work area.
    // https://crbug.com/893292
    set_should_verify_dialog_bounds(false);
    views::Button* chrome_labs_button =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->GetChromeLabsButton();
    views::test::ButtonTestApi(chrome_labs_button)
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(), 0, 0));
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsUiTest, InvokeUi_default) {
  set_baseline("2810222");
  ShowAndVerifyUi();
}

class ChromeLabsMultipleFeaturesUiTest : public DialogBrowserTest {
 public:
  ChromeLabsMultipleFeaturesUiTest()
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
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChromeLabs,
        {{features::kChromeLabsActivationPercentage.name, "100"}});
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
    scoped_chrome_labs_model_data_.SetModelDataForTesting(
        std::move(test_feature_info));
  }

  void SetUpOnMainThread() override {
    PinnedToolbarActionsModel* const actions_model =
        PinnedToolbarActionsModel::Get(browser()->profile());
    actions_model->UpdatePinnedState(kActionShowChromeLabs,
                                     features::IsToolbarPinningEnabled());
    views::test::WaitForAnimatingLayoutManager(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->pinned_toolbar_actions_container());
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble bounds may exceed display's work area.
    // https://crbug.com/893292
    set_should_verify_dialog_bounds(false);
    views::Button* chrome_labs_button =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->GetChromeLabsButton();
    views::test::ButtonTestApi(chrome_labs_button)
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(), 0, 0));
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
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsMultipleFeaturesUiTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)
