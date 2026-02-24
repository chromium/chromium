// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"

#include <map>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_features.h"
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

  void TearDown() override {
    manager_->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kAccept);
    BrowserWithTestWindowTest::TearDown();
  }

  void TestShouldShowInfoBarPrompt(
      std::optional<base::TimeDelta> last_declined_time_delta,
      std::optional<int> declined_count,
      bool expect_infobar_exists,
      bool use_framework_prefs = false) {
    const char* time_pref = use_framework_prefs
                                ? prefs::kDefaultBrowserLastDeclinedTime
                                : prefs::kDefaultBrowserInfobarLastDeclinedTime;
    const char* count_pref = use_framework_prefs
                                 ? prefs::kDefaultBrowserDeclinedCount
                                 : prefs::kDefaultBrowserInfobarDeclinedCount;

    if (last_declined_time_delta.has_value()) {
      local_state()->SetTime(
          time_pref, base::Time::Now() - last_declined_time_delta.value());
    } else {
      local_state()->ClearPref(time_pref);
    }
    if (declined_count.has_value()) {
      local_state()->SetInteger(count_pref, declined_count.value());
    } else {
      local_state()->ClearPref(count_pref);
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
          .WillOnce([&](infobars::InfoBar* infobar) { run_loop.Quit(); });
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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<DefaultBrowserPromptManager> manager_;

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

constexpr int kFrameworkMaxPromptCount = 5;
constexpr int kFrameworkRepromptDurationDays = 14;

TEST_F(DefaultBrowserPromptManagerTest, FrameworkInfoBarMaxPromptCount) {
  scoped_feature_list_.InitAndEnableFeature(
      default_browser::kDefaultBrowserPromptSurfaces);

  // Show if the declined count is less than the max prompt count.
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/kFrameworkMaxPromptCount - 1,
      /*expect_infobar_exists=*/true,
      /*use_framework_prefs=*/true);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/kFrameworkMaxPromptCount,
      /*expect_infobar_exists=*/false,
      /*use_framework_prefs=*/true);
}

TEST_F(DefaultBrowserPromptManagerTest, FrameworkInfoBarRepromptDuration) {
  scoped_feature_list_.InitAndEnableFeature(
      default_browser::kDefaultBrowserPromptSurfaces);

  // After the prompt is declined once, show the prompt again if the time since
  // the last time the prompt was declined is strictly longer than the base
  // reprompt duration.
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays),
      /*declined_count=*/1,
      /*expect_infobar_exists=*/false,
      /*use_framework_prefs=*/true);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/1,
      /*expect_infobar_exists=*/true,
      /*use_framework_prefs=*/true);

  // If the user has declined the prompt multiple times, the next reprompt
  // duration should be equal to the reprompt duration.

  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays),
      /*declined_count=*/2,
      /*expect_infobar_exists=*/false,
      /*use_framework_prefs=*/true);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/2,
      /*expect_infobar_exists=*/true,
      /*use_framework_prefs=*/true);

  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays),
      /*declined_count=*/3,
      /*expect_infobar_exists=*/false,
      /*use_framework_prefs=*/true);
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/3,
      /*expect_infobar_exists=*/true,
      /*use_framework_prefs=*/true);
}

TEST_F(DefaultBrowserPromptManagerTest, FrameworkPromptSurfaceBecomesInfoBar) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      default_browser::kDefaultBrowserPromptSurfaces,
      {{default_browser::kDefaultBrowserPromptSurfaceParam.name,
        "bubble_dialog"}});

  // When decline count is < 3, the surface should be bubble_dialog, so no
  // infobar is shown.
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/2,
      /*expect_infobar_exists=*/false,
      /*use_framework_prefs=*/true);

  // When decline count is >= 3, the surface should become an infobar.
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kFrameworkRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/3,
      /*expect_infobar_exists=*/true,
      /*use_framework_prefs=*/true);
}
