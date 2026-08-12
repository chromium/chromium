// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class DefaultBrowserPromptManagerTest : public testing::Test {
 public:
  DefaultBrowserPromptManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    manager_ = DefaultBrowserPromptManager::GetInstance();
    manager_->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kAccept);
  }

  void TearDown() override {
    manager_->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kAccept);
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

    bool prompt_shown = manager()->MaybeShowPrompt();
    if (prompt_shown) {
      ASSERT_TRUE(base::test::RunUntil([this]() {
        return manager()->GetPromptSurfaceManager() != nullptr;
      }));
    }

    if (expect_infobar_exists) {
      EXPECT_TRUE(prompt_shown);
      ASSERT_NE(manager()->GetPromptSurfaceManager(), nullptr);
      EXPECT_EQ(manager()->GetPromptSurfaceManager()->GetEntrypointType(),
                default_browser::DefaultBrowserEntrypointType::kStartupInfobar);
    } else {
      if (!prompt_shown) {
        EXPECT_EQ(manager()->GetPromptSurfaceManager(), nullptr);
      } else {
        // Prompt was shown, but using a non-infobar surface (e.g. bubble
        // dialog).
        ASSERT_NE(manager()->GetPromptSurfaceManager(), nullptr);
        EXPECT_NE(
            manager()->GetPromptSurfaceManager()->GetEntrypointType(),
            default_browser::DefaultBrowserEntrypointType::kStartupInfobar);
      }
    }
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

  DefaultBrowserPromptManager* manager() { return manager_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<DefaultBrowserPromptManager> manager_ = nullptr;
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

#if BUILDFLAG(IS_WIN)
constexpr int kFrameworkMaxPromptCount = 5;
constexpr int kFrameworkRepromptDurationDays = 14;

TEST_F(DefaultBrowserPromptManagerTest, FrameworkInfoBarMaxPromptCount) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{default_browser::kDefaultBrowserPromptSurfaces},
      /*disabled_features=*/{features::kSeparateDefaultAndPinPrompt});

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
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{default_browser::kDefaultBrowserPromptSurfaces},
      /*disabled_features=*/{features::kSeparateDefaultAndPinPrompt});

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
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{default_browser::kDefaultBrowserPromptSurfaces,
                             {{default_browser::
                                   kDefaultBrowserPromptSurfaceParam.name,
                               "bubble_dialog"}}}},
      /*disabled_features=*/{features::kSeparateDefaultAndPinPrompt});

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
#else
TEST_F(DefaultBrowserPromptManagerTest, PromptSurfacesIgnoredOnNonWin) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      default_browser::kDefaultBrowserPromptSurfaces,
      {{default_browser::kDefaultBrowserPromptSurfaceParam.name,
        "bubble_dialog"}});

  // Since PromptSurfaces is ignored on non-Windows platforms, it should still
  // behave like the standard infobar prompt (using infobar prefs and 21-day
  // reprompt duration).
  TestShouldShowInfoBarPrompt(
      /*last_declined_time_delta=*/base::Days(kRepromptDurationDays) +
          base::Microseconds(1),
      /*declined_count=*/kMaxPromptCount - 1,
      /*expect_infobar_exists=*/true,
      /*use_framework_prefs=*/false);
}
#endif
