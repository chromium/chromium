// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_pref_guardrails.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class WebAppGuardrailsTest : public testing::Test {
 public:
  WebAppGuardrailsTest() {
    WebAppPrefsUtilsRegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 protected:
  absl::optional<int> GetIntWebAppPref(const webapps::AppId& app_id,
                                       base::StringPiece path) {
    return ::web_app::GetIntWebAppPref(prefs(), app_id, path);
  }

  absl::optional<base::Time> GetTimeWebAppPref(const webapps::AppId& app_id,
                                               base::StringPiece path) {
    return ::web_app::GetTimeWebAppPref(prefs(), app_id, path);
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

  const webapps::AppId app_id = "test_app";
  const webapps::AppId app_id_2 = "test_app_2";
  const base::Time time_before_app_mute =
      base::Time::Now() -
      base::Days(kIphGuardrails.app_specific_mute_after_ignore_days.value()) -
      base::Hours(1);
  const base::Time time_before_global_mute =
      base::Time::Now() -
      base::Days(kIphGuardrails.app_agnostic_mute_after_ignore_days.value()) -
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

}  // namespace web_app
