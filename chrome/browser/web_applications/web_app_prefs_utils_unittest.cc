// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_prefs_utils.h"

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
const webapps::AppId app_id = "test_app";
const webapps::AppId app_id_2 = "test_app_2";
const base::Time time_before_app_mute =
    base::Time::Now() - base::Days(kIphAppSpecificMuteTimeSpanDays) -
    base::Hours(1);
const base::Time time_before_global_mute =
    base::Time::Now() - base::Days(kIphAppAgnosticMuteTimeSpanDays) -
    base::Hours(1);
}  // namespace

class WebAppPrefsUtilsTest : public testing::Test {
 public:
  WebAppPrefsUtilsTest() {
    WebAppPrefsUtilsRegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(WebAppPrefsUtilsTest, TestIphIgnoreRecorded) {
  EXPECT_FALSE(GetIntWebAppPref(prefs(), kIphIgnoreCount, app_id).has_value());
  EXPECT_FALSE(
      GetTimeWebAppPref(prefs(), app_id, kIphLastIgnoreTime).has_value());

  RecordInstallIphIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(prefs(), app_id, kIphIgnoreCount).value_or(0), 1);
  auto last_ignore_time =
      GetTimeWebAppPref(prefs(), app_id, kIphLastIgnoreTime);
  EXPECT_TRUE(last_ignore_time.has_value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphIgnoreCount).value_or(0), 1);
    EXPECT_EQ(base::ValueToTime(dict.Find(kIphLastIgnoreTime)),
              last_ignore_time.value());
  }
}

TEST_F(WebAppPrefsUtilsTest, TestIphIgnoreRecordUpdated) {
  RecordInstallIphIgnored(prefs(), app_id, base::Time::Now());
  auto last_ignore_time =
      GetTimeWebAppPref(prefs(), app_id, kIphLastIgnoreTime);
  EXPECT_TRUE(last_ignore_time.has_value());

  RecordInstallIphIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(prefs(), app_id, kIphIgnoreCount).value_or(0), 2);
  EXPECT_NE(GetTimeWebAppPref(prefs(), app_id, kIphLastIgnoreTime).value(),
            last_ignore_time.value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphIgnoreCount).value_or(0), 2);
    EXPECT_NE(base::ValueToTime(dict.Find(kIphLastIgnoreTime)),
              last_ignore_time.value());
  }
}

TEST_F(WebAppPrefsUtilsTest, TestIphInstallResetCounters) {
  RecordInstallIphIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(prefs(), app_id, kIphIgnoreCount).value_or(0), 1);
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphIgnoreCount).value_or(0), 1);
  }
  RecordInstallIphInstalled(prefs(), app_id);
  EXPECT_EQ(GetIntWebAppPref(prefs(), app_id, kIphIgnoreCount).value_or(0), 0);
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphIgnoreCount).value_or(0), 0);
  }
}

TEST_F(WebAppPrefsUtilsTest, TestIphAppIgnoredRecently) {
  EXPECT_TRUE(ShouldShowIph(prefs(), app_id));
  RecordInstallIphIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_FALSE(ShouldShowIph(prefs(), app_id));
}

TEST_F(WebAppPrefsUtilsTest, TestIphGlobalIgnoredRecently) {
  EXPECT_TRUE(ShouldShowIph(prefs(), app_id));
  RecordInstallIphIgnored(prefs(), app_id_2, base::Time::Now());
  EXPECT_FALSE(ShouldShowIph(prefs(), app_id));
}

TEST_F(WebAppPrefsUtilsTest, TestIphGlobalIgnoredPassedMuteTime) {
  RecordInstallIphIgnored(prefs(), app_id_2, time_before_global_mute);
  EXPECT_TRUE(ShouldShowIph(prefs(), app_id));
}

TEST_F(WebAppPrefsUtilsTest, TestIphAppIgnoredPassedMuteTime) {
  RecordInstallIphIgnored(prefs(), app_id, time_before_app_mute);
  EXPECT_TRUE(ShouldShowIph(prefs(), app_id));
}

TEST_F(WebAppPrefsUtilsTest, TestIphConsecutiveAppIgnore) {
  RecordInstallIphIgnored(prefs(), app_id, time_before_app_mute);
  UpdateIntWebAppPref(prefs(), app_id, kIphIgnoreCount,
                      kIphMuteAfterConsecutiveAppSpecificIgnores);
  EXPECT_FALSE(ShouldShowIph(prefs(), app_id));
}

TEST_F(WebAppPrefsUtilsTest, TestGlobalConsecutiveAppIgnore) {
  RecordInstallIphIgnored(prefs(), app_id_2, time_before_global_mute);
  {
    ScopedDictPrefUpdate update(prefs(), prefs::kWebAppsAppAgnosticIphState);
    update->Set(kIphIgnoreCount, kIphMuteAfterConsecutiveAppAgnosticIgnores);
  }
  EXPECT_FALSE(ShouldShowIph(prefs(), app_id));
}

TEST_F(WebAppPrefsUtilsTest, TestTakeAllWebAppInstallSources) {
  base::Value old = *base::JSONReader::Read(R"({
      "app1": {},
      "app2": { "latest_web_app_install_source": 2 },
      "app3": {
          "latest_web_app_install_source": 3,
         "IPH_last_ignore_time": "123345567"
      }
  })");

  prefs()->Set(prefs::kWebAppsPreferences, std::move(old));
  EXPECT_TRUE(GetWebAppInstallSourceDeprecated(prefs(), "app2"));
  EXPECT_TRUE(GetWebAppInstallSourceDeprecated(prefs(), "app3"));

  std::map<webapps::AppId, int> values = TakeAllWebAppInstallSources(prefs());

  // Verify the returned map.
  ASSERT_EQ(2u, values.size());
  auto app1 = values.find("app1");
  ASSERT_TRUE(app1 == values.end());
  auto app2 = values.find("app2");
  ASSERT_FALSE(app2 == values.end());
  EXPECT_EQ(2, app2->second);
  auto app3 = values.find("app3");
  ASSERT_FALSE(app3 == values.end());
  EXPECT_EQ(3, app3->second);
  EXPECT_FALSE(GetWebAppInstallSourceDeprecated(prefs(), "app2"));
  EXPECT_FALSE(GetWebAppInstallSourceDeprecated(prefs(), "app3"));

  // Verify what's left behind in prefs.
  const base::Value* web_apps_prefs =
      prefs()->GetUserPrefValue(prefs::kWebAppsPreferences);
  ASSERT_TRUE(web_apps_prefs);
  ASSERT_TRUE(web_apps_prefs->is_dict());
  const base::Value::Dict& web_apps_pref_dict = web_apps_prefs->GetDict();
  EXPECT_EQ(1u, web_apps_pref_dict.size());
  EXPECT_FALSE(web_apps_pref_dict.Find("app1"));
  EXPECT_FALSE(web_apps_pref_dict.Find("app2"));
  EXPECT_TRUE(web_apps_pref_dict.Find("app3"));
  EXPECT_FALSE(web_apps_pref_dict.FindByDottedPath(
      "app3.latest_web_app_install_source"));
}

TEST_F(WebAppPrefsUtilsTest, MLInstallIgnored) {
  EXPECT_FALSE(GetTimeWebAppPref(prefs(), app_id, kLastTimeMlInstallIgnored)
                   .has_value());
  EXPECT_FALSE(
      GetIntWebAppPref(prefs(), app_id, kConsecutiveMlInstallNotAcceptedCount)
          .has_value());

  RecordMlInstallIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(
      GetIntWebAppPref(prefs(), app_id, kConsecutiveMlInstallNotAcceptedCount)
          .value_or(0),
      1);
  auto last_ignore_time =
      GetTimeWebAppPref(prefs(), app_id, kLastTimeMlInstallIgnored);
  EXPECT_TRUE(last_ignore_time.has_value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0),
              1);
    EXPECT_EQ(base::ValueToTime(dict.Find(kLastTimeMlInstallIgnored)),
              last_ignore_time.value());
  }
}

TEST_F(WebAppPrefsUtilsTest, MLInstallDismissed) {
  EXPECT_FALSE(GetTimeWebAppPref(prefs(), app_id, kLastTimeMlInstallDismissed)
                   .has_value());
  EXPECT_FALSE(
      GetIntWebAppPref(prefs(), app_id, kConsecutiveMlInstallNotAcceptedCount)
          .has_value());

  RecordMlInstallDismissed(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(
      GetIntWebAppPref(prefs(), app_id, kConsecutiveMlInstallNotAcceptedCount)
          .value_or(0),
      1);
  auto last_dismissed_time =
      GetTimeWebAppPref(prefs(), app_id, kLastTimeMlInstallDismissed);
  EXPECT_TRUE(last_dismissed_time.has_value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0),
              1);
    EXPECT_EQ(base::ValueToTime(dict.Find(kLastTimeMlInstallDismissed)),
              last_dismissed_time.value());
  }
}

TEST_F(WebAppPrefsUtilsTest, MLAcceptResetsCounters) {
  RecordMlInstallIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(
      GetIntWebAppPref(prefs(), app_id, kConsecutiveMlInstallNotAcceptedCount)
          .value_or(0),
      1);
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0),
              1);
  }
  RecordMlInstallAccepted(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(
      GetIntWebAppPref(prefs(), app_id, kConsecutiveMlInstallNotAcceptedCount)
          .value_or(0),
      0);
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0),
              0);
  }
}

TEST_F(WebAppPrefsUtilsTest, MLGuardrailConsecutiveAppSpecificIgnores) {
  RecordMlInstallIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(
      GetIntWebAppPref(prefs(), app_id, kConsecutiveMlInstallNotAcceptedCount)
          .value_or(0),
      1);
  RecordMlInstallIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(
      GetIntWebAppPref(prefs(), app_id, kConsecutiveMlInstallNotAcceptedCount)
          .value_or(0),
      2);
  RecordMlInstallIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(
      GetIntWebAppPref(prefs(), app_id, kConsecutiveMlInstallNotAcceptedCount)
          .value_or(0),
      3);
  EXPECT_TRUE(IsMlPromotionBlockedByHistoryGuardrail(prefs(), app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMLPromotionGuardrailBlockReason),
              "app_specific_not_accept_count_exceeded");
  }
}

TEST_F(WebAppPrefsUtilsTest, MLGuardrailAppSpecificIgnoreForDays) {
  base::Time now_time = base::Time::Now();
  RecordMlInstallIgnored(prefs(), app_id, now_time);
  auto ignore_time =
      GetTimeWebAppPref(prefs(), app_id, kLastTimeMlInstallIgnored);
  EXPECT_TRUE(ignore_time.has_value());
  EXPECT_EQ(now_time, ignore_time);

  base::Time forwarded_time = base::Time::Now() + base::Days(1);
  RecordMlInstallIgnored(prefs(), app_id, forwarded_time);
  auto ignore_time_new =
      GetTimeWebAppPref(prefs(), app_id, kLastTimeMlInstallIgnored);
  EXPECT_TRUE(ignore_time_new.has_value());
  EXPECT_EQ(forwarded_time, ignore_time_new);
  EXPECT_TRUE(IsMlPromotionBlockedByHistoryGuardrail(prefs(), app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMLPromotionGuardrailBlockReason),
              "app_specific_ml_install_ignore_days_hit");
  }
}

TEST_F(WebAppPrefsUtilsTest, MLGuardrailAppSpecificDismissForDays) {
  base::Time now_time = base::Time::Now();
  RecordMlInstallDismissed(prefs(), app_id, now_time);
  auto dismiss_time =
      GetTimeWebAppPref(prefs(), app_id, kLastTimeMlInstallDismissed);
  EXPECT_TRUE(dismiss_time.has_value());
  EXPECT_EQ(now_time, dismiss_time);

  // Dismissing the same app within 14 days should trigger the guardrail
  // response.
  int randDays = base::RandInt(1, 13);
  base::Time forwarded_time = base::Time::Now() + base::Days(randDays);
  RecordMlInstallDismissed(prefs(), app_id, forwarded_time);
  auto dismiss_time_new =
      GetTimeWebAppPref(prefs(), app_id, kLastTimeMlInstallDismissed);
  EXPECT_TRUE(dismiss_time_new.has_value());
  EXPECT_EQ(forwarded_time, dismiss_time_new);
  EXPECT_TRUE(IsMlPromotionBlockedByHistoryGuardrail(prefs(), app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMLPromotionGuardrailBlockReason),
              "app_specific_ml_install_dismiss_days_hit");
  }
}

TEST_F(WebAppPrefsUtilsTest, MLGuardrailConsecutiveAppAgnosticIgnores) {
  const webapps::AppId& app_id1 = "app1";
  const webapps::AppId& app_id2 = "app2";
  const webapps::AppId& app_id3 = "app3";
  const webapps::AppId& app_id4 = "app4";
  const webapps::AppId& app_id5 = "app5";
  RecordMlInstallIgnored(prefs(), app_id1, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0),
              1);
  }
  RecordMlInstallDismissed(prefs(), app_id2, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0),
              2);
  }
  RecordMlInstallDismissed(prefs(), app_id3, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0),
              3);
  }
  RecordMlInstallDismissed(prefs(), app_id4, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0),
              4);
  }
  RecordMlInstallIgnored(prefs(), app_id5, base::Time::Now());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(dict.FindInt(kConsecutiveMlInstallNotAcceptedCount).value_or(0),
              5);
  }
  EXPECT_TRUE(IsMlPromotionBlockedByHistoryGuardrail(prefs(), app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMLPromotionGuardrailBlockReason),
              "app_agnostic_not_accept_count_exceeded");
  }
}

TEST_F(WebAppPrefsUtilsTest, MLGuardrailConsecutiveAppAgnosticIgnoreDays) {
  const webapps::AppId& app_id1 = "app1";
  RecordMlInstallIgnored(prefs(), app_id1, base::Time::Now());
  auto last_ignored_time =
      GetTimeWebAppPref(prefs(), app_id1, kLastTimeMlInstallIgnored);
  EXPECT_TRUE(last_ignored_time.has_value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(base::ValueToTime(dict.Find(kLastTimeMlInstallIgnored)),
              last_ignored_time.value());
  }
  EXPECT_TRUE(IsMlPromotionBlockedByHistoryGuardrail(prefs(), app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMLPromotionGuardrailBlockReason),
              "app_agnostic_ml_install_ignore_days_hit");
  }
}

TEST_F(WebAppPrefsUtilsTest, MLGuardrailConsecutiveAppAgnosticDismissDays) {
  const webapps::AppId& app_id1 = "app1";

  // Dismissing any app within the last 7 days should trigger the app agnostic
  // dismiss guardrail response.
  int randDays = base::RandInt(0, 6);
  RecordMlInstallDismissed(prefs(), app_id1,
                           base::Time::Now() - base::Days(randDays));
  auto last_dismissed_time =
      GetTimeWebAppPref(prefs(), app_id1, kLastTimeMlInstallDismissed);
  EXPECT_TRUE(last_dismissed_time.has_value());
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(base::ValueToTime(dict.Find(kLastTimeMlInstallDismissed)),
              last_dismissed_time.value());
  }
  EXPECT_TRUE(IsMlPromotionBlockedByHistoryGuardrail(prefs(), app_id));
  {
    const auto& dict = prefs()->GetDict(prefs::kWebAppsAppAgnosticMlState);
    EXPECT_EQ(*dict.FindString(kMLPromotionGuardrailBlockReason),
              "app_agnostic_ml_install_dismiss_days_hit");
  }
}

}  // namespace web_app
