// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/feature_discovery_duration_reporter.h"

#include "ash/public/cpp/feature_discovery_metric_util.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_base.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

// The mock feature's discovery duration histogram.
constexpr char kMockHistogram[] = "FeatureDiscoveryTestMockFeature";

}  // namespace

class FeatureDiscoveryDurationReporterBrowserTest : public LoginManagerTest {
 public:
  FeatureDiscoveryDurationReporterBrowserTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
  }
  FeatureDiscoveryDurationReporterBrowserTest(
      const FeatureDiscoveryDurationReporterBrowserTest&) = delete;
  FeatureDiscoveryDurationReporterBrowserTest& operator=(
      const FeatureDiscoveryDurationReporterBrowserTest&) = delete;
  ~FeatureDiscoveryDurationReporterBrowserTest() override = default;

  // LoginManagerTest:
  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    LoginUser(account_id1_);
    EXPECT_EQ(
        account_id1_,
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  }

  void TearDownOnMainThread() override {
    LoginManagerTest::TearDownOnMainThread();
  }

  AccountId account_id1_;
  AccountId account_id2_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

// Verifies that the feature discovery duration is recorded on a single active
// session.
IN_PROC_BROWSER_TEST_F(FeatureDiscoveryDurationReporterBrowserTest,
                       RecordFeatureDiscovery) {
  base::HistogramTester histogram_tester;
  auto* reporter = ash::FeatureDiscoveryDurationReporter::GetInstance();
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 1);

  // Record the feature discovery for the mock feature again. Verify that
  // duplicate recordings should not be made.
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 1);
}

// Verifies that the feature discovery duration recording works as expected
// when the active account is switched.
IN_PROC_BROWSER_TEST_F(FeatureDiscoveryDurationReporterBrowserTest,
                       RecordFeatureDiscoveryWithAccountSwitch) {
  base::HistogramTester histogram_tester;
  auto* reporter = ash::FeatureDiscoveryDurationReporter::GetInstance();

  // Switch to another account. Because user 2 is not primary, the feature
  // discovery duration should not be recorded.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 0);

  // Switch back to the primary account. Verify that the feature discovery
  // duration is recorded.
  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 1);
}

// Verifies that switching to another account then switching back should resume
// feature discovery duration recordings.
IN_PROC_BROWSER_TEST_F(FeatureDiscoveryDurationReporterBrowserTest,
                       ResumeMetricRecording) {
  base::HistogramTester histogram_tester;
  auto* reporter = ash::FeatureDiscoveryDurationReporter::GetInstance();
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);

  // Switch to another account without ending the observation explicitly.
  // Because user 2 is not primary, the feature discovery duration should not
  // be recorded.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 0);

  // Switch back to the onboarding account. Finish the observation without
  // starting explicitly. Verify that the metric is recorded.
  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 1);
}

IN_PROC_BROWSER_TEST_F(FeatureDiscoveryDurationReporterBrowserTest,
                       PRE_SaveCumulatedTimeWhenSignout) {
  auto* reporter = ash::FeatureDiscoveryDurationReporter::GetInstance();
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);

  // Wait for some time so that the cumulated time is not zero.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();
}

// Verifies that unfinished observations should resume after re-login.
IN_PROC_BROWSER_TEST_F(FeatureDiscoveryDurationReporterBrowserTest,
                       SaveCumulatedTimeWhenSignout) {
  auto* reporter = ash::FeatureDiscoveryDurationReporter::GetInstance();

  // Fetch the cumulated time from the pref service and check it is non-zero.
  PrefService* primary_user_pref =
      ash::ProfileHelper::Get()
          ->GetProfileByUser(user_manager::UserManager::Get()->GetPrimaryUser())
          ->GetPrefs();
  const base::Value* duration_value =
      primary_user_pref->GetDict("FeatureDiscoveryReporterObservedFeatures")
          .FindDict("kMockFeature")
          ->Find("cumulative_duration");
  std::optional<base::TimeDelta> duration =
      base::ValueToTimeDelta(duration_value);
  EXPECT_NE(base::TimeDelta(), *duration);

  // Finish the observation. Verify that the discovery duration is recorded.
  base::HistogramTester histogram_tester;
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 1);
}

}  // namespace ash
