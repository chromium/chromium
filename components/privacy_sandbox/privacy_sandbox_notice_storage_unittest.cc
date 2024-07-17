// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "privacy_sandbox_notice_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

namespace {

class PrivacySandboxNoticeStorageTest : public testing::Test {
 public:
  PrivacySandboxNoticeStorageTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    PrivacySandboxNoticeStorage::RegisterProfilePrefs(prefs()->registry());
    notice_storage_ = std::make_unique<PrivacySandboxNoticeStorage>();
  }

  PrivacySandboxNoticeData NoticeTestData() {
    PrivacySandboxNoticeData data;
    data.schema_version = 1;
    data.notice_action_taken = NoticeActionTaken::kAck;
    data.notice_action_taken_time =
        base::Time::FromMillisecondsSinceUnixEpoch(200);
    data.notice_first_shown = base::Time::FromMillisecondsSinceUnixEpoch(100);
    data.notice_last_shown = base::Time::FromMillisecondsSinceUnixEpoch(100);
    data.notice_shown_duration = base::Milliseconds(100);
    return data;
  }

  PrivacySandboxNoticeStorage* notice_storage() {
    return notice_storage_.get();
  }

  // Sets notice related prefs.
  void SaveNoticeData(const PrivacySandboxNoticeData& notice_data,
                      std::string_view notice) {
    if (notice_data.notice_first_shown != base::Time()) {
      notice_storage()->SetNoticeShown(prefs(), notice,
                                       notice_data.notice_first_shown);
    }
    if (notice_data.notice_last_shown != base::Time()) {
      notice_storage()->SetNoticeShown(prefs(), notice,
                                       notice_data.notice_last_shown);
    }

    if (notice_data.notice_action_taken != NoticeActionTaken::kNotSet) {
      notice_storage()->SetNoticeActionTaken(
          prefs(), notice, notice_data.notice_action_taken,
          notice_data.notice_action_taken_time);
    }
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

  std::string GetNoticeActionString(NoticeActionTaken action) {
    switch (action) {
      case NoticeActionTaken::kNotSet:
      case NoticeActionTaken::kUnknownActionPreMigration:
        return "";
      case NoticeActionTaken::kAck:
        return "Ack";
      case NoticeActionTaken::kClosed:
        return "Closed";
      case NoticeActionTaken::kLearnMore:
        return "LearnMore";
      case NoticeActionTaken::kOptIn:
        return "OptIn";
      case NoticeActionTaken::kOptOut:
        return "OptOut";
      case NoticeActionTaken::kSettings:
        return "Settings";
      case NoticeActionTaken::kOther:
        return "Other";
    }
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<PrivacySandboxNoticeStorage> notice_storage_;
};

TEST_F(PrivacySandboxNoticeStorageTest, CheckPSNoticeActionHistograms) {
  std::optional<base::HistogramVariantsEntryMap> actions;
  std::vector<std::string> missing_actions;
  {
    actions = base::ReadVariantsFromHistogramsXml("PSNoticeAction", "privacy");
    ASSERT_TRUE(actions.has_value());
  }

  for (int i = static_cast<int>(NoticeActionTaken::kNotSet);
       i <= static_cast<int>(NoticeActionTaken::kMaxValue); ++i) {
    std::string notice_name =
        GetNoticeActionString(static_cast<NoticeActionTaken>(i));
    if (!notice_name.empty() && !base::Contains(*actions, notice_name)) {
      missing_actions.emplace_back(notice_name);
    }
  }
  ASSERT_TRUE(missing_actions.empty())
      << "Actions:\n"
      << base::JoinString(missing_actions, ", ")
      << "\nconfigured in privacy_sandbox_notice_storage.cc but no "
         "corresponding variants were added to PSNoticeAction variants in "
         "//tools/metrics/histograms/metadata/privacy/histograms.xml";
}

TEST_F(PrivacySandboxNoticeStorageTest, NoticePathNotFound) {
  const auto actual =
      notice_storage()->ReadNoticeData(prefs(),
                                       /*notice=*/"NoticeTest1_v0");
  EXPECT_FALSE(actual.has_value());
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateDoesNotExist) {
  notice_storage()->RecordHistogramsOnStartup(prefs(),
                                              /*notice=*/"NoticeTest1_v0");
  const std::string histograms = histogram_tester_.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              testing::Not(testing::AnyOf(
                  "PrivacySandbox.Notice.NoticeStartupState.NoticeTest1_v0")));
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateUnknownState) {
  PrivacySandboxNoticeData data;
  data.notice_first_shown = base::Time::Now();
  data.notice_action_taken = NoticeActionTaken::kUnknownActionPreMigration;
  SaveNoticeData(data, /*notice=*/"NoticeTest1_v0");
  notice_storage()->RecordHistogramsOnStartup(prefs(),
                                              /*notice=*/"NoticeTest1_v0");
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.NoticeTest1_v0",
      NoticeStartupState::kUnknownState, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateWaiting) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken = NoticeActionTaken::kNotSet;
  data.notice_first_shown = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, /*notice=*/"NoticeTest1_v0");
  notice_storage()->RecordHistogramsOnStartup(prefs(),
                                              /*notice=*/"NoticeTest1_v0");
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.NoticeTest1_v0",
      NoticeStartupState::kPromptWaiting, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateFlowComplete) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken = NoticeActionTaken::kClosed;
  data.notice_first_shown = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, /*notice=*/"NoticeTest1_v0");
  notice_storage()->RecordHistogramsOnStartup(prefs(),
                                              /*notice=*/"NoticeTest1_v0");
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.NoticeTest1_v0",
      NoticeStartupState::kFlowCompleted, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateFlowCompleteOptIn) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken = NoticeActionTaken::kOptIn;
  data.notice_first_shown = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, /*notice=*/"NoticeTest1_v0");
  notice_storage()->RecordHistogramsOnStartup(prefs(),
                                              /*notice=*/"NoticeTest1_v0");
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.NoticeTest1_v0",
      NoticeStartupState::kFlowCompletedWithOptIn, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateFlowCompleteOptOut) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken = NoticeActionTaken::kOptOut;
  data.notice_first_shown = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, /*notice=*/"NoticeTest1_v0");
  notice_storage()->RecordHistogramsOnStartup(prefs(),
                                              /*notice=*/"NoticeTest1_v0");
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.NoticeTest1_v0",
      NoticeStartupState::kFlowCompletedWithOptOut, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateFlowCompleteAck) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken = NoticeActionTaken::kAck;
  data.notice_first_shown = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, /*notice=*/"NoticeTest1_v0");
  notice_storage()->RecordHistogramsOnStartup(prefs(),
                                              /*notice=*/"NoticeTest1_v0");
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.NoticeTest1_v0",
      NoticeStartupState::kFlowCompleted, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, SetsValuesAndReadsData) {
  const auto expected = NoticeTestData();
  SaveNoticeData(expected, /*notice=*/"NoticeTest1_v0");
  const auto actual =
      notice_storage()->ReadNoticeData(prefs(),
                                       /*notice=*/"NoticeTest1_v0");
  CompareNoticeData(expected, *actual);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.NoticeTest1_v0",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.NoticeTest1_v0", true, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       ReActionDoesNotRegisterAndEmitsHistogram) {
  std::string notice_name = "NoticeTest1_v0";
  SaveNoticeData(NoticeTestData(), notice_name);
  auto actual = notice_storage()->ReadNoticeData(prefs(), notice_name);
  EXPECT_EQ(NoticeActionTaken::kAck, actual->notice_action_taken);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.NoticeTest1_v0",
      NoticeActionTaken::kAck, 1);

  // Tries to override action, should not override and emits histograms.
  notice_storage()->SetNoticeActionTaken(
      prefs(), notice_name, NoticeActionTaken::kLearnMore, base::Time::Now());
  EXPECT_EQ(NoticeActionTaken::kAck, actual->notice_action_taken);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.NoticeTest1_v0",
      NoticeActionTaken::kLearnMore, 0);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeActionTakenBehavior.NoticeTest1_v0",
      NoticeActionBehavior::kDuplicateActionTaken, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, UpdateNoticeShownValue) {
  SaveNoticeData(NoticeTestData(), /*notice=*/"NoticeTest1_v0");
  auto actual = notice_storage()->ReadNoticeData(prefs(),
                                                 /*notice=*/"NoticeTest1_v0");
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(100),
            actual->notice_first_shown);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(100),
            actual->notice_last_shown);
  EXPECT_EQ(base::Milliseconds(100), actual->notice_shown_duration);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.NoticeTest1_v0",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.NoticeTest1_v0", true, 1);

  // Set notice shown value again.
  notice_storage()->SetNoticeShown(
      prefs(), "NoticeTest1_v0",
      base::Time::FromMillisecondsSinceUnixEpoch(150));
  actual = notice_storage()->ReadNoticeData(prefs(),
                                            /*notice=*/"NoticeTest1_v0");
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(100),
            actual->notice_first_shown);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(150),
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
  expected_notice2.notice_shown_duration = base::Milliseconds(200);
  SaveNoticeData(expected_notice2, /*notice=*/"NoticeTest2_v0");
  const auto actual_notice2 =
      notice_storage()->ReadNoticeData(prefs(),
                                       /*notice=*/"NoticeTest2_v0");

  CompareNoticeData(expected_notice1, *actual_notice1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.NoticeTest1_v0",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.NoticeTest1_v0", true, 1);
  CompareNoticeData(expected_notice2, *actual_notice2);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.NoticeTest2_v0",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration.NoticeTest2_v0_"
      "Settings",
      base::Milliseconds(200), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration.NoticeTest2_v0_"
      "Settings",
      base::Milliseconds(200), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.NoticeTest2_v0", true, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataNoticeActionOnlyMigratePrefsSuccess) {
  PrivacySandboxNoticeData expected_notice;
  expected_notice.schema_version = 1;
  expected_notice.notice_action_taken = NoticeActionTaken::kSettings;
  expected_notice.notice_action_taken_time =
      base::Time::FromMillisecondsSinceUnixEpoch(500);
  std::string notice_name = "NoticeTest1_v0";
  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);
  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);

  CompareNoticeData(expected_notice, *actual_notice);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.NoticeTest1_v0",
      NoticeActionTaken::kSettings, 1);
  const std::string histograms = histogram_tester_.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              testing::Not(testing::AnyOf(
                  "PrivacySandbox.Notice.FirstShownToInteractedDuration."
                  "NoticeTest1_v0_Settings")));
  EXPECT_THAT(histograms,
              testing::Not(testing::AnyOf(
                  "PrivacySandbox.Notice.LastShownToInteractedDuration."
                  "NoticeTest1_v0_Settings")));
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataNoticeShownOnlyMigratePrefsSuccess) {
  PrivacySandboxNoticeData expected_notice;
  expected_notice.schema_version = 1;
  expected_notice.notice_first_shown = base::Time::Now();
  expected_notice.notice_last_shown = base::Time::Now();
  std::string notice_name = "NoticeTest1_v0";
  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);
  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);

  CompareNoticeData(expected_notice, *actual_notice);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.NoticeTest1_v0", true, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataAllValuesMigratePrefsSuccess) {
  const auto expected_notice = NoticeTestData();
  std::string notice_name = "NoticeTest1_v0";

  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);

  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);

  CompareNoticeData(expected_notice, *actual_notice);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.NoticeTest1_v0",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.NoticeTest1_v0", true, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataReNoticeActionDoesNotOverwrite) {
  // Original notice.
  const auto expected_notice = NoticeTestData();
  std::string notice_name = "NoticeTest1_v0";

  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);

  // Notice data 2.
  PrivacySandboxNoticeData notice_data2;
  notice_data2.notice_action_taken = NoticeActionTaken::kSettings;
  notice_data2.notice_action_taken_time =
      base::Time::FromMillisecondsSinceUnixEpoch(500);

  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), notice_data2,
                                                    notice_name);

  // Prefs should still match original notice data.
  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);
  CompareNoticeData(expected_notice, *actual_notice);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.NoticeTest1_v0",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration.NoticeTest1_v0_Ack",
      base::Milliseconds(100), 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataReNoticeShownDoesNotOverwrite) {
  // Original notice.
  auto expected_notice = NoticeTestData();
  std::string notice_name = "NoticeTest1_v0";

  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);

  // Notice data 2.
  PrivacySandboxNoticeData notice_data2;
  notice_data2.notice_first_shown = base::Time::Now();
  notice_data2.notice_last_shown = base::Time::Now();
  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), notice_data2,
                                                    notice_name);

  // Prefs should still match original notice data.
  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);
  CompareNoticeData(expected_notice, *actual_notice);
}

}  // namespace
}  // namespace privacy_sandbox
