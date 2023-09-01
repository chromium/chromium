// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_service.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/startup/first_run_test_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/device_settings_lacros.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif

namespace {
class PolicyUpdateObserver : public policy::PolicyService::Observer {
 public:
  PolicyUpdateObserver(policy::PolicyService& policy_service,
                       base::OnceClosure policy_updated_callback)
      : policy_service_(policy_service),
        policy_updated_callback_(std::move(policy_updated_callback)) {
    DCHECK(policy_updated_callback_);
    policy_service_->AddObserver(policy::PolicyDomain::POLICY_DOMAIN_CHROME,
                                 this);
  }

  ~PolicyUpdateObserver() override {
    policy_service_->RemoveObserver(policy::PolicyDomain::POLICY_DOMAIN_CHROME,
                                    this);
  }

  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override {
    if (ns.domain != policy::PolicyDomain::POLICY_DOMAIN_CHROME) {
      return;
    }

    policy_service_->RemoveObserver(policy::PolicyDomain::POLICY_DOMAIN_CHROME,
                                    this);
    std::move(policy_updated_callback_).Run();
  }

  const raw_ref<policy::PolicyService> policy_service_;
  base::OnceClosure policy_updated_callback_;
};

// Converts JSON string to `base::Value` object.
static base::Value GetJSONAsValue(base::StringPiece json) {
  std::string error;
  auto value = JSONStringValueDeserializer(json).Deserialize(nullptr, &error);
  EXPECT_EQ("", error);
  return base::Value::FromUniquePtrValue(std::move(value));
}

}  // namespace

class FirstRunServiceBrowserTest : public FirstRunServiceBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    FirstRunServiceBrowserTestBase::SetUpOnMainThread();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    identity_test_env()->SetRefreshTokenForPrimaryAccount();
#endif
  }

  void TearDownOnMainThread() override { identity_test_env_adaptor_.reset(); }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &FirstRunServiceBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;

  // TODO(https://crbug.com/1324886): Needed because SyncService startup hangs
  // otherwise. Find a way to get it not to hang instead?
  profiles::testing::ScopedNonEnterpriseDomainSetterForTesting
      non_enterprise_domain_setter_;
};

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       OpenFirstRunIfNeededOpensPicker) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> proceed_future;
  bool expected_fre_finished = true;
  bool expected_proceed = false;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  expected_fre_finished = false;  // QuitEarly
#else
  expected_proceed = kForYouFreCloseShouldProceed.Get();
#endif

  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      proceed_future.GetCallback());

  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_FALSE(GetFirstRunFinishedPrefValue());

  histogram_tester.ExpectUniqueSample("ProfilePicker.FirstRun.ServiceCreated",
                                      true, 1);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunEntryPoint",
      FirstRunService::EntryPoint::kOther, 1);
#endif
  histogram_tester.ExpectUniqueSample("ProfilePicker.FirstRun.EntryPoint",
                                      FirstRunService::EntryPoint::kOther, 1);

  // We don't expect synthetic trials to be registered here, since no group
  // is configured with the feature. For the positive test case, see
  // `FirstRunServiceCohortBrowserTest.GroupRegisteredAfterFre`.
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kFirstRunStudyGroup));
  EXPECT_FALSE(variations::HasSyntheticTrial("ForYouFreSynthetic"));

  ProfilePicker::Hide();
  EXPECT_EQ(expected_proceed, proceed_future.Get());

  EXPECT_EQ(expected_fre_finished, GetFirstRunFinishedPrefValue());
  EXPECT_NE(expected_fre_finished, fre_service()->ShouldOpenFirstRun());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  histogram_tester.ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunOutcome", 0);
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kQuitEarly, 1);
  histogram_tester.ExpectTotalCount("ProfilePicker.FirstRun.FinishReason", 0);
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kQuitAtEnd, 1);
  histogram_tester.ExpectUniqueSample("ProfilePicker.FirstRun.FinishReason",
                                      /*kFinishedFlow*/ 1, 1);
#endif
}

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       OpenFirstRunIfNeededCalledTwice) {
  // When `OpenFirstRunIfNeeded` is called twice, the callback passed to it the
  // first time should be aborted (called with false) and replaced by the
  // callback passed to it the second time, which will be later called with
  // true on DICE and false on Lacros because it will quit early in the process.
  base::test::TestFuture<bool> first_proceed_future;
  base::test::TestFuture<bool> second_proceed_future;
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      first_proceed_future.GetCallback());
  profiles::testing::WaitForPickerWidgetCreated();

  bool expected_second_proceed = true;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  expected_second_proceed = false;
#endif
  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      second_proceed_future.GetCallback());
  EXPECT_FALSE(first_proceed_future.Get());

  histogram_tester.ExpectBucketCount(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kAbortTask, 1);

  ProfilePicker::Hide();
  profiles::testing::WaitForPickerClosed();
  EXPECT_EQ(expected_second_proceed, second_proceed_future.Get());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       FinishedSilentlyAlreadySyncing) {
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(account_id.empty());
  identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSync);
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(profile()->IsMainProfile());
  EXPECT_TRUE(ShouldOpenFirstRun(profile()));

  ASSERT_TRUE(fre_service());

  // The FRE should be finished silently during the creation of the service.
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedAlreadySyncing, 1);
}

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       FinishedSilentlySyncConsentDisabled) {
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  base::HistogramTester histogram_tester;

  profile()->GetPrefs()->SetBoolean(prefs::kEnableSyncConsent, false);
  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  ASSERT_TRUE(profile()->IsMainProfile());
  EXPECT_TRUE(ShouldOpenFirstRun(profile()));

  ASSERT_TRUE(fre_service());

  // The FRE should be finished silently during the creation of the service.
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
}

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       FinishedSilentlyIsCurrentUserEphemeral) {
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  base::HistogramTester histogram_tester;

  // Setup the ephemeral for Lacros.
  auto init_params = chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->is_current_user_ephemeral = true;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

  ASSERT_TRUE(profile()->IsMainProfile());
  EXPECT_TRUE(ShouldOpenFirstRun(profile()));

  ASSERT_TRUE(fre_service());

  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

class FirstRunServiceNotForYouBrowserTest : public FirstRunServiceBrowserTest {
 public:
  FirstRunServiceNotForYouBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(kForYouFre);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FirstRunServiceNotForYouBrowserTest,
                       ShouldOpenFirstRunNeverOnDice) {
  // Even though the FRE could be open, we should not create the service for it.
  EXPECT_TRUE(ShouldOpenFirstRun(profile()));
  EXPECT_EQ(nullptr, fre_service());
}

class FirstRunServiceCohortBrowserTest : public FirstRunServiceBrowserTest {
 public:
  static constexpr char kStudyTestGroupName1[] = "test_group_1";
  static constexpr char kStudyTestGroupName2[] = "test_group_2";

  FirstRunServiceCohortBrowserTest() {
    variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()
        ->ResetForTesting();

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kForYouFreSyntheticTrialRegistration,
             {{"group_name", kStudyTestGroupName1}}},
            {kForYouFre, {}},
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FirstRunServiceCohortBrowserTest,
                       PRE_GroupRegisteredAfterFre) {
  EXPECT_TRUE(ShouldOpenFirstRun(browser()->profile()));

  // We don't expect the synthetic trial to be registered before the FRE runs.
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kFirstRunStudyGroup));
  EXPECT_FALSE(variations::HasSyntheticTrial("ForYouFreSynthetic"));

  base::test::TestFuture<bool> proceed_future;
  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      proceed_future.GetCallback());

  // Opening the FRE triggers recording of the group.
  EXPECT_EQ(kStudyTestGroupName1,
            local_state->GetString(prefs::kFirstRunStudyGroup));
  EXPECT_TRUE(variations::HasSyntheticTrial("ForYouFreSynthetic"));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("ForYouFreSynthetic",
                                                  kStudyTestGroupName1));

  profiles::testing::WaitForPickerWidgetCreated();
  ProfilePicker::Hide();
  profiles::testing::WaitForPickerClosed();
  EXPECT_TRUE(proceed_future.Get());
}
IN_PROC_BROWSER_TEST_F(FirstRunServiceCohortBrowserTest,
                       GroupRegisteredAfterFre) {
  EXPECT_FALSE(ShouldOpenFirstRun(browser()->profile()));

  PrefService* local_state = g_browser_process->local_state();
  EXPECT_EQ(kStudyTestGroupName1,
            local_state->GetString(prefs::kFirstRunStudyGroup));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("ForYouFreSynthetic",
                                                  kStudyTestGroupName1));
}

IN_PROC_BROWSER_TEST_F(FirstRunServiceCohortBrowserTest,
                       PRE_PRE_GroupViaPrefs) {
  // Setting the pref, we expect it to get picked up in an upcoming startup.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kFirstRunStudyGroup, kStudyTestGroupName2);

  EXPECT_FALSE(variations::HasSyntheticTrial("ForYouFreSynthetic"));
}
IN_PROC_BROWSER_TEST_F(FirstRunServiceCohortBrowserTest, PRE_GroupViaPrefs) {
  // The synthetic group should not be registered yet since we didn't go through
  // the FRE.
  EXPECT_FALSE(variations::HasSyntheticTrial("ForYouFreSynthetic"));

  // Setting this should make the next run finally register the synthetic trial.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kFirstRunFinished, true);
}
IN_PROC_BROWSER_TEST_F(FirstRunServiceCohortBrowserTest, GroupViaPrefs) {
  EXPECT_TRUE(variations::HasSyntheticTrial("ForYouFreSynthetic"));
  // The registered group is read from the prefs, not from the feature param.
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("ForYouFreSynthetic",
                                                  kStudyTestGroupName2));
}

class FirstRunServiceControlBrowserTest : public FirstRunServiceBrowserTest {
 public:
  static constexpr char kStudyTestGroupName[] = "control";

  FirstRunServiceControlBrowserTest() {
    variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()
        ->ResetForTesting();

    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kForYouFreSyntheticTrialRegistration,
             {{"group_name", kStudyTestGroupName}}},
        },
        {kForYouFre});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};
IN_PROC_BROWSER_TEST_F(FirstRunServiceControlBrowserTest, PRE_Control) {
  EXPECT_EQ(nullptr, FirstRunServiceFactory::GetForBrowserContext(profile()));

  // The FRE is directly marked finished and we join the indicated cohort.
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_TRUE(local_state->GetBoolean(prefs::kFirstRunFinished));
  EXPECT_EQ(kStudyTestGroupName,
            local_state->GetString(prefs::kFirstRunStudyGroup));

  EXPECT_TRUE(variations::HasSyntheticTrial("ForYouFreSynthetic"));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("ForYouFreSynthetic",
                                                  kStudyTestGroupName));
}
IN_PROC_BROWSER_TEST_F(FirstRunServiceControlBrowserTest, Control) {
  EXPECT_EQ(nullptr, FirstRunServiceFactory::GetForBrowserContext(profile()));

  // On subsequent startups, we continue the registration.
  EXPECT_TRUE(variations::HasSyntheticTrial("ForYouFreSynthetic"));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("ForYouFreSynthetic",
                                                  kStudyTestGroupName));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

struct PolicyTestParam {
  const std::string test_suffix;
  const std::string key;
  const std::string value;  // As JSON string, base::Value is not copy-friendly.
  const bool should_open_fre = false;
};

const PolicyTestParam kPolicyTestParams[] = {
    {.key = policy::key::kSyncDisabled, .value = "true"},
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    {.key = policy::key::kBrowserSignin, .value = "0"},
    {.key = policy::key::kBrowserSignin, .value = "1", .should_open_fre = true},
#if !BUILDFLAG(IS_LINUX)
    {.key = policy::key::kBrowserSignin, .value = "2"},
#endif  // BUILDFLAG(IS_LINUX)
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    {.key = policy::key::kPromotionalTabsEnabled, .value = "false"},
};

std::string PolicyParamToTestSuffix(
    const ::testing::TestParamInfo<PolicyTestParam>& info) {
  return info.param.key + "_" + info.param.value;
}

class FirstRunServicePolicyBrowserTest
    : public FirstRunServiceBrowserTest,
      public testing::WithParamInterface<PolicyTestParam> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    FirstRunServiceBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetPolicy(const std::string& key, const std::string& value) {
    auto* policy_service = g_browser_process->policy_service();
    ASSERT_TRUE(policy_service);

    policy::PolicyMap policy;
    policy.Set(key, policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_PLATFORM, GetJSONAsValue(value), nullptr);

    base::RunLoop run_loop;
    PolicyUpdateObserver policy_update_observer{*policy_service,
                                                run_loop.QuitClosure()};

    policy_provider_.UpdateChromePolicy(policy);

    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{kForYouFre};

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(FirstRunServicePolicyBrowserTest, OpenFirstRunIfNeeded) {
  base::HistogramTester histogram_tester;

  signin_util::ResetForceSigninForTesting();
  SetPolicy(GetParam().key, GetParam().value);

  // The attempt to run the FRE should not be blocked
  EXPECT_TRUE(ShouldOpenFirstRun(browser()->profile()));
  EXPECT_TRUE(IsProfileNameDefault());

  // However the FRE should be silently marked as finished due to policies
  // forcing to skip it.
  ASSERT_TRUE(fre_service());

  base::RunLoop run_loop;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros the silent finish happens right when the service is created.
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());

  // Quitting the loop for consistency with the dice code path. Posting the task
  // is important to get the profile name resolution's timeout task to run
  // before the assertions below.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
#else
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      base::IgnoreArgs<bool>(run_loop.QuitClosure()));
  EXPECT_EQ(GetParam().should_open_fre, ProfilePicker::IsOpen());
#endif

  EXPECT_NE(GetParam().should_open_fre, GetFirstRunFinishedPrefValue());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (GetParam().should_open_fre) {
    histogram_tester.ExpectTotalCount(
        "Profile.LacrosPrimaryProfileFirstRunOutcome", 0);
  } else {
    histogram_tester.ExpectUniqueSample(
        "Profile.LacrosPrimaryProfileFirstRunOutcome",
        ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  ProfilePicker::Hide();
  run_loop.Run();

  absl::optional<std::u16string> expected_profile_name;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros we always have an account, the profile name will reflect it.
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  expected_profile_name = base::ASCIIToUTF16(account_info.email);
#else
  // On Dice platforms, we use a default enterprise name after skipped FREs.
  if (!GetParam().should_open_fre) {
    expected_profile_name = l10n_util::GetStringUTF16(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_PROFILE_NAME);
  }
#endif

  if (expected_profile_name.has_value()) {
    EXPECT_EQ(*expected_profile_name, GetProfileName());
  } else {
    EXPECT_TRUE(IsProfileNameDefault());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunServicePolicyBrowserTest,
                         testing::ValuesIn(kPolicyTestParams),
                         &PolicyParamToTestSuffix);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
struct FeatureTestParams {
  const base::FieldTrialParams feature_params;
  const bool expected_proceed;
};

const FeatureTestParams kFeatureTestParams[] = {
    {.feature_params = {{"close_should_proceed", "false"}},
     .expected_proceed = false},
    {.feature_params = {{"close_should_proceed", "true"}},
     .expected_proceed = true},
};

std::string FeatureParamToTestSuffix(
    const ::testing::TestParamInfo<FeatureTestParams>& info) {
  std::vector<std::string> pieces;
  for (const auto& feature_param : info.param.feature_params) {
    pieces.push_back(feature_param.first);
    pieces.push_back(feature_param.second);
  }

  return base::JoinString(pieces, "_");
}

class FirstRunServiceFeatureParamsBrowserTest
    : public FirstRunServiceBrowserTest,
      public testing::WithParamInterface<FeatureTestParams> {
 public:
  FirstRunServiceFeatureParamsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kForYouFre, GetParam().feature_params);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(FirstRunServiceFeatureParamsBrowserTest, CloseProceeds) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> proceed_future;

  ASSERT_TRUE(fre_service());
  EXPECT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      proceed_future.GetCallback());

  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_FALSE(GetFirstRunFinishedPrefValue());

  ProfilePicker::Hide();
  EXPECT_EQ(GetParam().expected_proceed, proceed_future.Get());

  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());

  // We log `QuitAtEnd`, whether proceed is overridden or not.
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kQuitAtEnd, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunServiceFeatureParamsBrowserTest,
                         testing::ValuesIn(kFeatureTestParams),
                         &FeatureParamToTestSuffix);
#endif
