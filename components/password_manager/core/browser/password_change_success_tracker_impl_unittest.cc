// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using password_manager::PasswordChangeMetricsRecorder;
using password_manager::PasswordChangeMetricsRecorderUkm;
using password_manager::PasswordChangeMetricsRecorderUma;
using password_manager::PasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTrackerImpl;
using testing::_;
using testing::StrictMock;
using UkmEntry = ukm::builders::PasswordManager_PasswordChangeFlowDuration;

constexpr char kUrl1[] = "https://www.example.com";
constexpr char kEtldPlus1[] = "example.com";
constexpr char kUrl2[] = "https://www.example.co.uk";
constexpr char kUrl2WithPath[] = "https://www.example.co.uk/login.php";
constexpr char kUsername1[] = "Paul";
constexpr char kUsername2[] = "Lori";
constexpr bool kNotPhished = false;

namespace {

void RegisterPasswordChangeSuccessTrackerPreferences(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      password_manager::prefs::kPasswordChangeSuccessTrackerVersion, 0);
  registry->RegisterListPref(
      password_manager::prefs::kPasswordChangeSuccessTrackerFlows);
}

class MockPasswordChangeMetricsRecorder
    : public password_manager::PasswordChangeMetricsRecorder {
 public:
  MockPasswordChangeMetricsRecorder() = default;
  ~MockPasswordChangeMetricsRecorder() override = default;

  MOCK_METHOD(void,
              OnFlowRecorded,
              (const std::string& url,
               PasswordChangeSuccessTracker::StartEvent start_event,
               PasswordChangeSuccessTracker::EndEvent end_event,
               PasswordChangeSuccessTracker::EntryPoint entry_point,
               base::TimeDelta duration),
              (override));
};

}  // namespace

// Tests of |PasswordChangeMetricsRecorderUma|.
class PasswordChangeMetricsRecorderUmaTest : public ::testing::Test {
 public:
  PasswordChangeMetricsRecorderUmaTest() = default;
  ~PasswordChangeMetricsRecorderUmaTest() override = default;

 protected:
  const base::HistogramTester& histogram_tester() { return histogram_tester_; }
  PasswordChangeMetricsRecorderUma& recorder() { return recorder_; }

 private:
  base::HistogramTester histogram_tester_;
  PasswordChangeMetricsRecorderUma recorder_;
};

TEST_F(PasswordChangeMetricsRecorderUmaTest, RecordSingleMetricsEvent) {
  constexpr PasswordChangeSuccessTracker::StartEvent start_event =
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow;
  constexpr PasswordChangeSuccessTracker::EndEvent end_event =
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen;
  constexpr PasswordChangeSuccessTracker::EntryPoint entry_point =
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog;

  recorder().OnFlowRecorded(kEtldPlus1, start_event, end_event, entry_point,
                            base::Seconds(30));

  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.PasswordChangeFlowDuration.LeakWarningDialog."
      "AutomatedFlow",
      base::Seconds(30), 1);

  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.PasswordChangeFlowDuration.LeakWarningDialog."
      "AutomatedFlow.AutomatedFlowPasswordChosen",
      base::Seconds(30), 1);
}

TEST_F(PasswordChangeMetricsRecorderUmaTest, RecordMultipleMetricsEvents) {
  constexpr PasswordChangeSuccessTracker::StartEvent start_event1 =
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow;
  constexpr PasswordChangeSuccessTracker::EndEvent end_event1 =
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen;
  constexpr PasswordChangeSuccessTracker::EndEvent end_event2 =
      PasswordChangeSuccessTracker::EndEvent::
          kManualFlowGeneratedPasswordChosen;
  constexpr PasswordChangeSuccessTracker::EntryPoint entry_point1 =
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog;

  recorder().OnFlowRecorded(kEtldPlus1, start_event1, end_event1, entry_point1,
                            base::Seconds(30));
  recorder().OnFlowRecorded(kEtldPlus1, start_event1, end_event2, entry_point1,
                            base::Seconds(30));

  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.PasswordChangeFlowDuration.LeakWarningDialog."
      "AutomatedFlow",
      base::Seconds(30), 2);

  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.PasswordChangeFlowDuration.LeakWarningDialog."
      "AutomatedFlow.AutomatedFlowPasswordChosen",
      base::Seconds(30), 1);

  histogram_tester().ExpectUniqueTimeSample(
      "PasswordManager.PasswordChangeFlowDuration.LeakWarningDialog."
      "AutomatedFlow.ManualFlowPasswordChosen",
      base::Seconds(30), 1);
}

TEST_F(PasswordChangeMetricsRecorderUmaTest,
       RecordMultipleMetricsEventsWithDifferentDurations) {
  constexpr PasswordChangeSuccessTracker::StartEvent start_event =
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow;
  constexpr PasswordChangeSuccessTracker::EndEvent end_event =
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen;
  constexpr PasswordChangeSuccessTracker::EntryPoint entry_point =
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings;

  const base::TimeDelta duration1 = base::Seconds(30);
  const base::TimeDelta duration2 = base::Minutes(30);

  recorder().OnFlowRecorded(kEtldPlus1, start_event, end_event, entry_point,
                            duration1);
  recorder().OnFlowRecorded(kEtldPlus1, start_event, end_event, entry_point,
                            duration2);

  histogram_tester().ExpectTimeBucketCount(
      "PasswordManager.PasswordChangeFlowDuration.LeakCheckInSettings."
      "AutomatedFlow",
      duration1, 1);
  histogram_tester().ExpectTimeBucketCount(
      "PasswordManager.PasswordChangeFlowDuration.LeakCheckInSettings."
      "AutomatedFlow",
      duration2, 1);
  histogram_tester().ExpectTotalCount(
      "PasswordManager.PasswordChangeFlowDuration.LeakCheckInSettings."
      "AutomatedFlow",
      2);

  histogram_tester().ExpectTimeBucketCount(
      "PasswordManager.PasswordChangeFlowDuration.LeakCheckInSettings."
      "AutomatedFlow."
      "ManualFlowPasswordChosen",
      duration1, 1);
  histogram_tester().ExpectTimeBucketCount(
      "PasswordManager.PasswordChangeFlowDuration.LeakCheckInSettings."
      "AutomatedFlow.ManualFlowPasswordChosen",
      duration2, 1);
  histogram_tester().ExpectTotalCount(
      "PasswordManager.PasswordChangeFlowDuration.LeakCheckInSettings."
      "AutomatedFlow.ManualFlowPasswordChosen",
      2);
}

// Tests of |PasswordChangeMetricsRecorderUkm|.
class PasswordChangeMetricsRecorderUkmTest : public ::testing::Test {
 public:
  PasswordChangeMetricsRecorderUkmTest() = default;
  ~PasswordChangeMetricsRecorderUkmTest() override = default;

 protected:
  const ukm::TestAutoSetUkmRecorder& ukm_tester() { return test_ukm_recorder_; }
  PasswordChangeMetricsRecorderUkm& recorder() { return recorder_; }

 private:
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  // The object to test.
  PasswordChangeMetricsRecorderUkm recorder_;
};

TEST_F(PasswordChangeMetricsRecorderUkmTest, RecordSingleMetricsEvent) {
  constexpr PasswordChangeSuccessTracker::StartEvent start_event =
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow;
  constexpr PasswordChangeSuccessTracker::EndEvent end_event =
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen;
  constexpr PasswordChangeSuccessTracker::EntryPoint entry_point =
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog;

  recorder().OnFlowRecorded(kEtldPlus1, start_event, end_event, entry_point,
                            base::Seconds(30));

  // Check that UKM logging is correct.
  const auto& entries = ukm_tester().GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(entry->source_id, ukm::NoURLSourceId());
    ukm_tester().ExpectEntryMetric(entry, UkmEntry::kStartEventName,
                                   static_cast<int64_t>(start_event));
    ukm_tester().ExpectEntryMetric(entry, UkmEntry::kEndEventName,
                                   static_cast<int64_t>(end_event));
    ukm_tester().ExpectEntryMetric(entry, UkmEntry::kEntryPointName,
                                   static_cast<int64_t>(entry_point));
    // Exponential bucketing maps 30 seconds to the 29 second bucket.
    ukm_tester().ExpectEntryMetric(entry, UkmEntry::kDurationName, 29);
  }
}

TEST_F(PasswordChangeMetricsRecorderUkmTest,
       RecordSingleMetricsEventWithTimeout) {
  constexpr PasswordChangeSuccessTracker::StartEvent start_event =
      PasswordChangeSuccessTracker::StartEvent::kManualChangePasswordUrlFlow;
  constexpr PasswordChangeSuccessTracker::EndEvent end_event =
      PasswordChangeSuccessTracker::EndEvent::kTimeout;
  constexpr PasswordChangeSuccessTracker::EntryPoint entry_point =
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings;

  recorder().OnFlowRecorded(kEtldPlus1, start_event, end_event, entry_point,
                            PasswordChangeSuccessTracker::kFlowTimeout);

  // Check that UKM logging is correct.
  const auto& entries = ukm_tester().GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(entry->source_id, ukm::NoURLSourceId());
    ukm_tester().ExpectEntryMetric(entry, UkmEntry::kStartEventName,
                                   static_cast<int64_t>(start_event));
    ukm_tester().ExpectEntryMetric(entry, UkmEntry::kEndEventName,
                                   static_cast<int64_t>(end_event));
    ukm_tester().ExpectEntryMetric(entry, UkmEntry::kEntryPointName,
                                   static_cast<int64_t>(entry_point));
    // With a bucket spacing of 1.1, 3600 seconds are mapped to the bucket
    // with 3299 seconds.
    ukm_tester().ExpectEntryMetric(entry, UkmEntry::kDurationName, 3299);
  }
}

// Tests of |PasswordChangeSuccessTrackerImpl|.
class PasswordChangeSuccessTrackerImplTest : public ::testing::Test {
 public:
  PasswordChangeSuccessTrackerImplTest() {
    RegisterPasswordChangeSuccessTrackerPreferences(pref_service_.registry());

    password_change_success_tracker_ =
        std::make_unique<PasswordChangeSuccessTrackerImpl>(&pref_service_);

    auto recorder =
        std::make_unique<StrictMock<MockPasswordChangeMetricsRecorder>>();
    metrics_recorder_ = recorder.get();
    password_change_success_tracker_->AddMetricsRecorder(std::move(recorder));
  }

  ~PasswordChangeSuccessTrackerImplTest() override = default;

 protected:
  PrefService* pref_service() { return &pref_service_; }

  PasswordChangeSuccessTracker* tracker() {
    return password_change_success_tracker_.get();
  }

  MockPasswordChangeMetricsRecorder* metrics_recorder() {
    return metrics_recorder_;
  }

  void AddMetricsRecorder(
      std::unique_ptr<PasswordChangeMetricsRecorder> recorder) {
    password_change_success_tracker_->AddMetricsRecorder(std::move(recorder));
  }

  void FastForwardBy(base::TimeDelta time_step) {
    task_environment_.FastForwardBy(time_step);
  }

  const base::UserActionTester& user_action_tester() {
    return user_action_tester_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<PasswordChangeSuccessTrackerImpl>
      password_change_success_tracker_;
  raw_ptr<MockPasswordChangeMetricsRecorder> metrics_recorder_;
  base::UserActionTester user_action_tester_;
};

TEST(PasswordChangeSuccessTrackerImpl, DeletedOutdatedEventRecords) {
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  RegisterPasswordChangeSuccessTrackerPreferences(pref_service_.registry());

  // Set an outdated version that contains flows.
  pref_service_.SetInteger(
      password_manager::prefs::kPasswordChangeSuccessTrackerVersion, 0);

  base::Value::List flows;
  flows.Append(base::Value::Dict());
  flows.Append(base::Value::Dict());
  pref_service_.SetList(
      password_manager::prefs::kPasswordChangeSuccessTrackerFlows,
      std::move(flows));

  {
    const base::Value::List& value = pref_service_.GetList(
        password_manager::prefs::kPasswordChangeSuccessTrackerFlows);
    EXPECT_EQ(value.size(), 2u);
  }

  std::unique_ptr<PasswordChangeSuccessTracker>
      password_change_success_tracker_ =
          std::make_unique<PasswordChangeSuccessTrackerImpl>(&pref_service_);

  // Version has been updated and old records have been deleted.
  absl::optional<int> version = pref_service_.GetInteger(
      password_manager::prefs::kPasswordChangeSuccessTrackerVersion);
  ASSERT_TRUE(version);
  EXPECT_EQ(version.value(), PasswordChangeSuccessTrackerImpl::kTrackerVersion);

  {
    const base::Value::List& value = pref_service_.GetList(
        password_manager::prefs::kPasswordChangeSuccessTrackerFlows);
    EXPECT_TRUE(value.empty());
  }
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       SuccessfulAutomatedFlowFromSettings) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowGeneratedPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));
  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen,
      kNotPhished);
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
      kNotPhished);
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       SuccessfulAutomatedFlowFromLeakWarning) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);

  // This flow completion cannot be matched due to a different username,
  // so there is no call to the recorder.
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen,
      kNotPhished);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::kAutomatedFlowOwnPasswordChosen,
      kNotPhished);
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       SuccessfulAutomatedFlowWithChangedUrl) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2WithPath), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);

  // This flow completion cannot be matched due to a different url,
  // so there is no call to the recorder.
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen,
      kNotPhished);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kAutomatedFlowOwnPasswordChosen,
      kNotPhished);
}

TEST_F(PasswordChangeSuccessTrackerImplTest, SuccessfulManualFlows) {
  tracker()->OnManualChangePasswordFlowStarted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);

  tracker()->OnChangePasswordFlowModified(
      GURL(kUrl2),
      PasswordChangeSuccessTracker::StartEvent::kManualHomepageFlow);
  tracker()->OnChangePasswordFlowModified(
      GURL(kUrl1),
      PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow);
  tracker()->OnChangePasswordFlowModified(
      GURL(kUrl1),
      PasswordChangeSuccessTracker::StartEvent::kManualHomepageFlow);

  // The first candidate with matching url is used.
  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow,
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
      kNotPhished);
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       SuccessfulFlowSeveralMatchingCandidates) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  // The second entry should be matched. Since there cannot be simultaneous
  // automated flows, we assume implicitly that the first one would have been
  // abandoned.
  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowGeneratedPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen,
      kNotPhished);
}

TEST_F(PasswordChangeSuccessTrackerImplTest, AutomatedFlowEndsInPasswordReset) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  // There are two calls: One to terminate the automated change flow and one
  // to record the password reset flow.
  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowResetLinkRequested,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowModified(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kManualResetLinkFlow);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kManualResetLinkFlow,
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
      kNotPhished);
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       PasswordResetSeveralMatchingCandidates) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  // The second entry should be matched. Since there cannot be simultaneous
  // automated flows, we assume implicitly that the first one would have been
  // abandoned.
  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowResetLinkRequested,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));
  tracker()->OnChangePasswordFlowModified(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::StartEvent::kManualResetLinkFlow);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kManualResetLinkFlow,
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
      kNotPhished);
}

TEST_F(PasswordChangeSuccessTrackerImplTest, TimeoutForIncompleteFlow) {
  tracker()->OnManualChangePasswordFlowStarted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);

  FastForwardBy(2 * PasswordChangeSuccessTracker::kFlowTypeRefinementTimeout);

  tracker()->OnChangePasswordFlowModified(
      GURL(kUrl1),
      PasswordChangeSuccessTracker::StartEvent::kManualChangePasswordUrlFlow);

  // We expect no call.
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::
          kManualFlowGeneratedPasswordChosen,
      kNotPhished);
}

TEST_F(PasswordChangeSuccessTrackerImplTest, TimeoutForFlow) {
  tracker()->OnManualChangePasswordFlowStarted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);
  tracker()->OnChangePasswordFlowModified(
      GURL(kUrl1),
      PasswordChangeSuccessTracker::StartEvent::kManualChangePasswordUrlFlow);

  FastForwardBy(2 * PasswordChangeSuccessTracker::kFlowTimeout);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::
              kManualChangePasswordUrlFlow,
          PasswordChangeSuccessTracker::EndEvent::kTimeout,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::
          kManualFlowGeneratedPasswordChosen,
      kNotPhished);
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       IntegrationTestWithMetricsRecorderUma) {
  base::HistogramTester histogram_tester;

  // Manually add the Uma recorder.
  AddMetricsRecorder(std::make_unique<PasswordChangeMetricsRecorderUma>());

  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2WithPath), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);

  // This flow completion cannot be matched due to a different url,
  // so there is no call to the recorder.
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen,
      kNotPhished);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kAutomatedFlowOwnPasswordChosen,
      kNotPhished);

  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordChangeFlowDuration.LeakWarningDialog."
      "AutomatedFlow",
      1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordChangeFlowDuration.LeakWarningDialog."
      "AutomatedFlow.AutomatedFlowPasswordChosen",
      1);
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       IntegrationTestWithMetricsRecorderUkm) {
  ukm::TestAutoSetUkmRecorder ukm_tester;

  // Manually add the Ukm recorder.
  AddMetricsRecorder(std::make_unique<PasswordChangeMetricsRecorderUkm>());

  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2WithPath), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog);

  // This flow completion cannot be matched due to a different url,
  // so there is no call to the recorder.
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen,
      kNotPhished);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kAutomatedFlowOwnPasswordChosen,
      kNotPhished);

  // Check that UKM logging is correct.
  const auto& entries = ukm_tester.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(entry->source_id, ukm::NoURLSourceId());
    ukm_tester.ExpectEntryMetric(
        entry, UkmEntry::kStartEventName,
        static_cast<int64_t>(
            PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow));
    ukm_tester.ExpectEntryMetric(
        entry, UkmEntry::kEndEventName,
        static_cast<int64_t>(PasswordChangeSuccessTracker::EndEvent::
                                 kAutomatedFlowOwnPasswordChosen));
    ukm_tester.ExpectEntryMetric(
        entry, UkmEntry::kEntryPointName,
        static_cast<int64_t>(
            PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog));
  }
}
TEST_F(PasswordChangeSuccessTrackerImplTest,
       AutomatedFlowOwnPasswordUserActionForPhishedPassword) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kAutomatedFlowOwnPasswordChosen,
      /* phished= */ true);

  EXPECT_EQ(
      1, user_action_tester().GetActionCount(
             "PasswordProtection.PasswordUpdated.AutomatedFlowPasswordChosen"));
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       AutomatedFlowGeneratedPasswordUserActionForPhishedPassword) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowGeneratedPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen,
      /* phished= */ true);

  EXPECT_EQ(
      1, user_action_tester().GetActionCount(
             "PasswordProtection.PasswordUpdated.AutomatedFlowPasswordChosen"));
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       AutomatedFlowResetPasswordUserActionForPhishedPassword) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowResetLinkRequested,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kAutomatedFlowResetLinkRequested,
      /* phished= */ true);

  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "PasswordProtection.PasswordUpdated."
                   "AutomatedFlowResetLinkRequested"));
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       ManualFlowGeneratedPasswordUserActionForPhishedPassword) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kManualChangePasswordUrlFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::
              kManualChangePasswordUrlFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kManualFlowGeneratedPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::
          kManualFlowGeneratedPasswordChosen,
      /* phished= */ true);

  EXPECT_EQ(1,
            user_action_tester().GetActionCount(
                "PasswordProtection.PasswordUpdated.ManualFlowPasswordChosen"));
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       ManualFlowOwnPasswordUserActionForPhishedPassword) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kManualChangePasswordUrlFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::
              kManualChangePasswordUrlFlow,
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
      /* phished= */ true);

  EXPECT_EQ(1,
            user_action_tester().GetActionCount(
                "PasswordProtection.PasswordUpdated.ManualFlowPasswordChosen"));
}

TEST_F(PasswordChangeSuccessTrackerImplTest,
       TimeoutUserActionForPhishedPassword) {
  tracker()->OnChangePasswordFlowStarted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kManualChangePasswordUrlFlow,
      PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtldPlus1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::
              kManualChangePasswordUrlFlow,
          PasswordChangeSuccessTracker::EndEvent::kTimeout,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2, PasswordChangeSuccessTracker::EndEvent::kTimeout,
      /* phished= */ true);

  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "PasswordProtection.PasswordUpdated.Timeout"));
}