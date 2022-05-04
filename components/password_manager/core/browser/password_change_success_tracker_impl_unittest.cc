// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"

#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using password_manager::PasswordChangeMetricsRecorder;
using password_manager::PasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTrackerImpl;
using testing::_;
using testing::StrictMock;

constexpr char kUrl1[] = "https://www.example.com";
constexpr char kUrl2[] = "https://www.example.co.uk";
constexpr char kUrl2WithPath[] = "https://www.example.co.uk/login.php";
constexpr char kUsername1[] = "Paul";
constexpr char kUsername2[] = "Lori";

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

  void FastForwardBy(base::TimeDelta time_step) {
    task_environment_.FastForwardBy(time_step);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<PasswordChangeSuccessTrackerImpl>
      password_change_success_tracker_;
  raw_ptr<MockPasswordChangeMetricsRecorder> metrics_recorder_;
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

  const base::Value* value = pref_service_.Get(
      password_manager::prefs::kPasswordChangeSuccessTrackerFlows);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->GetList().size(), 2u);

  std::unique_ptr<PasswordChangeSuccessTracker>
      password_change_success_tracker_ =
          std::make_unique<PasswordChangeSuccessTrackerImpl>(&pref_service_);

  // Version has been updated and old records have been deleted.
  absl::optional<int> version = pref_service_.GetInteger(
      password_manager::prefs::kPasswordChangeSuccessTrackerVersion);
  ASSERT_TRUE(version);
  EXPECT_EQ(version.value(), PasswordChangeSuccessTrackerImpl::kTrackerVersion);

  value = pref_service_.Get(
      password_manager::prefs::kPasswordChangeSuccessTrackerFlows);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->GetList().size(), 0u);
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
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowGeneratedPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));
  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen);
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen);
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
          kAutomatedFlowGeneratedPasswordChosen);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::kAutomatedFlowOwnPasswordChosen);
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
          kAutomatedFlowGeneratedPasswordChosen);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl2)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl2), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kAutomatedFlowOwnPasswordChosen);
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
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kManualWellKnownUrlFlow,
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen);
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
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowGeneratedPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::
          kAutomatedFlowGeneratedPasswordChosen);
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
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowResetLinkRequestRequested,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowModified(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::StartEvent::kManualResetLinkFlow);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kManualResetLinkFlow,
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));

  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername2,
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen);
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
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
          PasswordChangeSuccessTracker::EndEvent::
              kAutomatedFlowResetLinkRequestRequested,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));
  tracker()->OnChangePasswordFlowModified(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::StartEvent::kManualResetLinkFlow);

  EXPECT_CALL(
      *metrics_recorder(),
      OnFlowRecorded(
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::kManualResetLinkFlow,
          PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings, _));
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::kManualFlowOwnPasswordChosen);
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
          kManualFlowGeneratedPasswordChosen);
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
          PasswordChangeSuccessTrackerImpl::ExtractEtld1(GURL(kUrl1)),
          PasswordChangeSuccessTracker::StartEvent::
              kManualChangePasswordUrlFlow,
          PasswordChangeSuccessTracker::EndEvent::kTimeout,
          PasswordChangeSuccessTracker::EntryPoint::kLeakWarningDialog, _));
  tracker()->OnChangePasswordFlowCompleted(
      GURL(kUrl1), kUsername1,
      PasswordChangeSuccessTracker::EndEvent::
          kManualFlowGeneratedPasswordChosen);
}
