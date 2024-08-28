// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_pref_guardrails.h"

#include <memory>
#include <string_view>

#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
const webapps::AppId app_id = "app_id";
}

class WebAppGuardrailsTest : public testing::Test {
 public:
  WebAppGuardrailsTest() {
    WebAppPrefGuardrails::RegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 protected:
  std::optional<int> GetIntWebAppPref(const webapps::AppId& app,
                                      std::string_view path) {
    return ::web_app::GetIntWebAppPref(prefs(), app, path);
  }

  std::optional<base::Time> GetTimeWebAppPref(const webapps::AppId& app,
                                              std::string_view path) {
    return ::web_app::GetTimeWebAppPref(prefs(), app, path);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

class WebAppGuardrailsIphTest : public WebAppGuardrailsTest {
 public:
  WebAppGuardrailsIphTest()
      : guardrails_(WebAppPrefGuardrails::GetForDesktopInstallIph(prefs())) {}

 protected:
  WebAppPrefGuardrails& guardrails() { return guardrails_; }
  bool ShouldShowIph(const webapps::AppId& app) {
    return !guardrails().IsBlockedByGuardrails(app);
  }

  const webapps::AppId app_id_2 = "test_app_2";
  const base::Time time_before_app_mute =
      base::Time::Now() -
      base::Days(kIphGuardrails.app_specific_mute_after_ignore_days.value()) -
      base::Hours(1);
  const base::Time time_before_global_mute =
      base::Time::Now() -
      base::Days(kIphGuardrails.global_mute_after_ignore_days.value()) -
      base::Hours(1);

 private:
  WebAppPrefGuardrails guardrails_;
};

TEST_F(WebAppGuardrailsIphTest, Ignore) {
  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(app_id, kIphPrefNames.not_accepted_count_name)
                .value_or(0),
            1);
  auto last_ignore_time =
      GetTimeWebAppPref(app_id, kIphPrefNames.last_ignore_time_name);
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphPrefNames.not_accepted_count_name).value_or(0),
              1);
    EXPECT_EQ(base::ValueToTime(dict.Find(kIphPrefNames.last_ignore_time_name)),
              last_ignore_time.value());
  }
}

TEST_F(WebAppGuardrailsIphTest, IgnoreRecordUpdated) {
  guardrails().RecordIgnore(app_id, base::Time::Now());
  auto last_ignore_time =
      GetTimeWebAppPref(app_id, kIphPrefNames.last_ignore_time_name);
  EXPECT_TRUE(last_ignore_time.has_value());

  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(app_id, kIphPrefNames.not_accepted_count_name)
                .value_or(0),
            2);
  EXPECT_NE(
      GetTimeWebAppPref(app_id, kIphPrefNames.last_ignore_time_name).value(),
      last_ignore_time.value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphPrefNames.not_accepted_count_name).value_or(0),
              2);
    EXPECT_NE(base::ValueToTime(dict.Find(kIphPrefNames.last_ignore_time_name)),
              last_ignore_time.value());
  }
}

TEST_F(WebAppGuardrailsIphTest, InstallResetCounters) {
  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(app_id, kIphPrefNames.not_accepted_count_name)
                .value_or(0),
            1);
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphPrefNames.not_accepted_count_name).value_or(0),
              1);
  }
  guardrails().RecordAccept(app_id);
  EXPECT_EQ(GetIntWebAppPref(app_id, kIphPrefNames.not_accepted_count_name)
                .value_or(0),
            0);
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphPrefNames.not_accepted_count_name).value_or(0),
              0);
  }
}

TEST_F(WebAppGuardrailsIphTest, AppIgnoredRecently) {
  EXPECT_TRUE(ShouldShowIph(app_id));
  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_FALSE(ShouldShowIph(app_id));
}

TEST_F(WebAppGuardrailsIphTest, GlobalIgnoredRecently) {
  EXPECT_TRUE(ShouldShowIph(app_id));
  guardrails().RecordIgnore(app_id_2, base::Time::Now());
  EXPECT_FALSE(ShouldShowIph(app_id));
}

TEST_F(WebAppGuardrailsIphTest, GlobalIgnoredPassedMuteTime) {
  guardrails().RecordIgnore(app_id_2, time_before_global_mute);
  EXPECT_TRUE(ShouldShowIph(app_id));
}

TEST_F(WebAppGuardrailsIphTest, TestIphAppIgnoredPassedMuteTime) {
  guardrails().RecordIgnore(app_id, time_before_app_mute);
  EXPECT_TRUE(ShouldShowIph(app_id));
}

TEST_F(WebAppGuardrailsIphTest, TestIphConsecutiveAppIgnore) {
  guardrails().RecordIgnore(app_id, time_before_app_mute);
  guardrails().RecordIgnore(app_id, base::Time::Now());
  guardrails().RecordIgnore(app_id, base::Time::Now());
  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_FALSE(ShouldShowIph(app_id));
}

TEST_F(WebAppGuardrailsIphTest, TestGlobalConsecutiveAppIgnore) {
  guardrails().RecordIgnore(app_id_2, base::Time::Now());
  guardrails().RecordIgnore(app_id, base::Time::Now());
  guardrails().RecordIgnore(app_id_2, base::Time::Now());
  guardrails().RecordIgnore(app_id, base::Time::Now());
  guardrails().RecordIgnore(app_id_2, base::Time::Now());
  EXPECT_FALSE(ShouldShowIph(app_id));
}

class WebAppGuardrailsMLTest : public WebAppGuardrailsTest {
 public:
  WebAppGuardrailsMLTest()
      : guardrails_(WebAppPrefGuardrails::GetForMlInstallPrompt(prefs())) {}

 protected:
  WebAppPrefGuardrails& guardrails() { return guardrails_; }
  bool IsMLBlockedByGuardrails(const webapps::AppId& app) {
    return guardrails().IsBlockedByGuardrails(app);
  }

 private:
  WebAppPrefGuardrails guardrails_;
};

TEST_F(WebAppGuardrailsMLTest, MLInstallIgnored) {
  EXPECT_FALSE(
      GetTimeWebAppPref(app_id, kMlPromoPrefNames.last_ignore_time_name)
          .has_value());
  EXPECT_FALSE(
      GetIntWebAppPref(app_id, kMlPromoPrefNames.not_accepted_count_name)
          .has_value());

  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(app_id, kMlPromoPrefNames.not_accepted_count_name)
                .value_or(0),
            1);
  auto last_ignore_time =
      GetTimeWebAppPref(app_id, kMlPromoPrefNames.last_ignore_time_name);
  EXPECT_TRUE(last_ignore_time.has_value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        dict.FindInt(kMlPromoPrefNames.not_accepted_count_name).value_or(0), 1);
    EXPECT_EQ(
        base::ValueToTime(dict.Find(kMlPromoPrefNames.last_ignore_time_name)),
        last_ignore_time.value());
  }
}

TEST_F(WebAppGuardrailsMLTest, MLInstallDismissed) {
  EXPECT_FALSE(
      GetTimeWebAppPref(app_id, kMlPromoPrefNames.last_dismiss_time_name)
          .has_value());
  EXPECT_FALSE(
      GetIntWebAppPref(app_id, kMlPromoPrefNames.not_accepted_count_name)
          .has_value());

  guardrails().RecordDismiss(app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(app_id, kMlPromoPrefNames.not_accepted_count_name)
                .value_or(0),
            1);
  auto last_dismissed_time =
      GetTimeWebAppPref(app_id, kMlPromoPrefNames.last_dismiss_time_name);
  EXPECT_TRUE(last_dismissed_time.has_value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        dict.FindInt(kMlPromoPrefNames.not_accepted_count_name).value_or(0), 1);
    EXPECT_EQ(
        base::ValueToTime(dict.Find(kMlPromoPrefNames.last_dismiss_time_name)),
        last_dismissed_time.value());
  }
}

TEST_F(WebAppGuardrailsMLTest, MLAcceptResetsCounters) {
  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(app_id, kMlPromoPrefNames.not_accepted_count_name)
                .value_or(0),
            1);
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        dict.FindInt(kMlPromoPrefNames.not_accepted_count_name).value_or(0), 1);
  }
  guardrails().RecordAccept(app_id);
  EXPECT_EQ(GetIntWebAppPref(app_id, kMlPromoPrefNames.not_accepted_count_name)
                .value_or(0),
            0);
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        dict.FindInt(kMlPromoPrefNames.not_accepted_count_name).value_or(0), 0);
  }
}

TEST_F(WebAppGuardrailsMLTest, MLGuardrailConsecutiveAppSpecificIgnores) {
  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(app_id, kMlPromoPrefNames.not_accepted_count_name)
                .value_or(0),
            1);
  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(app_id, kMlPromoPrefNames.not_accepted_count_name)
                .value_or(0),
            2);
  guardrails().RecordIgnore(app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(app_id, kMlPromoPrefNames.not_accepted_count_name)
                .value_or(0),
            3);
  EXPECT_TRUE(IsMLBlockedByGuardrails(app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMlPromoPrefNames.block_reason_name),
              "app_specific_not_accept_count_exceeded:app_id");
  }
}

TEST_F(WebAppGuardrailsMLTest, MLGuardrailAppSpecificIgnoreForDays) {
  base::Time now_time = base::Time::Now();
  guardrails().RecordIgnore(app_id, now_time);
  auto ignore_time =
      GetTimeWebAppPref(app_id, kMlPromoPrefNames.last_ignore_time_name);
  EXPECT_TRUE(ignore_time.has_value());
  EXPECT_EQ(now_time, ignore_time);

  base::Time forwarded_time = base::Time::Now() + base::Days(1);
  guardrails().RecordIgnore(app_id, forwarded_time);
  auto ignore_time_new =
      GetTimeWebAppPref(app_id, kMlPromoPrefNames.last_ignore_time_name);
  EXPECT_TRUE(ignore_time_new.has_value());
  EXPECT_EQ(forwarded_time, ignore_time_new);
  EXPECT_TRUE(IsMLBlockedByGuardrails(app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMlPromoPrefNames.block_reason_name),
              "app_specific_ignore_days_hit:app_id");
  }
}

TEST_F(WebAppGuardrailsMLTest, MLGuardrailAppSpecificDismissForDays) {
  base::Time now_time = base::Time::Now();
  guardrails().RecordDismiss(app_id, now_time);
  auto dismiss_time =
      GetTimeWebAppPref(app_id, kMlPromoPrefNames.last_dismiss_time_name);
  EXPECT_TRUE(dismiss_time.has_value());
  EXPECT_EQ(now_time, dismiss_time);

  // Dismissing the same app within 14 days should trigger the guardrail
  // response.
  int randDays = base::RandInt(1, 13);
  base::Time forwarded_time = base::Time::Now() + base::Days(randDays);
  guardrails().RecordDismiss(app_id, forwarded_time);
  auto dismiss_time_new =
      GetTimeWebAppPref(app_id, kMlPromoPrefNames.last_dismiss_time_name);
  EXPECT_TRUE(dismiss_time_new.has_value());
  EXPECT_EQ(forwarded_time, dismiss_time_new);
  EXPECT_TRUE(IsMLBlockedByGuardrails(app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMlPromoPrefNames.block_reason_name),
              "app_specific_dismiss_days_hit:app_id");
  }
}

TEST_F(WebAppGuardrailsMLTest, MLGuardrailConsecutiveAppAgnosticIgnores) {
  const webapps::AppId& app_id1 = "app1";
  const webapps::AppId& app_id2 = "app2";
  const webapps::AppId& app_id3 = "app3";
  const webapps::AppId& app_id4 = "app4";
  const webapps::AppId& app_id5 = "app5";
  guardrails().RecordIgnore(app_id1, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        dict.FindInt(kMlPromoPrefNames.not_accepted_count_name).value_or(0), 1);
  }
  guardrails().RecordDismiss(app_id2, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        dict.FindInt(kMlPromoPrefNames.not_accepted_count_name).value_or(0), 2);
  }
  guardrails().RecordDismiss(app_id3, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        dict.FindInt(kMlPromoPrefNames.not_accepted_count_name).value_or(0), 3);
  }
  guardrails().RecordDismiss(app_id4, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        dict.FindInt(kMlPromoPrefNames.not_accepted_count_name).value_or(0), 4);
  }
  guardrails().RecordIgnore(app_id5, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        dict.FindInt(kMlPromoPrefNames.not_accepted_count_name).value_or(0), 5);
  }
  EXPECT_TRUE(IsMLBlockedByGuardrails(app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMlPromoPrefNames.block_reason_name),
              "global_not_accept_count_exceeded");
  }
}

TEST_F(WebAppGuardrailsMLTest, MLGuardrailConsecutiveAppAgnosticIgnoreDays) {
  const webapps::AppId& app_id1 = "app1";
  guardrails().RecordIgnore(app_id1, base::Time::Now());
  auto last_ignored_time =
      GetTimeWebAppPref(app_id1, kMlPromoPrefNames.last_ignore_time_name);
  EXPECT_TRUE(last_ignored_time.has_value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        base::ValueToTime(dict.Find(kMlPromoPrefNames.last_ignore_time_name)),
        last_ignored_time.value());
  }
  EXPECT_TRUE(IsMLBlockedByGuardrails(app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMlPromoPrefNames.block_reason_name),
              "global_ignore_days_hit");
  }
}

TEST_F(WebAppGuardrailsMLTest, MLGuardrailConsecutiveAppAgnosticDismissDays) {
  const webapps::AppId& app_id1 = "app1";

  // Dismissing any app within the last 7 days should trigger the app agnostic
  // dismiss guardrail response.
  int randDays = base::RandInt(0, 6);
  guardrails().RecordDismiss(app_id1, base::Time::Now() - base::Days(randDays));
  auto last_dismissed_time =
      GetTimeWebAppPref(app_id1, kMlPromoPrefNames.last_dismiss_time_name);
  EXPECT_TRUE(last_dismissed_time.has_value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(
        base::ValueToTime(dict.Find(kMlPromoPrefNames.last_dismiss_time_name)),
        last_dismissed_time.value());
  }
  EXPECT_TRUE(IsMLBlockedByGuardrails(app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMlPromoPrefNames.block_reason_name),
              "global_dismiss_days_hit");
  }
}

// TODO(b/308774918): Consider using ScopedTimeClockOverrides instead of
// moving time forward.
class WebAppPrefsMLGuardrailsMaxStorageTest : public WebAppTest {
 public:
  WebAppPrefsMLGuardrailsMaxStorageTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    WebAppPrefGuardrails::RegisterProfilePrefs(prefs_.registry());
    base::FieldTrialParams params;
    params["max_days_to_store_guardrails"] = "2";
    feature_list_.InitAndEnableFeatureWithParameters(
        webapps::features::kWebAppsEnableMLModelForPromotion,
        std::move(params));
  }

  void SetUp() override { WebAppTest::SetUp(); }

  bool IsMLPromoBlockedTimeSet() {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    return dict.contains(kMlPromoPrefNames.all_blocked_time_name);
  }

  std::optional<base::Time> GetMLPromoBlockedTime() {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    auto* value =
        dict.FindByDottedPath(kMlPromoPrefNames.all_blocked_time_name);
    EXPECT_NE(value, nullptr) << " kAllMLPromosBlockedTime not set.";
    return base::ValueToTime(value);
  }

  void FastForwardTimeForMaxDaysToStoreGuardrails() {
    task_environment()->FastForwardBy(base::Days(
        webapps::features::kMaxDaysForMLPromotionGuardrailStorage.Get()));
  }

  bool IsMLBlockedByGuardrails(const webapps::AppId& app) {
    return guardrails().IsBlockedByGuardrails(app);
  }

  // Mimic a user blocked by guardrails for continuous 5 dismissals or ignores.
  void ForceMLPromoAgnosticGuardrailsBlocked() {
    const webapps::AppId& app_id1 = "app1";
    const webapps::AppId& app_id2 = "app2";
    const webapps::AppId& app_id3 = "app3";
    const webapps::AppId& app_id4 = "app4";
    const webapps::AppId& app_id5 = "app5";
    guardrails().RecordIgnore(app_id1, base::Time::Now());
    task_environment()->FastForwardBy(base::Milliseconds(1));
    guardrails().RecordDismiss(app_id2, base::Time::Now());
    task_environment()->FastForwardBy(base::Milliseconds(1));
    guardrails().RecordIgnore(app_id3, base::Time::Now());
    task_environment()->FastForwardBy(base::Milliseconds(1));
    guardrails().RecordDismiss(app_id4, base::Time::Now());
    task_environment()->FastForwardBy(base::Milliseconds(1));
    guardrails().RecordIgnore(app_id5, base::Time::Now());
    task_environment()->FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(IsMLBlockedByGuardrails("app_id"));
    task_environment()->FastForwardBy(base::Milliseconds(1));
  }

 protected:
  WebAppPrefGuardrails guardrails() {
    return WebAppPrefGuardrails::GetForMlInstallPrompt(prefs());
  }
  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(WebAppPrefsMLGuardrailsMaxStorageTest,
       BasicBehaviorGuardrailBlockedAfter5NonAccepts) {
  EXPECT_FALSE(IsMLPromoBlockedTimeSet());

  ForceMLPromoAgnosticGuardrailsBlocked();
  EXPECT_TRUE(IsMLPromoBlockedTimeSet());
  EXPECT_TRUE(GetMLPromoBlockedTime().has_value());
}

TEST_F(WebAppPrefsMLGuardrailsMaxStorageTest,
       MLBlockedPrefClearedOnInstallAccept) {
  EXPECT_FALSE(IsMLPromoBlockedTimeSet());

  ForceMLPromoAgnosticGuardrailsBlocked();
  EXPECT_TRUE(IsMLPromoBlockedTimeSet());

  guardrails().RecordAccept("app");
  EXPECT_FALSE(IsMLPromoBlockedTimeSet());
}

TEST_F(WebAppPrefsMLGuardrailsMaxStorageTest,
       MoreThan5NonAcceptsDoesNotUpdateBlockTime) {
  ForceMLPromoAgnosticGuardrailsBlocked();

  // Triggering a non-acceptance of the dialog after already not accepting 5
  // times should not update the time the blocked pref was set.
  const base::Time time_ml_install_dismissed_again = base::Time::Now();
  guardrails().RecordDismiss("app", time_ml_install_dismissed_again);
  EXPECT_TRUE(IsMLPromoBlockedTimeSet());

  std::optional<base::Time> ml_promo_time_blocked_from_pref =
      GetMLPromoBlockedTime();
  EXPECT_TRUE(ml_promo_time_blocked_from_pref.has_value());
  EXPECT_NE(ml_promo_time_blocked_from_pref, time_ml_install_dismissed_again);
}

TEST_F(WebAppPrefsMLGuardrailsMaxStorageTest, ClearAndResetGuardrails) {
  ForceMLPromoAgnosticGuardrailsBlocked();
  // This is important so that the global guardrail dismisses are not hit, and
  // tests can verify a clean guardrail reset, i.e. once reset, an app is
  // fully unblocked.
  task_environment()->FastForwardBy(
      base::Days(*kMlPromoGuardrails.global_mute_after_dismiss_days));
  EXPECT_TRUE(IsMLPromoBlockedTimeSet());

  FastForwardTimeForMaxDaysToStoreGuardrails();

  EXPECT_FALSE(IsMLBlockedByGuardrails("app"));
  EXPECT_FALSE(IsMLPromoBlockedTimeSet());

  const base::Value::Dict& dict =
      prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
  std::optional<int> agnostic_not_installed_count =
      dict.FindInt(kMlPromoPrefNames.not_accepted_count_name);
  EXPECT_TRUE(agnostic_not_installed_count.has_value());
  EXPECT_EQ(agnostic_not_installed_count, 0);
}

#if !BUILDFLAG(IS_CHROMEOS)
class WebAppPrefsLinkCapturingIPHGuardrailsTest : public WebAppTest {
 public:
  WebAppPrefsLinkCapturingIPHGuardrailsTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    WebAppPrefGuardrails::RegisterProfilePrefs(prefs_.registry());
    base::FieldTrialParams params;
    params["link_capturing_guardrail_storage_duration"] = "2";
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing, std::move(params));
  }

  void SetUp() override { WebAppTest::SetUp(); }

  bool IsDesktopIphBlockedTimeSet() {
    const auto& dict =
        prefs()->GetDict(prefs::kWebAppsAppAgnosticIPHLinkCapturingState);
    return dict.contains(
        kIPHNavigationCapturingPrefNames.all_blocked_time_name);
  }

  std::optional<base::Time> GetIphBlockedTime() {
    const auto& dict =
        prefs()->GetDict(prefs::kWebAppsAppAgnosticIPHLinkCapturingState);
    auto* value = dict.FindByDottedPath(
        kIPHNavigationCapturingPrefNames.all_blocked_time_name);
    EXPECT_NE(value, nullptr) << " ";
    return base::ValueToTime(value);
  }

  void FastForwardTimeForMaxDaysToStoreGuardrails() {
    task_environment()->FastForwardBy(base::Days(
        features::kNavigationCapturingIPHGuardrailStorageDuration.Get()));
  }

  bool IsDesktopLinkCapturingIphBlocked(const webapps::AppId& app) {
    return guardrails().IsBlockedByGuardrails(app);
  }

  void ForceUserBlockedOnIphGuardrails() {
    const std::vector<webapps::AppId> apps{"app1", "app2", "app3",
                                           "app4", "app5", "app6"};
    for (const webapps::AppId& app : apps) {
      guardrails().RecordDismiss(app, base::Time::Now());
      task_environment()->FastForwardBy(base::Milliseconds(1));
    }
    EXPECT_TRUE(IsDesktopLinkCapturingIphBlocked("app_id"));
    task_environment()->FastForwardBy(base::Milliseconds(1));
  }

 protected:
  WebAppPrefGuardrails guardrails() {
    return WebAppPrefGuardrails::GetForNavigationCapturingIph(prefs());
  }
  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(WebAppPrefsLinkCapturingIPHGuardrailsTest, Dismiss) {
  base::Time dismiss_time = base::Time::Now();
  guardrails().RecordDismiss(app_id, dismiss_time);
  {
    const auto& dict =
        prefs()->GetDict(prefs::kWebAppsAppAgnosticIPHLinkCapturingState);
    EXPECT_EQ(
        dict.FindInt(kIPHNavigationCapturingPrefNames.not_accepted_count_name)
            .value_or(0),
        1);
    EXPECT_EQ(base::ValueToTime(dict.Find(
                  kIPHNavigationCapturingPrefNames.last_dismiss_time_name)),
              dismiss_time);
  }
}

TEST_F(WebAppPrefsLinkCapturingIPHGuardrailsTest, Accept) {
  guardrails().RecordDismiss(app_id, base::Time::Now());
  {
    const auto& dict =
        prefs()->GetDict(prefs::kWebAppsAppAgnosticIPHLinkCapturingState);
    EXPECT_EQ(
        dict.FindInt(kIPHNavigationCapturingPrefNames.not_accepted_count_name)
            .value_or(0),
        1);
  }
  guardrails().RecordAccept(app_id);
  {
    const auto& dict =
        prefs()->GetDict(prefs::kWebAppsAppAgnosticIPHLinkCapturingState);
    EXPECT_EQ(
        dict.FindInt(kIPHNavigationCapturingPrefNames.not_accepted_count_name)
            .value_or(0),
        0);
  }
}

TEST_F(WebAppPrefsLinkCapturingIPHGuardrailsTest,
       GuardrailsBlockedAfter6Dismisses) {
  EXPECT_FALSE(IsDesktopIphBlockedTimeSet());

  ForceUserBlockedOnIphGuardrails();
  EXPECT_TRUE(IsDesktopIphBlockedTimeSet());
  EXPECT_TRUE(GetIphBlockedTime().has_value());
}

TEST_F(WebAppPrefsLinkCapturingIPHGuardrailsTest, ClearAndResetGuardrails) {
  ForceUserBlockedOnIphGuardrails();
  EXPECT_TRUE(IsDesktopIphBlockedTimeSet());

  FastForwardTimeForMaxDaysToStoreGuardrails();

  EXPECT_FALSE(IsDesktopLinkCapturingIphBlocked("app"));
  EXPECT_FALSE(IsDesktopIphBlockedTimeSet());

  const base::Value::Dict& dict =
      prefs()->GetDict(prefs::kWebAppsAppAgnosticIPHLinkCapturingState);
  std::optional<int> agnostic_not_installed_count =
      dict.FindInt(kIPHNavigationCapturingPrefNames.not_accepted_count_name);
  EXPECT_TRUE(agnostic_not_installed_count.has_value());
  EXPECT_EQ(*agnostic_not_installed_count, 0);
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
