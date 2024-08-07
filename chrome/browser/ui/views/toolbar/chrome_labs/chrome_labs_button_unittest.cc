// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_button.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/unexpire_flags.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/dot_indicator.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/common/pref_names.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/test/base/scoped_channel_override.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

const char kFirstTestFeatureId[] = "feature-1";
BASE_FEATURE(kTestFeature1, "FeatureName1", base::FEATURE_ENABLED_BY_DEFAULT);
const char kSecondTestFeatureId[] = "feature-2";
BASE_FEATURE(kTestFeature2, "FeatureName2", base::FEATURE_DISABLED_BY_DEFAULT);
const char kExpiredFlagTestFeatureId[] = "expired-feature";
BASE_FEATURE(kTestFeatureExpired, "Expired", base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

class ChromeLabsButtonTest : public TestWithBrowserView {
 public:
  ChromeLabsButtonTest()
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_({{kFirstTestFeatureId, "", "",
                                  flags_ui::FlagsState::GetCurrentPlatform(),
                                  FEATURE_VALUE_TYPE(kTestFeature1)}})

  {
  }
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChromeLabs,
        {{features::kChromeLabsActivationPercentage.name, "100"}});
    std::vector<LabInfo> test_feature_info = {
        {kFirstTestFeatureId, u"", u"", "", version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);

    TestWithBrowserView::SetUp();
    profile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

TEST_F(ChromeLabsButtonTest, ShowAndHideChromeLabsBubbleOnPress) {
  views::Button* labs_button = browser_view()->toolbar()->GetChromeLabsButton();
  ChromeLabsCoordinator* coordinator =
      browser_view()->browser()->GetFeatures().chrome_labs_coordinator();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::OwnerSettingsServiceAsh* service_ =
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(GetProfile());
  coordinator->SetShouldCircumventDeviceCheckForTesting(true);
#endif

  EXPECT_FALSE(coordinator->BubbleExists());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(labs_button);
  test_api.NotifyClick(e);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  service_->RunPendingIsOwnerCallbacksForTesting(/*is_owner=*/false);
#endif
  EXPECT_TRUE(coordinator->BubbleExists());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      coordinator->GetChromeLabsBubbleView()->GetWidget());
  test_api.NotifyClick(e);
  destroyed_waiter.Wait();
  EXPECT_FALSE(coordinator->BubbleExists());
}

TEST_F(ChromeLabsButtonTest, ShouldButtonShowTest) {
  // There are experiments available so the button should not be nullptr.
  EXPECT_NE(browser_view()->toolbar()->GetChromeLabsButton(), nullptr);
  // Enterprise policy is initially set to true.
  EXPECT_TRUE(browser_view()->toolbar()->GetChromeLabsButton()->GetVisible());

  // Default enterprise policy value should show the Chrome Labs button.
  profile()->GetPrefs()->ClearPref(
      chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy);
  EXPECT_TRUE(browser_view()->toolbar()->GetChromeLabsButton()->GetVisible());

  profile()->GetPrefs()->SetBoolean(
      chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, false);
  EXPECT_FALSE(browser_view()->toolbar()->GetChromeLabsButton()->GetVisible());
}

TEST_F(ChromeLabsButtonTest, DotIndicatorTest) {
  ChromeLabsButton* chrome_labs_button =
      browser_view()->toolbar()->chrome_labs_button();
  EXPECT_TRUE(chrome_labs_button->GetDotIndicatorVisibilityForTesting());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(chrome_labs_button);
  test_api.NotifyClick(e);
  EXPECT_FALSE(chrome_labs_button->GetDotIndicatorVisibilityForTesting());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class ChromeLabsButtonTestSafeMode : public ChromeLabsButtonTest {
 public:
  ChromeLabsButtonTestSafeMode() : ChromeLabsButtonTest() {}
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kSafeMode);
    ChromeLabsButtonTest::SetUp();
  }

  void TearDown() override {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        ash::switches::kSafeMode);
    ChromeLabsButtonTest::TearDown();
  }
};

TEST_F(ChromeLabsButtonTestSafeMode, ButtonShouldNotShowTest) {
  EXPECT_EQ(browser_view()->toolbar()->GetChromeLabsButton(), nullptr);
}

class ChromeLabsButtonTestSecondaryUser : public ChromeLabsButtonTest {
 public:
  ChromeLabsButtonTestSecondaryUser() : ChromeLabsButtonTest() {}

  void LogIn(const std::string& email) override {
    // Fake primary user log-in, so that the created profile will be interpreted
    // as secondary user's profile.
    ChromeLabsButtonTest::LogIn("primary-user@domain.com");
    ChromeLabsButtonTest::LogIn(email);
  }
};

TEST_F(ChromeLabsButtonTestSecondaryUser, ButtonShouldNotShowTest) {
  EXPECT_EQ(browser_view()->toolbar()->GetChromeLabsButton(), nullptr);
}

#endif

class ChromeLabsButtonNoExperimentsAvailableTest : public TestWithBrowserView {
 public:
  ChromeLabsButtonNoExperimentsAvailableTest()
      :
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        channel_override_(chrome::ScopedChannelOverride(
            chrome::ScopedChannelOverride::Channel::kDev)),
#endif
        scoped_feature_entries_({{kSecondTestFeatureId, "", "", 0,
                                  FEATURE_VALUE_TYPE(kTestFeature2)}}) {
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChromeLabs,
        {{features::kChromeLabsActivationPercentage.name, "100"}});

    std::vector<LabInfo> test_feature_info = {
        {kSecondTestFeatureId, u"", u"", "", version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);

    TestWithBrowserView::SetUp();
    profile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

TEST_F(ChromeLabsButtonNoExperimentsAvailableTest, ButtonShouldNotShowTest) {
  EXPECT_EQ(browser_view()->toolbar()->GetChromeLabsButton(), nullptr);
}

class ChromeLabsButtonOnlyExpiredFeaturesAvailableTest
    : public TestWithBrowserView {
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
  }
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChromeLabs,
        {{features::kChromeLabsActivationPercentage.name, "100"}});

    std::vector<LabInfo> test_feature_info = {{kExpiredFlagTestFeatureId, u"",
                                               u"", "",
                                               version_info::Channel::STABLE}};
    scoped_chrome_labs_model_data_.SetModelDataForTesting(test_feature_info);

    TestWithBrowserView::SetUp();
    profile()->GetPrefs()->SetBoolean(
        chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, true);
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  chrome::ScopedChannelOverride channel_override_;
#endif
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedChromeLabsModelDataForTesting scoped_chrome_labs_model_data_;
};

TEST_F(ChromeLabsButtonOnlyExpiredFeaturesAvailableTest,
       ButtonShouldNotShowTest) {
  EXPECT_EQ(browser_view()->toolbar()->GetChromeLabsButton(), nullptr);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(GOOGLE_CHROME_BRANDING)
