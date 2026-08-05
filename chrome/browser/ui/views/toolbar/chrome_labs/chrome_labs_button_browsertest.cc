// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/unexpire_flags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "components/webui/flags/feature_entry_macros.h"
#include "content/public/test/browser_test.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/dot_indicator.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/test/base/scoped_channel_override.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

const char kFirstTestFeatureId[] = "feature-1";
BASE_FEATURE(kTestFeature1, base::FEATURE_ENABLED_BY_DEFAULT);
const char kSecondTestFeatureId[] = "feature-2";
BASE_FEATURE(kTestFeature2, base::FEATURE_DISABLED_BY_DEFAULT);
const char kExpiredFlagTestFeatureId[] = "expired-feature";
BASE_FEATURE(kTestFeatureExpired, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

class ChromeLabsButtonTest : public InProcessBrowserTest {
 public:
  ChromeLabsButtonTest()
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_({{kFirstTestFeatureId, "", "",
                                  flags_ui::FlagsState::GetCurrentPlatform(),
                                  FEATURE_VALUE_TYPE(kTestFeature1)}}) {
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"", u"", "", version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);
    ForceChromeLabsActivationForTesting();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);
  }

  views::Button* GetChromeLabsButton() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->GetChromeLabsButton();
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsButtonTest,
                       ShowAndHideChromeLabsBubbleOnPress) {
  views::Button* labs_button = GetChromeLabsButton();
  ASSERT_NE(labs_button, nullptr);
  ChromeLabsCoordinator* coordinator = ChromeLabsCoordinator::From(browser());
  ASSERT_NE(coordinator, nullptr);

#if BUILDFLAG(IS_CHROMEOS)
  coordinator->SetShouldCircumventDeviceCheckForTesting(true);
#endif

  EXPECT_FALSE(coordinator->BubbleExists());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(labs_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return coordinator->BubbleExists(); }));

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      coordinator->GetChromeLabsBubbleView()->GetWidget());
  test_api.NotifyClick(e);
  destroyed_waiter.Wait();
  EXPECT_FALSE(coordinator->BubbleExists());
}

IN_PROC_BROWSER_TEST_F(ChromeLabsButtonTest, ShouldButtonShowTest) {
  // There are experiments available so the button should not be nullptr.
  EXPECT_NE(GetChromeLabsButton(), nullptr);
  // Enterprise policy is initially set to true.
  EXPECT_TRUE(GetChromeLabsButton()->GetVisible());

  // Default enterprise policy value should show the Chrome Labs button.
  browser()->GetProfile()->GetPrefs()->ClearPref(
      chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy);
  EXPECT_TRUE(GetChromeLabsButton()->GetVisible());

  browser()->GetProfile()->GetPrefs()->SetBoolean(
      chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, false);
  EXPECT_FALSE(GetChromeLabsButton()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ChromeLabsButtonTest, ShouldButtonShowEphemerallyTest) {
  // Unpin the button so it is not visible by default.
  PinnedToolbarActionsModel::Get(browser()->GetProfile())
      ->UpdatePinnedState(kActionShowChromeLabs, false);

  EXPECT_EQ(GetChromeLabsButton(), nullptr);

  ChromeLabsCoordinator* coordinator = ChromeLabsCoordinator::From(browser());
  coordinator->Show();

  // Showing the bubble when the button was not previously showing should cause
  // it to show.
  EXPECT_TRUE(coordinator->BubbleExists());
  EXPECT_NE(GetChromeLabsButton(), nullptr);
  EXPECT_TRUE(GetChromeLabsButton()->GetVisible());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      coordinator->GetChromeLabsBubbleView()->GetWidget());
  coordinator->Hide();
  destroyed_waiter.Wait();

  // Hiding the bubble should cause the ephemeral button to hide.
  EXPECT_EQ(GetChromeLabsButton(), nullptr);
}

IN_PROC_BROWSER_TEST_F(ChromeLabsButtonTest, DotIndicatorTest) {
  views::Button* labs_button = GetChromeLabsButton();
  ChromeLabsCoordinator* coordinator = ChromeLabsCoordinator::From(browser());
#if BUILDFLAG(IS_CHROMEOS)
  coordinator->SetShouldCircumventDeviceCheckForTesting(true);
#endif
  coordinator->MaybeInstallDotIndicator();
  views::DotIndicator* dot_indicator = coordinator->GetDotIndicator();
  EXPECT_TRUE(dot_indicator->GetVisible());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(labs_button);
  test_api.NotifyClick(e);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !dot_indicator->GetVisible(); }));
  EXPECT_TRUE(coordinator->BubbleExists());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      coordinator->GetChromeLabsBubbleView()->GetWidget());
  test_api.NotifyClick(e);
  destroyed_waiter.Wait();
  EXPECT_FALSE(coordinator->BubbleExists());
}

#if BUILDFLAG(IS_CHROMEOS)

class ChromeLabsButtonTestSafeMode : public ChromeLabsButtonTest {
 public:
  ChromeLabsButtonTestSafeMode() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeLabsButtonTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kSafeMode);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeLabsButtonTestSafeMode, ButtonShouldNotShowTest) {
  views::Button* button = GetChromeLabsButton();
  EXPECT_TRUE(!button || !button->GetVisible());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class ChromeLabsButtonNoExperimentsAvailableTest : public InProcessBrowserTest {
 public:
  ChromeLabsButtonNoExperimentsAvailableTest()
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_({{kSecondTestFeatureId, "", "", 0,
                                  FEATURE_VALUE_TYPE(kTestFeature2)}}) {
    std::vector<LabInfo> test_feature_info = {
        {kSecondTestFeatureId, u"", u"", "", version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);
    ForceChromeLabsActivationForTesting();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsButtonNoExperimentsAvailableTest,
                       ButtonShouldNotShowTest) {
  views::Button* button = BrowserView::GetBrowserViewForBrowser(browser())
                              ->toolbar()
                              ->GetChromeLabsButton();
  EXPECT_TRUE(!button || !button->GetVisible());
}

class ChromeLabsButtonOnlyExpiredFeaturesAvailableTest
    : public InProcessBrowserTest {
 public:
  ChromeLabsButtonOnlyExpiredFeaturesAvailableTest()
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_({{kExpiredFlagTestFeatureId, "", "",
                                  flags_ui::FlagsState::GetCurrentPlatform(),
                                  FEATURE_VALUE_TYPE(kTestFeatureExpired)}}) {
    flags::testing::SetFlagExpiration(kExpiredFlagTestFeatureId, 0);
    std::vector<LabInfo> test_feature_info = {{kExpiredFlagTestFeatureId, u"",
                                               u"", "",
                                               version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);
    ForceChromeLabsActivationForTesting();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

IN_PROC_BROWSER_TEST_F(ChromeLabsButtonOnlyExpiredFeaturesAvailableTest,
                       ButtonShouldNotShowTest) {
  views::Button* button = BrowserView::GetBrowserViewForBrowser(browser())
                              ->toolbar()
                              ->GetChromeLabsButton();
  EXPECT_TRUE(!button || !button->GetVisible());
}

#endif  // !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)
