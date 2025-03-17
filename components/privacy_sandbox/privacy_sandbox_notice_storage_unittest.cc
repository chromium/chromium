// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version_info/version_info.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
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
    scoped_feature_list_.InitAndEnableFeature(
        kPrivacySandboxMigratePrefsToSchemaV2);
  }

  PrivacySandboxNoticeStorage* notice_storage() {
    return notice_storage_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<PrivacySandboxNoticeStorage> notice_storage_;
  base::test::ScopedFeatureList scoped_feature_list_;
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

  for (int i = static_cast<int>(NoticeEvent::kAck);
       i <= static_cast<int>(NoticeEvent::kMaxValue); ++i) {
    std::string notice_name =
        PrivacySandboxNoticeStorage::GetNoticeActionStringFromEvent(
            static_cast<NoticeEvent>(i));
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

TEST_F(PrivacySandboxNoticeStorageTest, NoNoticeNameExpectCrash) {
  EXPECT_DEATH_IF_SUPPORTED(
      notice_storage()->SetNoticeShown(prefs(), "Notice1", base::Time()), "");
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateEmitsPromptWaiting) {
  notice_storage()->SetNoticeShown(
      prefs(), kTopicsConsentModal,
      base::Time::FromMillisecondsSinceUnixEpoch(200));

  notice_storage()->RecordHistogramsOnStartup(prefs(), kTopicsConsentModal);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kPromptWaiting, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest, StartupStateEmitsUnknownState) {
  // Migrate actions without shown.
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(NoticeActionTaken::kAck));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken_time"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(200)));
  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  notice_storage()->RecordHistogramsOnStartup(prefs(), notice);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      NoticeStartupState::kUnknownState, 1);
}

class PrivacySandboxNoticeStorageStartupTest
    : public PrivacySandboxNoticeStorageTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<std::pair<NoticeEvent, int>>,
                     NoticeStartupState>> {};

TEST_P(PrivacySandboxNoticeStorageStartupTest, StartupStateEmitsSuccessfully) {
  for (auto event_info : std::get<0>(GetParam())) {
    base::Time timestamp =
        base::Time::FromMillisecondsSinceUnixEpoch(event_info.second);
    if (event_info.first == NoticeEvent::kShown) {
      notice_storage()->SetNoticeShown(prefs(), kTopicsConsentModal, timestamp);
    } else {
      notice_storage()->SetNoticeActionTaken(prefs(), kTopicsConsentModal,
                                             event_info.first, timestamp);
    }
  }

  notice_storage()->RecordHistogramsOnStartup(prefs(), kTopicsConsentModal);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeStartupState.TopicsConsentDesktopModal",
      std::get<1>(GetParam()), 1);
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxNoticeStorageStartupTest,
    PrivacySandboxNoticeStorageStartupTest,
    testing::ValuesIn(
        std::vector<std::tuple<std::vector<std::pair<NoticeEvent, int>>,
                               NoticeStartupState>>{
            // Entry 0.
            {{std::make_pair(NoticeEvent::kShown, 100),
              std::make_pair(NoticeEvent::kClosed, 150)},
             NoticeStartupState::kFlowCompleted},
            // Entry 1.
            {{std::make_pair(NoticeEvent::kShown, 100),
              std::make_pair(NoticeEvent::kSettings, 110),
              std::make_pair(NoticeEvent::kShown, 120),
              std::make_pair(NoticeEvent::kOptIn, 130)},
             NoticeStartupState::kFlowCompletedWithOptIn},
            // Entry 2.
            {{std::make_pair(NoticeEvent::kShown, 100),
              std::make_pair(NoticeEvent::kOptOut, 150)},
             NoticeStartupState::kFlowCompletedWithOptOut},
            // Entry 3.
            {{std::make_pair(NoticeEvent::kShown, 100),
              std::make_pair(NoticeEvent::kAck, 150)},
             NoticeStartupState::kFlowCompleted},
            // Entry 4.
            {{std::make_pair(NoticeEvent::kShown, 100),
              std::make_pair(NoticeEvent::kClosed, 150),
              std::make_pair(NoticeEvent::kShown, 200)},
             NoticeStartupState::kPromptWaiting}}));

TEST_F(PrivacySandboxNoticeStorageTest, SetsValuesAndReadsData) {
  notice_storage()->SetNoticeShown(
      prefs(), kTopicsConsentModal,
      base::Time::FromMillisecondsSinceUnixEpoch(100));
  notice_storage()->SetNoticeActionTaken(
      prefs(), kTopicsConsentModal, NoticeEvent::kAck,
      base::Time::FromMillisecondsSinceUnixEpoch(200));
  const auto actual =
      notice_storage()->ReadNoticeData(prefs(), kTopicsConsentModal);

  EXPECT_EQ(actual->GetNoticeEvents().size(), 2u);
  auto expected = std::make_pair(
      NoticeEvent::kShown, base::Time::FromMillisecondsSinceUnixEpoch(100));
  EXPECT_EQ(actual->GetNoticeEvents()[0], expected);
  expected = std::make_pair(NoticeEvent::kAck,
                            base::Time::FromMillisecondsSinceUnixEpoch(200));
  EXPECT_EQ(actual->GetNoticeEvents()[1], expected);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal",
      NoticeEvent::kAck, 1);
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
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal",
      NoticeEvent::kShown, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       ReActionDoesNotRegisterAndEmitsHistogram) {
  std::string notice = kTopicsConsentModal;
  notice_storage()->SetNoticeShown(
      prefs(), notice, base::Time::FromMillisecondsSinceUnixEpoch(100));
  notice_storage()->SetNoticeActionTaken(
      prefs(), notice, NoticeEvent::kSettings,
      base::Time::FromMillisecondsSinceUnixEpoch(200));

  auto actual = notice_storage()->ReadNoticeData(prefs(), notice);
  auto expected = std::make_pair(
      NoticeEvent::kSettings, base::Time::FromMillisecondsSinceUnixEpoch(200));
  EXPECT_EQ(actual->GetNoticeEvents().size(), 2u);
  EXPECT_EQ(actual->GetNoticeEvents()[1], expected);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal",
      NoticeEvent::kSettings, 1);

  // Tries to override action, should not override and emits histograms.
  notice_storage()->SetNoticeActionTaken(prefs(), notice, NoticeEvent::kAck,
                                         base::Time::Now());
  EXPECT_EQ(actual->GetNoticeEvents().size(), 2u);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal",
      NoticeEvent::kAck, 0);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kAck, 0);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeActionTakenBehavior."
      "TopicsConsentDesktopModal",
      NoticeActionBehavior::kDuplicateActionTaken, 1);
}

TEST_F(PrivacySandboxNoticeStorageTest,
       MultipleNoticeShownValuesRegisterSuccessfully) {
  std::string notice = kTopicsConsentModal;
  notice_storage()->SetNoticeShown(
      prefs(), notice, base::Time::FromMillisecondsSinceUnixEpoch(100));
  notice_storage()->SetNoticeActionTaken(
      prefs(), notice, NoticeEvent::kSettings,
      base::Time::FromMillisecondsSinceUnixEpoch(200));

  auto actual = notice_storage()->ReadNoticeData(prefs(), kTopicsConsentModal);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(100),
            actual->GetNoticeFirstShownFromEvents());
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(100),
            actual->GetNoticeLastShownFromEvents());

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShownForFirstTime.TopicsConsentDesktopModal",
      true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal",
      NoticeEvent::kSettings, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal",
      NoticeEvent::kShown, 1);

  // Set notice shown value again.
  notice_storage()->SetNoticeShown(
      prefs(), notice, base::Time::FromMillisecondsSinceUnixEpoch(250));
  actual = notice_storage()->ReadNoticeData(prefs(), kTopicsConsentModal);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(250),
            actual->GetNoticeLastShownFromEvents());
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShownForFirstTime.TopicsConsentDesktopModal",
      false, 1);
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(100),
            actual->GetNoticeFirstShownFromEvents());
}

TEST_F(PrivacySandboxNoticeStorageTest, SetMultipleNotices) {
  // Notice data 1.
  std::string notice = kTopicsConsentModal;
  notice_storage()->SetNoticeShown(
      prefs(), notice, base::Time::FromMillisecondsSinceUnixEpoch(100));
  notice_storage()->SetNoticeActionTaken(
      prefs(), notice, NoticeEvent::kSettings,
      base::Time::FromMillisecondsSinceUnixEpoch(200));
  const auto actual_notice1 = notice_storage()->ReadNoticeData(prefs(), notice);

  // Notice data 2.
  std::string notice2 = kTopicsConsentModalClankCCT;
  notice_storage()->SetNoticeShown(
      prefs(), notice2, base::Time::FromMillisecondsSinceUnixEpoch(50));
  notice_storage()->SetNoticeActionTaken(
      prefs(), notice2, NoticeEvent::kAck,
      base::Time::FromMillisecondsSinceUnixEpoch(70));
  const auto actual_notice2 =
      notice_storage()->ReadNoticeData(prefs(), notice2);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentDesktopModal",
      NoticeActionTaken::kSettings, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal",
      NoticeEvent::kSettings, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentDesktopModal_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentDesktopModal_Settings",
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentDesktopModal",
      NoticeEvent::kShown, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentDesktopModal", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeAction.TopicsConsentModalClankCCT",
      NoticeActionTaken::kAck, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentModalClankCCT",
      NoticeEvent::kAck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.FirstShownToInteractedDuration."
      "TopicsConsentModalClankCCT_"
      "Ack",
      base::Milliseconds(20), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.Notice.LastShownToInteractedDuration."
      "TopicsConsentModalClankCCT_Ack",
      base::Milliseconds(20), 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeShown.TopicsConsentModalClankCCT", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.Notice.NoticeEvent.TopicsConsentModalClankCCT",
      NoticeEvent::kShown, 1);
}

using NoticeEvents = std::vector<std::pair<NoticeEvent, base::Time>>;
class PrivacySandboxNoticeStorageV2Test
    : public PrivacySandboxNoticeStorageTest {};

TEST_F(PrivacySandboxNoticeStorageV2Test,
       PrivacySandboxMigratePrefsToSchemaV2FlagDisabledDoesNotMigrate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.Reset();
  scoped_feature_list.InitAndDisableFeature(
      kPrivacySandboxMigratePrefsToSchemaV2);
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_last_shown"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(100)));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(NoticeActionTaken::kAck));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken_time"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(200)));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  auto notice_data = notice_storage()->ReadNoticeData(prefs(), notice);
  EXPECT_EQ(notice_data->GetSchemaVersion(), 1);
  NoticeEvents events = notice_data->GetNoticeEvents();

  EXPECT_EQ(events.size(), 0u);
}

TEST_F(PrivacySandboxNoticeStorageV2Test,
       AllEventsPopulatedMigrateSuccessfully) {
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_last_shown"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(100)));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(NoticeActionTaken::kAck));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken_time"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(200)));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  NoticeEvents events =
      notice_storage()->ReadNoticeData(prefs(), notice)->GetNoticeEvents();
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
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_last_shown"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(500)));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  NoticeEvents events =
      notice_storage()->ReadNoticeData(prefs(), notice)->GetNoticeEvents();
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
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(std::get<0>(GetParam())));
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken_time"}),
      base::TimeToValue(base::Time::FromMillisecondsSinceUnixEpoch(200)));
  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  NoticeEvents events =
      notice_storage()->ReadNoticeData(prefs(), notice)->GetNoticeEvents();
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
  std::string notice = kTopicsConsentModal;
  ScopedDictPrefUpdate update(prefs(), "privacy_sandbox.notices");
  update.Get().SetByDottedPath(base::StrCat({notice, ".", "schema_version"}),
                               1);
  update.Get().SetByDottedPath(
      base::StrCat({notice, ".", "notice_action_taken"}),
      static_cast<int>(std::get<0>(GetParam())));

  PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(prefs());

  NoticeEvents events =
      notice_storage()->ReadNoticeData(prefs(), notice)->GetNoticeEvents();
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
