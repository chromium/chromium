// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
const AppId app_id = "test_app";
const AppId app_id_2 = "test_app_2";
const base::Time time_before_app_mute =
    base::Time::Now() -
    base::TimeDelta::FromDays(kIphAppSpecificMuteTimeSpanDays) -
    base::TimeDelta::FromHours(1);
const base::Time time_before_global_mute =
    base::Time::Now() -
    base::TimeDelta::FromDays(kIphAppAgnosticMuteTimeSpanDays) -
    base::TimeDelta::FromHours(1);
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
    auto* dict = prefs()->GetDictionary(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict->FindIntKey(kIphIgnoreCount).value_or(0), 1);
    EXPECT_EQ(util::ValueToTime(dict->FindKey(kIphLastIgnoreTime)),
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
    auto* dict = prefs()->GetDictionary(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict->FindIntKey(kIphIgnoreCount).value_or(0), 2);
    EXPECT_NE(util::ValueToTime(dict->FindKey(kIphLastIgnoreTime)),
              last_ignore_time.value());
  }
}

TEST_F(WebAppPrefsUtilsTest, TestIphInstallResetCounters) {
  RecordInstallIphIgnored(prefs(), app_id, base::Time::Now());
  EXPECT_EQ(GetIntWebAppPref(prefs(), app_id, kIphIgnoreCount).value_or(0), 1);
  {
    auto* dict = prefs()->GetDictionary(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict->FindIntKey(kIphIgnoreCount).value_or(0), 1);
  }
  RecordInstallIphInstalled(prefs(), app_id);
  EXPECT_EQ(GetIntWebAppPref(prefs(), app_id, kIphIgnoreCount).value_or(0), 0);
  {
    auto* dict = prefs()->GetDictionary(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict->FindIntKey(kIphIgnoreCount).value_or(0), 0);
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
    prefs::ScopedDictionaryPrefUpdate update(
        prefs(), prefs::kWebAppsAppAgnosticIphState);
    update->SetInteger(kIphIgnoreCount,
                       kIphMuteAfterConsecutiveAppAgnosticIgnores);
  }
  EXPECT_FALSE(ShouldShowIph(prefs(), app_id));
}
}  // namespace web_app
