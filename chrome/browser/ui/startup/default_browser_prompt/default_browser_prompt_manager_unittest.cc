// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"

#include <map>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class DefaultBrowserPromptManagerObserver
    : public DefaultBrowserPromptManager::Observer {
public:
  MOCK_METHOD(void, OnShowAppMenuPromptChanged, (), (override));
};

class InfoBarManagerObserver : public infobars::InfoBarManager::Observer {
public:
  MOCK_METHOD(void, OnInfoBarAdded, (infobars::InfoBar * infobar), (override));
};
} // namespace

class DefaultBrowserPromptManagerTest : public BrowserWithTestWindowTest {
public:
  DefaultBrowserPromptManagerTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    manager_ = DefaultBrowserPromptManager::GetInstance();
    manager_->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kAccept);

    // Set up a single tab in the foreground.
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(
            profile(), content::SiteInstance::Create(profile()));
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
  }

  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }

  void EnableDefaultBrowserPromptRefreshFeatureWithParams(
      std::map<std::string, std::string> params) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kDefaultBrowserPromptRefresh, params);
  }

  void TestShouldShowInfoBarPrompt(
      std::optional<base::TimeDelta> last_declined_time_delta,
      std::optional<int> declined_count, bool expect_infobar_exists) {
    if (last_declined_time_delta.has_value()) {
      local_state()->SetTime(prefs::kDefaultBrowserLastDeclinedTime,
                             base::Time::Now() -
                                 last_declined_time_delta.value());
    } else {
      local_state()->ClearPref(prefs::kDefaultBrowserLastDeclinedTime);
    }
    if (declined_count.has_value()) {
      local_state()->SetInteger(prefs::kDefaultBrowserDeclinedCount,
                                declined_count.value());
    } else {
      local_state()->ClearPref(prefs::kDefaultBrowserDeclinedCount);
    }

    manager()->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kAccept);

    infobars::ContentInfoBarManager *infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(
            browser()->tab_strip_model()->GetWebContentsAt(0));
    infobar_observation_.Observe(infobar_manager);

    EXPECT_CALL(infobar_manager_observer_, OnInfoBarAdded)
        .Times(expect_infobar_exists ? 1 : 0);
    manager()->MaybeShowPrompt();

    infobar_observation_.Reset();
  }

  PrefService *local_state() { return g_browser_process->local_state(); }

  DefaultBrowserPromptManager *manager() { return manager_; }

private:
  raw_ptr<DefaultBrowserPromptManager> manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

  InfoBarManagerObserver infobar_manager_observer_;
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_observation_{&infobar_manager_observer_};
};

TEST_F(DefaultBrowserPromptManagerTest, NotifiesAppMenuObservers) {
  DefaultBrowserPromptManagerObserver prompt_manager_observer;
  base::ScopedObservation<DefaultBrowserPromptManager,
                          DefaultBrowserPromptManager::Observer>
      prompt_manager_observation{&prompt_manager_observer};
  prompt_manager_observation.Observe(
      DefaultBrowserPromptManager::GetInstance());

  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuChip.name, "true"}});

  ASSERT_FALSE(manager()->get_show_app_menu_prompt());

  EXPECT_CALL(prompt_manager_observer, OnShowAppMenuPromptChanged).Times(1);
  manager()->MaybeShowPrompt();
  ASSERT_TRUE(manager()->get_show_app_menu_prompt());

  // Does not notify observers a second time if the value is the same.
  EXPECT_CALL(prompt_manager_observer, OnShowAppMenuPromptChanged).Times(0);
  manager()->MaybeShowPrompt();
  ASSERT_TRUE(manager()->get_show_app_menu_prompt());

  prompt_manager_observation.Reset();
}

TEST_F(DefaultBrowserPromptManagerTest, ShowsAppMenuItemWithParamEnabled) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuItem.name, "true"}});

  auto *manager = DefaultBrowserPromptManager::GetInstance();
  ASSERT_FALSE(manager->get_show_app_menu_item());

  manager->MaybeShowPrompt();
  ASSERT_TRUE(manager->get_show_app_menu_item());
}

TEST_F(DefaultBrowserPromptManagerTest, HidesAppMenuItemWithParamDisabled) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuItem.name, "false"}});

  auto *manager = DefaultBrowserPromptManager::GetInstance();
  ASSERT_FALSE(manager->get_show_app_menu_item());

  manager->MaybeShowPrompt();
  ASSERT_FALSE(manager->get_show_app_menu_item());
}

TEST_F(DefaultBrowserPromptManagerTest, AppMenuItemHiddenOnPromptAccept) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuItem.name, "true"}});

  auto *manager = DefaultBrowserPromptManager::GetInstance();
  manager->MaybeShowPrompt();
  ASSERT_TRUE(manager->get_show_app_menu_item());

  manager->CloseAllPrompts(DefaultBrowserPromptManager::CloseReason::kAccept);
  ASSERT_FALSE(manager->get_show_app_menu_item());
}

TEST_F(DefaultBrowserPromptManagerTest, AppMenuItemPersistsOnPromptDismissed) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuItem.name, "true"}});

  auto *manager = DefaultBrowserPromptManager::GetInstance();
  manager->MaybeShowPrompt();
  ASSERT_TRUE(manager->get_show_app_menu_item());

  manager->CloseAllPrompts(DefaultBrowserPromptManager::CloseReason::kDismiss);
  ASSERT_TRUE(manager->get_show_app_menu_item());
}

TEST_F(DefaultBrowserPromptManagerTest, InfoBarMaxPromptCount) {
  // If max prompt count is negative, do not limit the number of times the
  // prompt is shown.
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "-1"},
       {features::kRepromptDurationMultiplier.name, "1"}});
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/12345,
      /*expected=*/true);

  // Never show the prompt if max prompt count is zero.
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "0"},
       {features::kRepromptDurationMultiplier.name, "2"}});
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/std::nullopt,
      /*declined_count=*/std::nullopt,
      /*expected=*/false);

  // If max prompt count is 1, only show the prompt if declined count is unset.
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "1"},
       {features::kRepromptDurationMultiplier.name, "1"}});
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/std::nullopt,
      /*declined_count=*/std::nullopt,
      /*expected=*/true);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/1,
      /*expected=*/false);

  // Show if the declined count is less than the max prompt count.
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "5"},
       {features::kRepromptDurationMultiplier.name, "1"}});
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/4,
      /*expected=*/true);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/5,
      /*expected=*/false);
}

TEST_F(DefaultBrowserPromptManagerTest, InfoBarRepromptDuration) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "-1"},
       {features::kRepromptDurationMultiplier.name, "2"}});

  // After the prompt is declined once, show the prompt again if the time since
  // the last time the prompt was declined is strictly longer than the base
  // reprompt duration.
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(1),
      /*declined_count=*/1,
      /*expected=*/false);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(1) + base::Microseconds(1),
      /*declined_count=*/1,
      /*expected=*/true);

  // If the user has declined the prompt multiple times, the next reprompt
  // duration should be multiplied by the reprompt multiplier for each
  // additional time the prompt has been declined.
  // So the prompt should be shown if the last declined time is older than:
  // base reprompt duration *
  //     (reprompt duration multiplier ^ (declined count - 1))

  // For example, after the prompt has been declined a second time, only show
  // the prompt (1 day) * (2^1) = 2 days after it was last declined.
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(2),
      /*declined_count=*/2,
      /*expected=*/false);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(2) + base::Microseconds(1),
      /*declined_count=*/2,
      /*expected=*/true);

  // After the prompt has been declined a third time, only show the prompt
  // (1 day) * (2^2) = 4 days after it was last declined.
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(4),
      /*declined_count=*/3,
      /*expected=*/false);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(4) + base::Microseconds(1),
      /*declined_count=*/3,
      /*expected=*/true);
}

TEST_F(DefaultBrowserPromptManagerTest, PromptHiddenWhenFeatureParamDisabled) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kRepromptDuration.name, "1d"},
       {features::kMaxPromptCount.name, "-1"},
       {features::kRepromptDurationMultiplier.name, "1"},
       {features::kShowDefaultBrowserInfoBar.name, "false"}});

  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/std::nullopt,
      /*declined_count=*/std::nullopt,
      /*expected=*/false);
}

TEST_F(DefaultBrowserPromptManagerTest, AppMenuFeatureParamFalse) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams({});
  ASSERT_FALSE(features::kShowDefaultBrowserAppMenuChip.Get());
  chrome::startup::default_prompt::MaybeResetAppMenuPromptPrefs(profile());
  manager()->MaybeShowPrompt();
  EXPECT_FALSE(manager()->get_show_app_menu_prompt());
}

TEST_F(DefaultBrowserPromptManagerTest, ShowAppMenuFirstTime) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuChip.name, "true"},
       {features::kDefaultBrowserAppMenuDuration.name, "1d"}});
  ASSERT_TRUE(local_state()
                  ->FindPreference(prefs::kDefaultBrowserFirstShownTime)
                  ->IsDefaultValue());
  chrome::startup::default_prompt::MaybeResetAppMenuPromptPrefs(profile());
  manager()->MaybeShowPrompt();
  EXPECT_TRUE(manager()->get_show_app_menu_prompt());
  EXPECT_EQ(base::Time::Now(),
            local_state()->GetTime(prefs::kDefaultBrowserFirstShownTime));

  task_environment()->FastForwardBy(base::Days(1) - base::Microseconds(1));
  EXPECT_TRUE(manager()->get_show_app_menu_prompt());

  task_environment()->FastForwardBy(base::Microseconds(1));
  EXPECT_FALSE(manager()->get_show_app_menu_prompt());
  EXPECT_TRUE(local_state()
                  ->FindPreference(prefs::kDefaultBrowserFirstShownTime)
                  ->IsDefaultValue());
  EXPECT_EQ(base::Time::Now(),
            local_state()->GetTime(prefs::kDefaultBrowserLastDeclinedTime));
  EXPECT_EQ(1, local_state()->GetInteger(prefs::kDefaultBrowserDeclinedCount));
}

TEST_F(DefaultBrowserPromptManagerTest, DoNotShowIfPromptsShouldNotBeReshown) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuChip.name, "true"},
       {features::kMaxPromptCount.name, "1"}});
  local_state()->ClearPref(prefs::kDefaultBrowserFirstShownTime);
  local_state()->SetTime(prefs::kDefaultBrowserLastDeclinedTime,
                         base::Time::Now());
  local_state()->SetInteger(prefs::kDefaultBrowserDeclinedCount, 1);
  chrome::startup::default_prompt::MaybeResetAppMenuPromptPrefs(profile());
  manager()->MaybeShowPrompt();
  EXPECT_FALSE(manager()->get_show_app_menu_prompt());
}

TEST_F(DefaultBrowserPromptManagerTest, KeepShowingIfFirstShownTimeIsRecent) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuChip.name, "true"},
       {features::kDefaultBrowserAppMenuDuration.name, "2d"}});
  local_state()->SetTime(prefs::kDefaultBrowserFirstShownTime,
                         base::Time::Now() - base::Days(1));
  local_state()->ClearPref(prefs::kDefaultBrowserLastDeclinedTime);
  local_state()->ClearPref(prefs::kDefaultBrowserDeclinedCount);
  chrome::startup::default_prompt::MaybeResetAppMenuPromptPrefs(profile());
  manager()->MaybeShowPrompt();
  EXPECT_TRUE(manager()->get_show_app_menu_prompt());

  task_environment()->FastForwardBy(base::Days(1) - base::Microseconds(1));
  EXPECT_TRUE(manager()->get_show_app_menu_prompt());

  task_environment()->FastForwardBy(base::Microseconds(1));
  EXPECT_FALSE(manager()->get_show_app_menu_prompt());
  EXPECT_TRUE(local_state()
                  ->FindPreference(prefs::kDefaultBrowserFirstShownTime)
                  ->IsDefaultValue());
  EXPECT_EQ(base::Time::Now(),
            local_state()->GetTime(prefs::kDefaultBrowserLastDeclinedTime));
  EXPECT_EQ(1, local_state()->GetInteger(prefs::kDefaultBrowserDeclinedCount));
}

TEST_F(DefaultBrowserPromptManagerTest, StopShowingIfFirstShownTimeTooOld) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuChip.name, "true"},
       {features::kDefaultBrowserAppMenuDuration.name, "1s"},
       {features::kRepromptDuration.name, "1d"}});
  local_state()->SetTime(prefs::kDefaultBrowserFirstShownTime,
                         base::Time::Now() - base::Seconds(1));
  local_state()->SetTime(prefs::kDefaultBrowserLastDeclinedTime,
                         base::Time::Now() - base::Days(1) -
                             base::Microseconds(1));
  local_state()->SetInteger(prefs::kDefaultBrowserDeclinedCount, 1);
  chrome::startup::default_prompt::MaybeResetAppMenuPromptPrefs(profile());
  manager()->MaybeShowPrompt();
  EXPECT_FALSE(manager()->get_show_app_menu_prompt());
  EXPECT_TRUE(local_state()
                  ->FindPreference(prefs::kDefaultBrowserFirstShownTime)
                  ->IsDefaultValue());
  EXPECT_EQ(base::Time::Now(),
            local_state()->GetTime(prefs::kDefaultBrowserLastDeclinedTime));
  EXPECT_EQ(2, local_state()->GetInteger(prefs::kDefaultBrowserDeclinedCount));
}

// This is a regression test for a crash that occurred when the default prompt
// timer expired after a browser was closed.
TEST_F(DefaultBrowserPromptManagerTest, DoesNotWritePrefWhenBrowserIsClosed) {
  EnableDefaultBrowserPromptRefreshFeatureWithParams(
      {{features::kShowDefaultBrowserAppMenuChip.name, "true"},
       {features::kDefaultBrowserAppMenuDuration.name, "1d"}});
  chrome::startup::default_prompt::MaybeResetAppMenuPromptPrefs(profile());
  manager()->MaybeShowPrompt();
  EXPECT_TRUE(manager()->get_show_app_menu_prompt());
  EXPECT_EQ(0, local_state()->GetInteger(prefs::kDefaultBrowserDeclinedCount));

  // Close browser then trigger timer
  BrowserList::RemoveBrowser(BrowserList::GetInstance()->GetLastActive());
  task_environment()->FastForwardBy(base::Days(1));

  EXPECT_FALSE(manager()->get_show_app_menu_prompt());
  EXPECT_EQ(0, local_state()->GetInteger(prefs::kDefaultBrowserDeclinedCount));
}
