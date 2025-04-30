// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"

#include <map>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
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
class InfoBarManagerObserver : public infobars::InfoBarManager::Observer {
 public:
  MOCK_METHOD(void, OnInfoBarAdded, (infobars::InfoBar * infobar), (override));
};
}  // namespace

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

  void TestShouldShowInfoBarPrompt(
      std::optional<base::TimeDelta> last_declined_time_delta,
      std::optional<int> declined_count,
      bool expect_infobar_exists) {
    if (last_declined_time_delta.has_value()) {
      local_state()->SetTime(
          prefs::kDefaultBrowserLastDeclinedTime,
          base::Time::Now() - last_declined_time_delta.value());
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

    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(
            browser()->tab_strip_model()->GetWebContentsAt(0));
    infobar_observation_.Observe(infobar_manager);

    base::RunLoop run_loop;
    if (expect_infobar_exists) {
      EXPECT_CALL(infobar_manager_observer_, OnInfoBarAdded)
          .WillOnce(testing::Invoke(
              [&](infobars::InfoBar* infobar) { run_loop.Quit(); }));
    } else {
      EXPECT_CALL(infobar_manager_observer_, OnInfoBarAdded).Times(0);
    }

    manager()->MaybeShowPrompt();
    if (expect_infobar_exists) {
      // The info bar shows asynchronously, after checking if Chrome can be
      // pinned to the taskbar, so need to wait for it to be shown.
      run_loop.Run();
    }
    // The decision not to show the info bar is synchronous; no need to wait.
    infobar_observation_.Reset();
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

  DefaultBrowserPromptManager* manager() { return manager_; }

 private:
  raw_ptr<DefaultBrowserPromptManager> manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

  InfoBarManagerObserver infobar_manager_observer_;
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_observation_{&infobar_manager_observer_};
};

TEST_F(DefaultBrowserPromptManagerTest, ShowsAppMenuItem) {
  auto* manager = DefaultBrowserPromptManager::GetInstance();
  ASSERT_FALSE(manager->show_app_menu_item());

  manager->MaybeShowPrompt();
  ASSERT_TRUE(manager->show_app_menu_item());
}

TEST_F(DefaultBrowserPromptManagerTest, AppMenuItemHiddenOnPromptAccept) {
  auto* manager = DefaultBrowserPromptManager::GetInstance();
  manager->MaybeShowPrompt();
  ASSERT_TRUE(manager->show_app_menu_item());

  manager->CloseAllPrompts(DefaultBrowserPromptManager::CloseReason::kAccept);
  ASSERT_FALSE(manager->show_app_menu_item());
}

TEST_F(DefaultBrowserPromptManagerTest, AppMenuItemPersistsOnPromptDismissed) {
  auto* manager = DefaultBrowserPromptManager::GetInstance();
  manager->MaybeShowPrompt();
  ASSERT_TRUE(manager->show_app_menu_item());

  manager->CloseAllPrompts(DefaultBrowserPromptManager::CloseReason::kDismiss);
  ASSERT_TRUE(manager->show_app_menu_item());
}

constexpr int kMaxPromptCount = 5;
constexpr int kRepromptDurationDays = 21;

TEST_F(DefaultBrowserPromptManagerTest, InfoBarMaxPromptCount) {
  // Show if the declined count is less than the max prompt count.
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/kMaxPromptCount - 1,
      /*expect_infobar_exists=*/true);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/kMaxPromptCount,
      /*expect_infobar_exists=*/false);
}

TEST_F(DefaultBrowserPromptManagerTest, InfoBarRepromptDuration) {
  // After the prompt is declined once, show the prompt again if the time since
  // the last time the prompt was declined is strictly longer than the base
  // reprompt duration.
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kRepromptDurationDays),
      /*declined_count=*/1,
      /*expect_infobar_exists=*/false);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/1,
      /*expect_infobar_exists=*/true);

  // If the user has declined the prompt multiple times, the next reprompt
  // duration should be equal to the reprompt duration.

  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kRepromptDurationDays),
      /*declined_count=*/2,
      /*expect_infobar_exists=*/false);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/2,
      /*expect_infobar_exists=*/true);

  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kRepromptDurationDays),
      /*declined_count=*/3,
      /*expect_infobar_exists=*/false);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/3,
      /*expect_infobar_exists=*/true);
}
