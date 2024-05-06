// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "privacy_sandbox_notice_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

class PrivacySandboxNoticeStorageTestPeer {
 public:
  static PrivacySandboxNoticeStorage* GetPrivacySandboxNoticeStorageInstance() {
    static base::NoDestructor<PrivacySandboxNoticeStorage> instance;
    return instance.get();
  }
};

namespace {

class PrivacySandboxNoticeStorageTest : public testing::Test {
 public:
  PrivacySandboxNoticeStorageTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    PrivacySandboxNoticeStorage::RegisterProfilePrefs(prefs()->registry());
  }

  PrivacySandboxNoticeData NoticeTestData() {
    return PrivacySandboxNoticeData{
        /*schema_version=*/0,
        /*notice_action_taken=*/NoticeActionTaken::kAck,
        /*notice_action_taken_time=*/
        base::Time::FromMillisecondsSinceUnixEpoch(100),
        /*notice_first_shown=*/base::Time::FromMillisecondsSinceUnixEpoch(200),
        /*notice_last_shown=*/base::Time::FromMillisecondsSinceUnixEpoch(200),
        /*notice_shown_duration=*/base::Microseconds(199000),
    };
  }

  PrivacySandboxNoticeStorage* notice_storage() {
    return PrivacySandboxNoticeStorageTestPeer::
        GetPrivacySandboxNoticeStorageInstance();
  }

  // Sets notice related prefs.
  void SaveNoticeData(const PrivacySandboxNoticeData& notice_data,
                      std::string_view notice) {
    notice_storage()->SetSchemaVersion(prefs(), notice,
                                       notice_data.schema_version);
    notice_storage()->SetNoticeActionTaken(
        prefs(), notice, notice_data.notice_action_taken,
        notice_data.notice_action_taken_time);
    notice_storage()->SetNoticeShown(prefs(), notice,
                                     notice_data.notice_last_shown);
    notice_storage()->SetNoticeShownDuration(prefs(), notice,
                                             notice_data.notice_shown_duration);
  }

  // Compares notice related data.
  void CompareNoticeData(const PrivacySandboxNoticeData& expected,
                         const PrivacySandboxNoticeData& actual) {
    EXPECT_EQ(expected.schema_version, actual.schema_version);
    EXPECT_EQ(expected.notice_action_taken, actual.notice_action_taken);
    EXPECT_EQ(expected.notice_action_taken_time,
              actual.notice_action_taken_time);
    EXPECT_EQ(expected.notice_first_shown, actual.notice_first_shown);
    EXPECT_EQ(expected.notice_last_shown, actual.notice_last_shown);
    EXPECT_EQ(expected.notice_shown_duration, actual.notice_shown_duration);
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(PrivacySandboxNoticeStorageTest, NoticePathNotFound) {
  const auto actual =
      notice_storage()->ReadNoticeData(prefs(),
                                       /*notice=*/"NoticeTest1_v0");
  EXPECT_FALSE(actual.has_value());
}

TEST_F(PrivacySandboxNoticeStorageTest, NoNoticeDataSet) {
  PrivacySandboxNoticeData data = {};
  SaveNoticeData(data, /*notice=*/"NoticeTest1_v0");
  const auto actual =
      notice_storage()->ReadNoticeData(prefs(),
                                       /*notice=*/"NoticeTest1_v0");
  CompareNoticeData(data, *actual);
}

TEST_F(PrivacySandboxNoticeStorageTest, OnlySchemaVersionSet) {
  PrivacySandboxNoticeData data;
  data.schema_version = 1;
  SaveNoticeData(data, /*notice=*/"NoticeTest1_v0");
  const auto actual =
      notice_storage()->ReadNoticeData(prefs(),
                                       /*notice=*/"NoticeTest1_v0");
  CompareNoticeData(data, *actual);
}

TEST_F(PrivacySandboxNoticeStorageTest, SetsValuesAndReadsData) {
  const auto expected = NoticeTestData();
  SaveNoticeData(expected, /*notice=*/"NoticeTest1_v0");
  const auto actual =
      notice_storage()->ReadNoticeData(prefs(),
                                       /*notice=*/"NoticeTest1_v0");
  CompareNoticeData(expected, *actual);
}

TEST_F(PrivacySandboxNoticeStorageTest, UpdateNoticeShownValue) {
  SaveNoticeData(NoticeTestData(), /*notice=*/"NoticeTest1_v0");
  auto actual = notice_storage()->ReadNoticeData(prefs(),
                                                 /*notice=*/"NoticeTest1_v0");
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(200),
            actual->notice_first_shown);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(200),
            actual->notice_last_shown);

  // Set notice shown value again.
  notice_storage()->SetNoticeShown(
      prefs(), "NoticeTest1_v0",
      base::Time::FromMillisecondsSinceUnixEpoch(400));
  actual = notice_storage()->ReadNoticeData(prefs(),
                                            /*notice=*/"NoticeTest1_v0");
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(200),
            actual->notice_first_shown);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(400),
            actual->notice_last_shown);
}

TEST_F(PrivacySandboxNoticeStorageTest, SetMultipleNotices) {
  // Notice data 1.
  const auto expected_notice1 = NoticeTestData();
  SaveNoticeData(expected_notice1, /*notice=*/"NoticeTest1_v0");
  const auto actual_notice1 =
      notice_storage()->ReadNoticeData(prefs(),
                                       /*notice=*/"NoticeTest1_v0");

  // Notice data 2.
  auto expected_notice2 = NoticeTestData();
  expected_notice2.notice_action_taken = NoticeActionTaken::kSettings;
  expected_notice2.notice_action_taken_time =
      base::Time::FromMillisecondsSinceUnixEpoch(300);
  SaveNoticeData(expected_notice2, /*notice=*/"NoticeTest2_v0");
  const auto actual_notice2 =
      notice_storage()->ReadNoticeData(prefs(),
                                       /*notice=*/"NoticeTest2_v0");

  CompareNoticeData(expected_notice1, *actual_notice1);
  CompareNoticeData(expected_notice2, *actual_notice2);
}

}  // namespace
}  // namespace privacy_sandbox
