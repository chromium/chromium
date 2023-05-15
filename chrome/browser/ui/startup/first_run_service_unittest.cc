// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_service.h"

#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/startup/first_run_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_test_utils.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error "Unsupported platform"
#endif

namespace {

class ScopedTestingMetricsService {
 public:
  // Sets up a `metrics::MetricsService` instance and makes it available in its
  // scope via `testing_browser_process->metrics_service()`.
  //
  // This service only supports feature related to the usage of synthetic field
  // trials.
  //
  // Requires:
  // - the local state prefs to be usable from `testing_browser_process`
  // - a task runner to be available (see //docs/threading_and_tasks_testing.md)
  explicit ScopedTestingMetricsService(
      TestingBrowserProcess* testing_browser_process)
      : browser_process_(testing_browser_process) {
    CHECK(browser_process_);

    auto* local_state = browser_process_->local_state();
    CHECK(local_state)
        << "Error: local state prefs are required. In a unit test, this can be "
           "set up using base::test::ScopedFeatureList.";

    // The `SyntheticTrialsActiveGroupIdProvider` needs to be notified of
    // changes from the registry for them to be used through the variations API.
    synthetic_trial_registry_observation_.Observe(&synthetic_trial_registry_);

    metrics_service_client_.set_synthetic_trial_registry(
        &synthetic_trial_registry_);

    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state, &enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath());

    // Needs to be set up, will be updated at each synthetic trial change.
    variations::InitCrashKeys();

    // Required by `MetricsService` to record UserActions. We don't rely on
    // these here, since we never make it start recording metrics, but the task
    // runner is still required during the shutdown sequence.
    base::SetRecordActionTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());

    metrics_service_ = std::make_unique<metrics::MetricsService>(
        metrics_state_manager_.get(), &metrics_service_client_, local_state);

    browser_process_->SetMetricsService(metrics_service_.get());
  }

  ~ScopedTestingMetricsService() {
    // The scope is closing, undo the set up that was done in the constuctor:
    // `MetricsService` and other necessary parts like crash keys.
    browser_process_->SetMetricsService(nullptr);
    variations::ClearCrashKeysInstanceForTesting();

    // Note: Clears all the synthetic trials, not juste the ones registered
    // during the lifetime of this object.
    variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()
        ->ResetForTesting();
  }

  metrics::MetricsService* Get() { return metrics_service_.get(); }

 private:
  raw_ptr<TestingBrowserProcess> browser_process_ = nullptr;

  metrics::TestEnabledStateProvider enabled_state_provider_{/*consent=*/true,
                                                            /*enabled=*/true};

  variations::SyntheticTrialRegistry synthetic_trial_registry_;
  base::ScopedObservation<variations::SyntheticTrialRegistry,
                          variations::SyntheticTrialObserver>
      synthetic_trial_registry_observation_{
          variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()};

  metrics::TestMetricsServiceClient metrics_service_client_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  std::unique_ptr<metrics::MetricsService> metrics_service_;
};

struct FirstRunFieldTrialTestParams {
  double entropy_value;
  version_info::Channel channel;

  bool expect_study_enabled;
  bool expect_feature_enabled;
};

}  // namespace

class FirstRunServiceTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(FirstRunServiceTest, ShouldOpenFirstRun) {
  TestingProfileManager profile_manager{TestingBrowserProcess::GetGlobal()};
  ASSERT_TRUE(profile_manager.SetUp());

  auto* profile = profile_manager.CreateTestingProfile("Test Profile");
  EXPECT_TRUE(ShouldOpenFirstRun(profile));

  SetIsFirstRun(false);
  EXPECT_FALSE(ShouldOpenFirstRun(profile));

  SetIsFirstRun(true);
  EXPECT_TRUE(ShouldOpenFirstRun(profile));

  g_browser_process->local_state()->SetBoolean(prefs::kFirstRunFinished, true);
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
}

class FirstRunFieldTrialCreatorTest
    : public testing::Test,
      public testing::WithParamInterface<FirstRunFieldTrialTestParams> {
 public:
  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(FirstRunFieldTrialCreatorTest, SetUpFromClientSide) {
  {
    base::MockEntropyProvider low_entropy_provider{GetParam().entropy_value};
    auto feature_list = std::make_unique<base::FeatureList>();

    FirstRunService::SetUpClientSideFieldTrial(
        low_entropy_provider, feature_list.get(), GetParam().channel);

    // Substitute the existing feature list with the one with field trial
    // configurations we are testing, so we can check the assertions.
    scoped_feature_list().InitWithFeatureList(std::move(feature_list));
  }

  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("ForYouFreStudy"));

  EXPECT_EQ(GetParam().expect_study_enabled,
            base::FeatureList::IsEnabled(kForYouFreSyntheticTrialRegistration));
  EXPECT_EQ(GetParam().expect_feature_enabled,
            base::FeatureList::IsEnabled(kForYouFre));

  EXPECT_EQ(true, kForYouFreCloseShouldProceed.Get());
  EXPECT_EQ(SigninPromoVariant::kSignIn, kForYouFreSignInPromoVariant.Get());
  EXPECT_EQ(GetParam().expect_study_enabled
                ? (GetParam().expect_feature_enabled ? "ClientSideEnabled-2"
                                                     : "ClientSideDisabled-2")
                : "",
            kForYouFreStudyGroup.Get());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FirstRunFieldTrialCreatorTest,
    testing::Values(
        FirstRunFieldTrialTestParams{.entropy_value = 0.6,
                                     .channel = version_info::Channel::BETA,
                                     .expect_study_enabled = true,
                                     .expect_feature_enabled = false},
        FirstRunFieldTrialTestParams{.entropy_value = 0.01,
                                     .channel = version_info::Channel::BETA,
                                     .expect_study_enabled = true,
                                     .expect_feature_enabled = true},
        FirstRunFieldTrialTestParams{.entropy_value = 0.99,
                                     .channel = version_info::Channel::STABLE,
                                     .expect_study_enabled = false,
                                     .expect_feature_enabled = false},
        FirstRunFieldTrialTestParams{.entropy_value = 0.016,
                                     .channel = version_info::Channel::STABLE,
                                     .expect_study_enabled = true,
                                     .expect_feature_enabled = false},
        FirstRunFieldTrialTestParams{.entropy_value = 0.009,
                                     .channel = version_info::Channel::STABLE,
                                     .expect_study_enabled = true,
                                     .expect_feature_enabled = true}),

    [](const ::testing::TestParamInfo<FirstRunFieldTrialTestParams>& params) {
      return base::StringPrintf(
          "%02.0fpctEntropy%s", params.param.entropy_value * 100,
          version_info::GetChannelString(params.param.channel).data());
    });

// Tests to verify the logic for synthetic trial registration that we use to
// assign a given client in a cohort for our long term tracking metrics.
class FirstRunCohortSetupTest : public testing::Test {
 public:
  static constexpr char kStudyTestGroupName1[] = "test_group_1";
  static constexpr char kStudyTestGroupName2[] = "test_group_2";

 private:
  base::test::TaskEnvironment task_environment_;
  ScopedTestingLocalState testing_local_state_{
      TestingBrowserProcess::GetGlobal()};
  ScopedTestingMetricsService testing_metrics_service_{
      TestingBrowserProcess::GetGlobal()};
};

// `JoinFirstRunCohort` is run when the FRE is finished, if a group name is
// provided through the feature flags, should result in registering the
// synthetic trial with that group name and store it for subsequent startups.
TEST_F(FirstRunCohortSetupTest, JoinFirstRunCohort) {
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kFirstRunStudyGroup));
  EXPECT_FALSE(
      variations::HasSyntheticTrial(FirstRunService::kSyntheticTrialName));

  // No group name available through the features, should no-op.
  FirstRunService::JoinFirstRunCohort();
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kFirstRunStudyGroup));
  EXPECT_FALSE(
      variations::HasSyntheticTrial(FirstRunService::kSyntheticTrialName));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {kForYouFreSyntheticTrialRegistration,
           {{"group_name", kStudyTestGroupName1}}},
          {kForYouFre, {}},
      },
      /*disabled_features=*/{});

  // A group name is available, the trial should get registered.
  FirstRunService::JoinFirstRunCohort();
  EXPECT_EQ(kStudyTestGroupName1,
            local_state->GetString(prefs::kFirstRunStudyGroup));
  EXPECT_TRUE(
      variations::HasSyntheticTrial(FirstRunService::kSyntheticTrialName));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      FirstRunService::kSyntheticTrialName, kStudyTestGroupName1));
}

// `EnsureStickToFirstRunCohort` is run on startup, and should result in
// registering the synthetic trial if the client saw the FRE and we recorded a
// group name to assign to it.
TEST_F(FirstRunCohortSetupTest, EnsureStickToFirstRunCohort) {
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kFirstRunStudyGroup));
  EXPECT_FALSE(
      variations::HasSyntheticTrial(FirstRunService::kSyntheticTrialName));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {kForYouFreSyntheticTrialRegistration,
           {{"group_name", kStudyTestGroupName1}}},
          {kForYouFre, {}},
      },
      /*disabled_features=*/{});

  // `EnsureStickToFirstRunCohort()` no-ops without some specific prefs.
  FirstRunService::EnsureStickToFirstRunCohort();
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kFirstRunStudyGroup));
  EXPECT_FALSE(
      variations::HasSyntheticTrial(FirstRunService::kSyntheticTrialName));

  // Setting the group name pref: the first, but not sufficient requirement.
  // We also set it to a different name from the feature flag to verify which
  // one is used.
  local_state->SetString(prefs::kFirstRunStudyGroup, kStudyTestGroupName2);
  FirstRunService::EnsureStickToFirstRunCohort();
  EXPECT_FALSE(
      variations::HasSyntheticTrial(FirstRunService::kSyntheticTrialName));

  // Marking the FRE finished: the second and final requirement.
  local_state->SetBoolean(prefs::kFirstRunFinished, true);
  FirstRunService::EnsureStickToFirstRunCohort();
  EXPECT_TRUE(
      variations::HasSyntheticTrial(FirstRunService::kSyntheticTrialName));

  // The registered group is read from the prefs, not from the feature param.
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      FirstRunService::kSyntheticTrialName, kStudyTestGroupName2));
  EXPECT_FALSE(variations::IsInSyntheticTrialGroup(
      FirstRunService::kSyntheticTrialName, kStudyTestGroupName1));
}
