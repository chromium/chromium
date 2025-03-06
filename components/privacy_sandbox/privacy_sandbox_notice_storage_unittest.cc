// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/test/task_environment.h"
#include "base/version_info/version_info.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

namespace {

// TODO(crbug.com/333406690): Make a test notice name list injectable so tests
// don't have to use actual notice names.
class PrivacySandboxNoticeStorageTest : public testing::Test {
 public:
  PrivacySandboxNoticeStorageTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    PrivacySandboxNoticeStorage::RegisterProfilePrefs(prefs()->registry());
    notice_storage_ = std::make_unique<PrivacySandboxNoticeStorage>();
  }

  PrivacySandboxNoticeData NoticeTestData() {
    PrivacySandboxNoticeData data;
    data.SetSchemaVersion(1);
    data.SetChromeVersion(version_info::GetVersionNumber());
    data.notice_action_taken_ = NoticeActionTaken::kAck;
    data.notice_action_taken_time_ =
        base::Time::FromMillisecondsSinceUnixEpoch(200);
    data.notice_first_shown_ = base::Time::FromMillisecondsSinceUnixEpoch(100);
    data.notice_last_shown_ = base::Time::FromMillisecondsSinceUnixEpoch(100);
    data.notice_shown_duration_ = base::Milliseconds(100);
    return data;
  }

  PrivacySandboxNoticeStorage* notice_storage() {
    return notice_storage_.get();
  }

  // Sets notice related prefs.
  void SaveNoticeData(const PrivacySandboxNoticeData& notice_data,
                      std::string_view notice) {
    if (notice_data.notice_first_shown_ != base::Time()) {
      notice_storage()->SetNoticeShown(prefs(), notice,
                                       notice_data.notice_first_shown_);
    }
    if (notice_data.notice_last_shown_ != base::Time()) {
      notice_storage()->SetNoticeShown(prefs(), notice,
                                       notice_data.notice_last_shown_);
    }

    if (notice_data.notice_action_taken_ != NoticeActionTaken::kNotSet) {
      notice_storage()->SetNoticeActionTaken(
          prefs(), notice, notice_data.notice_action_taken_,
          notice_data.notice_action_taken_time_);
    }
  }

  // Compares notice related data.
  void CompareNoticeData(const PrivacySandboxNoticeData& expected,
                         const PrivacySandboxNoticeData& actual) {
    EXPECT_EQ(expected.GetSchemaVersion(), actual.GetSchemaVersion());
    EXPECT_EQ(expected.GetChromeVersion(), actual.GetChromeVersion());
    EXPECT_EQ(expected.notice_action_taken_, actual.notice_action_taken_);
    EXPECT_EQ(expected.notice_action_taken_time_,
              actual.notice_action_taken_time_);
    EXPECT_EQ(expected.notice_first_shown_, actual.notice_first_shown_);
    EXPECT_EQ(expected.notice_last_shown_, actual.notice_last_shown_);
    EXPECT_EQ(expected.notice_shown_duration_, actual.notice_shown_duration_);
  }

  std::string GetNoticeActionString(NoticeActionTaken action) {
    switch (action) {
      case NoticeActionTaken::kAck:
        return "Ack";
      case NoticeActionTaken::kClosed:
        return "Closed";
      case NoticeActionTaken::kOptIn:
        return "OptIn";
      case NoticeActionTaken::kOptOut:
        return "OptOut";
      case NoticeActionTaken::kSettings:
        return "Settings";
      default:
        return "";
    }
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<PrivacySandboxNoticeStorage> notice_storage_;
};

TEST_F(PrivacySandboxNoticeStorageTest, CheckPSNoticeHistograms) {
  std::optional<base::HistogramVariantsEntryMap> notices;
  std::vector<std::string> missing_notices;
  {
    notices = base::ReadVariantsFromHistogramsXml("PSNotice", "privacy");
    ASSERT_TRUE(notices.has_value());
  }
  EXPECT_EQ(std::size(kPrivacySandboxNoticeNames), notices->size());
  for (const auto& name : kPrivacySandboxNoticeNames) {
    // TODO(crbug.com/333406690): Implement something to clean up notices that
    // don't exist.
    if (!base::Contains(*notices, std::string(name))) {
      missing_notices.emplace_back(name);
    }
  }
  ASSERT_TRUE(missing_notices.empty())
      << "Notices:\n"
      << base::JoinString(missing_notices, ", ")
      << "\nconfigured in privacy_sandbox_notice_constants.h but no "
         "corresponding variants were added to PSNotice variants in "
         "//tools/metrics/histograms/metadata/privacy/histograms.xml";
}

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
      notice_storage()->ReadNoticeData(prefs(), kTopicsConsentModal);
  EXPECT_FALSE(actual.has_value());
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateDoesNotExist) {
  notice_storage()->RecordHistogramsOnStartup(prefs(), kTopicsConsentModal);
  const std::string histograms = histogram_tester_.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms, testing::Not(testing::AnyOf(
                              "PrivacySandbox.Notice.NoticeStartupState."
                              "TopicsConsentDesktopModal")));
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateUnknownState) {
  PrivacySandboxNoticeData data;
  data.notice_first_shown_ = base::Time::Now();
  data.notice_action_taken_ = NoticeActionTaken::kUnknownActionPreMigration;
  SaveNoticeData(data, kTopicsConsentModal);
  notice_storage()->RecordHistogramsOnStartup(prefs(), kTopicsConsentModal);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kUnknownState, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateWaiting) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken_ = NoticeActionTaken::kNotSet;
  data.notice_first_shown_ = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, kTopicsConsentModal);
  notice_storage()->RecordHistogramsOnStartup(prefs(), kTopicsConsentModal);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kPromptWaiting, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateFlowComplete) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken_ = NoticeActionTaken::kClosed;
  data.notice_first_shown_ = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, kTopicsConsentModal);
  notice_storage()->RecordHistogramsOnStartup(prefs(), kTopicsConsentModal);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kFlowCompleted, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateFlowCompleteOptIn) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken_ = NoticeActionTaken::kOptIn;
  data.notice_first_shown_ = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, kTopicsConsentModal);
  notice_storage()->RecordHistogramsOnStartup(prefs(), kTopicsConsentModal);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kFlowCompletedWithOptIn, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateFlowCompleteOptOut) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken_ = NoticeActionTaken::kOptOut;
  data.notice_first_shown_ = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, kTopicsConsentModal);
  notice_storage()->RecordHistogramsOnStartup(prefs(), kTopicsConsentModal);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kFlowCompletedWithOptOut, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateFlowCompleteAck) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken_ = NoticeActionTaken::kAck;
  data.notice_first_shown_ = base::Time::FromMillisecondsSinceUnixEpoch(200);
  SaveNoticeData(data, kTopicsConsentModal);
  notice_storage()->RecordHistogramsOnStartup(prefs(), kTopicsConsentModal);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kFlowCompleted, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, NoNoticeNameExpectCrash) {
  PrivacySandboxNoticeData data = NoticeTestData();
  data.SetChromeVersion("");
  EXPECT_DEATH_IF_SUPPORTED(SaveNoticeData(data, "Notice1"), "");
}

TEST_F(PrivacySandboxNoticeStorageTest, SetsValuesAndReadsData) {
  auto expected = NoticeTestData();
  SaveNoticeData(expected, kTopicsConsentModal);
  const auto actual =
      notice_storage()->ReadNoticeData(prefs(), kTopicsConsentModal);
  CompareNoticeData(expected, *actual);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       ReActionDoesNotRegisterAndEmitsHistogram) {
  std::string notice_name = kTopicsConsentModal;
  auto data = NoticeTestData();
  SaveNoticeData(data, notice_name);

  auto actual = notice_storage()->ReadNoticeData(prefs(), notice_name);
  EXPECT_EQ(NoticeActionTaken::kAck, actual->notice_action_taken_);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kAck, 1);

  // Tries to override action, should not override and emits histograms.
  notice_storage()->SetNoticeActionTaken(
      prefs(), notice_name, NoticeActionTaken::kSettings, base::Time::Now());
  EXPECT_EQ(NoticeActionTaken::kAck, actual->notice_action_taken_);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kSettings, 0);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeActionTakenBehavior."
      "TopicsConsentDesktopModal",
      NoticeActionBehavior::kDuplicateActionTaken, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, UpdateNoticeShownValue) {
  auto data = NoticeTestData();
  SaveNoticeData(data, kTopicsConsentModal);
  auto actual = notice_storage()->ReadNoticeData(prefs(), kTopicsConsentModal);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(100),
            actual->notice_first_shown_);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(100),
            actual->notice_last_shown_);
  EXPECT_EQ(base::Milliseconds(100), actual->notice_shown_duration_);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShownForFirstTime.TopicsConsentDesktopModal",
      true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);

  // Set notice shown value again.
  notice_storage()->SetNoticeShown(
      prefs(), kTopicsConsentModal,
      base::Time::FromMillisecondsSinceUnixEpoch(150));
  actual = notice_storage()->ReadNoticeData(prefs(), kTopicsConsentModal);
  // Sets twice in SaveNoticeData(...) and then once again above.
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShownForFirstTime.TopicsConsentDesktopModal",
      false, 2);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(100),
            actual->notice_first_shown_);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(150),
            actual->notice_last_shown_);
}

TEST_F(PrivacySandboxNoticeStorageTest, SetMultipleNotices) {
  // Notice data 1.
  auto expected_notice1 = NoticeTestData();
  SaveNoticeData(expected_notice1, kTopicsConsentModal);
  const auto actual_notice1 =
      notice_storage()->ReadNoticeData(prefs(), kTopicsConsentModal);

  // Notice data 2.
  auto expected_notice2 = NoticeTestData();
  expected_notice2.notice_action_taken_ = NoticeActionTaken::kSettings;
  expected_notice2.notice_action_taken_time_ =
      base::Time::FromMillisecondsSinceUnixEpoch(300);
  expected_notice2.notice_shown_duration_ = base::Milliseconds(200);
  SaveNoticeData(expected_notice2, kTopicsConsentModalClankCCT);
  const auto actual_notice2 =
      notice_storage()->ReadNoticeData(prefs(), kTopicsConsentModalClankCCT);

  CompareNoticeData(expected_notice1, *actual_notice1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);
  CompareNoticeData(expected_notice2, *actual_notice2);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentModalClankCCT",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentModalClankCCT_"
      "Settings",
      base::Milliseconds(200), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentModalClankCCT_"
      "Settings",
      base::Milliseconds(200), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentModalClankCCT", true, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataNoticeActionOnlyMigratePrefsSuccess) {
  PrivacySandboxNoticeData expected_notice;
  expected_notice.SetSchemaVersion(1);
  expected_notice.notice_action_taken_ = NoticeActionTaken::kSettings;
  expected_notice.notice_action_taken_time_ =
      base::Time::FromMillisecondsSinceUnixEpoch(500);
  std::string notice_name = kTopicsConsentModal;
  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);
  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);

  CompareNoticeData(expected_notice, *actual_notice);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kSettings, 1);
  const std::string histograms = histogram_tester_.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              testing::Not(testing::AnyOf(
                  "PrivacySandbox.Notice.FirstShownToInteractedDuration."
                  "TopicsConsentDesktopModal_Settings")));
  EXPECT_THAT(histograms,
              testing::Not(testing::AnyOf(
                  "PrivacySandbox.Notice.LastShownToInteractedDuration."
                  "TopicsConsentDesktopModal_Settings")));
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataNoticeShownOnlyMigratePrefsSuccess) {
  PrivacySandboxNoticeData expected_notice;
  expected_notice.SetSchemaVersion(1);
  expected_notice.notice_first_shown_ = base::Time::Now();
  expected_notice.notice_last_shown_ = base::Time::Now();
  std::string notice_name = kTopicsConsentModal;
  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);
  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);

  CompareNoticeData(expected_notice, *actual_notice);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataAllValuesMigratePrefsSuccess) {
  auto expected_notice = NoticeTestData();
  expected_notice.SetChromeVersion("");
  std::string notice_name = kTopicsConsentModal;

  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);

  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);

  CompareNoticeData(expected_notice, *actual_notice);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataReNoticeActionDoesNotOverwrite) {
  // Original notice.
  auto expected_notice = NoticeTestData();
  expected_notice.SetChromeVersion("");
  std::string notice_name = kTopicsConsentModal;

  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);

  // Notice data 2.
  PrivacySandboxNoticeData notice_data2;
  notice_data2.notice_action_taken_ = NoticeActionTaken::kSettings;
  notice_data2.notice_action_taken_time_ =
      base::Time::FromMillisecondsSinceUnixEpoch(500);

  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), notice_data2,
                                                    notice_name);

  // Prefs should still match original notice data.
  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);
  CompareNoticeData(expected_notice, *actual_notice);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Ack",
      base::Milliseconds(100), 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MigrateNoticeDataReNoticeShownDoesNotOverwrite) {
  // Original notice.
  auto expected_notice = NoticeTestData();
  expected_notice.SetChromeVersion("");
  std::string notice_name = kTopicsConsentModal;

  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), expected_notice,
                                                    notice_name);

  // Notice data 2.
  PrivacySandboxNoticeData notice_data2;
  notice_data2.notice_first_shown_ = base::Time::Now();
  notice_data2.notice_last_shown_ = base::Time::Now();
  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), notice_data2,
                                                    notice_name);

  // Prefs should still match original notice data.
  const auto actual_notice =
      notice_storage()->ReadNoticeData(prefs(), notice_name);
  CompareNoticeData(expected_notice, *actual_notice);
}

using NoticeEvents = std::vector<std::pair<NoticeEvent, base::Time>>;
class PrivacySandboxNoticeStorageV2Test
    : public PrivacySandboxNoticeStorageTest {};

TEST_F(PrivacySandboxNoticeStorageV2Test,
       AllEventsPopulatedMigrateSuccessfully) {
  PrivacySandboxNoticeData data;
  data.notice_last_shown_ = base::Time::FromMillisecondsSinceUnixEpoch(100);
  data.notice_action_taken_ = NoticeActionTaken::kAck;
  data.notice_action_taken_time_ =
      base::Time::FromMillisecondsSinceUnixEpoch(200);
  std::string notice_name = kTopicsConsentModal;
  SaveNoticeData(data, notice_name);

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  NoticeEvents events =
      notice_storage()->ReadNoticeData(prefs(), notice_name)->GetNoticeEvents();
  auto expected = std::make_pair(
      NoticeEvent::kShown, base::Time::FromMillisecondsSinceUnixEpoch(100));
  EXPECT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0], expected);

  auto expected1 = std::make_pair(
      NoticeEvent::kAck, base::Time::FromMillisecondsSinceUnixEpoch(200));
  EXPECT_EQ(events[1], expected1);
}

TEST_F(PrivacySandboxNoticeStorageV2Test,
       NoticeShownPopulatedMigrateSuccessfully) {
  PrivacySandboxNoticeData data;
  data.notice_last_shown_ = base::Time::FromMillisecondsSinceUnixEpoch(500);
  std::string notice_name = kTopicsConsentModal;
  SaveNoticeData(data, notice_name);

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  NoticeEvents events =
      notice_storage()->ReadNoticeData(prefs(), notice_name)->GetNoticeEvents();
  auto expected = std::make_pair(
      NoticeEvent::kShown, base::Time::FromMillisecondsSinceUnixEpoch(500));
  EXPECT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0], expected);
}

TEST_F(PrivacySandboxNoticeStorageV2Test, SchemaAlreadyUpToDateDoesNotMigrate) {
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(
      base::StrCat({kTopicsConsentModal, ".schema_version"}), 2);

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());
  NoticeEvents events = notice_storage()
                            ->ReadNoticeData(prefs(), kTopicsConsentModal)
                            ->GetNoticeEvents();
  EXPECT_EQ(events.size(), 0u);
}

class PrivacySandboxNoticeStorageV2ActionsTest
    : public PrivacySandboxNoticeStorageTest,
      public testing::WithParamInterface<
          std::tuple<NoticeActionTaken, std::optional<NoticeEvent>>> {};

TEST_P(PrivacySandboxNoticeStorageV2ActionsTest,
       NoticeActionWithoutShownPopulatedMigrateSuccessfully) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken_ = std::get<0>(GetParam());
  data.notice_action_taken_time_ =
      base::Time::FromMillisecondsSinceUnixEpoch(200);
  std::string notice_name = kTopicsConsentModal;
  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), data, notice_name);

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  NoticeEvents events =
      notice_storage()->ReadNoticeData(prefs(), notice_name)->GetNoticeEvents();
  auto notice_event = std::get<1>(GetParam());
  if (notice_event) {
    auto expected = std::make_pair(
        *notice_event, base::Time::FromMillisecondsSinceUnixEpoch(200));
    EXPECT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], expected);
  } else {
    EXPECT_EQ(events.size(), 0u);
  }
}

TEST_P(PrivacySandboxNoticeStorageV2ActionsTest,
       NoticeActionPopulatedWithoutTimestampMigrateSuccessfully) {
  PrivacySandboxNoticeData data;
  data.notice_action_taken_ = std::get<0>(GetParam());
  std::string notice_name = kTopicsConsentModal;
  notice_storage()->MigratePrivacySandboxNoticeData(prefs(), data, notice_name);

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  NoticeEvents events =
      notice_storage()->ReadNoticeData(prefs(), notice_name)->GetNoticeEvents();
  auto notice_event = std::get<1>(GetParam());
  if (notice_event) {
    auto expected = std::make_pair(*notice_event, base::Time());
    EXPECT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], expected);
  } else {
    EXPECT_EQ(events.size(), 0u);
  }
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxNoticeStorageV2ActionsTest,
    PrivacySandboxNoticeStorageV2ActionsTest,
    testing::ValuesIn(
        std::vector<std::tuple<NoticeActionTaken, std::optional<NoticeEvent>>>{
            {NoticeActionTaken::kNotSet, std::nullopt},
            {NoticeActionTaken::kAck, NoticeEvent::kAck},
            {NoticeActionTaken::kClosed, NoticeEvent::kClosed},
            {NoticeActionTaken::kLearnMore_Deprecated, std::nullopt},
            {NoticeActionTaken::kOptIn, NoticeEvent::kOptIn},
            {NoticeActionTaken::kOptOut, NoticeEvent::kOptOut},
            {NoticeActionTaken::kOther, std::nullopt},
            {NoticeActionTaken::kSettings, NoticeEvent::kSettings},
            {NoticeActionTaken::kUnknownActionPreMigration, std::nullopt},
            {NoticeActionTaken::kTimedOut, std::nullopt}}));

class PrivacySandboxNoticeDataTest : public testing::Test {};

TEST_F(PrivacySandboxNoticeDataTest, NoPrivacySandboxNoticeDataReturnsNothing) {
  PrivacySandboxNoticeData data;
  EXPECT_EQ(data.GetNoticeFirstShownFromEvents(), std::nullopt);
  EXPECT_EQ(data.GetNoticeLastShownFromEvents(), std::nullopt);
  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(), std::nullopt);
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoticeShownEvent_AccessorReturnsFirstShownSuccessfully) {
  std::vector<std::pair<NoticeEvent, base::Time>> notice_events = {
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(100)),
      std::make_pair(NoticeEvent::kAck,
                     base::Time::FromMillisecondsSinceUnixEpoch(150)),
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(200))};
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(notice_events);
  EXPECT_EQ(data.GetNoticeFirstShownFromEvents(),
            base::Time::FromMillisecondsSinceUnixEpoch(100));
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoticeShownEvent_AccessorReturnsLastShownSuccessfully) {
  std::vector<std::pair<NoticeEvent, base::Time>> notice_events = {
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(100)),
      std::make_pair(NoticeEvent::kAck,
                     base::Time::FromMillisecondsSinceUnixEpoch(150)),
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(200))};
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(notice_events);
  EXPECT_EQ(data.GetNoticeLastShownFromEvents(),
            base::Time::FromMillisecondsSinceUnixEpoch(200));
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoNoticeActionTakenEvent_AccessorReturnsNoValue) {
  std::vector<std::pair<NoticeEvent, base::Time>> notice_events = {
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(100)),
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(200))};
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(notice_events);
  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(), std::nullopt);
}

TEST_F(PrivacySandboxNoticeDataTest,
       NoticeActionTakenEvent_AccessorReturnsActionSuccessfully) {
  std::vector<std::pair<NoticeEvent, base::Time>> notice_events = {
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(100)),
      std::make_pair(NoticeEvent::kAck,
                     base::Time::FromMillisecondsSinceUnixEpoch(120)),
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(200)),
      std::make_pair(NoticeEvent::kOptIn,
                     base::Time::FromMillisecondsSinceUnixEpoch(250))};
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(notice_events);
  auto expected = std::make_pair(
      NoticeEvent::kAck, base::Time::FromMillisecondsSinceUnixEpoch(120));
  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(), expected);
}

TEST_F(
    PrivacySandboxNoticeDataTest,
    NoticeActionTakenEvent_AccessorReturnsActionSuccessfullyMultipleActions) {
  std::vector<std::pair<NoticeEvent, base::Time>> notice_events = {
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(100)),
      std::make_pair(NoticeEvent::kAck,
                     base::Time::FromMillisecondsSinceUnixEpoch(120)),
      std::make_pair(NoticeEvent::kSettings,
                     base::Time::FromMillisecondsSinceUnixEpoch(150)),
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(200)),
      std::make_pair(NoticeEvent::kOptIn,
                     base::Time::FromMillisecondsSinceUnixEpoch(250))};
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(notice_events);
  auto expected = std::make_pair(
      NoticeEvent::kSettings, base::Time::FromMillisecondsSinceUnixEpoch(150));
  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(), expected);
}

TEST_F(
    PrivacySandboxNoticeDataTest,
    NoticeActionTakenEvent_AccessorReturnsActionSuccessfullyWithMultipleShownValues) {
  std::vector<std::pair<NoticeEvent, base::Time>> notice_events = {
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(100)),
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(110)),
      std::make_pair(NoticeEvent::kAck,
                     base::Time::FromMillisecondsSinceUnixEpoch(120)),
      std::make_pair(NoticeEvent::kSettings,
                     base::Time::FromMillisecondsSinceUnixEpoch(150)),
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(200)),
      std::make_pair(NoticeEvent::kShown,
                     base::Time::FromMillisecondsSinceUnixEpoch(220)),
      std::make_pair(NoticeEvent::kOptIn,
                     base::Time::FromMillisecondsSinceUnixEpoch(250))};
  PrivacySandboxNoticeData data;
  data.SetNoticeEvents(notice_events);
  auto expected = std::make_pair(
      NoticeEvent::kSettings, base::Time::FromMillisecondsSinceUnixEpoch(150));
  EXPECT_EQ(data.GetNoticeActionTakenForFirstShownFromEvents(), expected);
}

}  // namespace
}  // namespace privacy_sandbox
