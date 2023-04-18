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
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/profile_ui_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
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

// Updates command line flags to make the test believe that we are on a fresh
// install. Intended to be called from the test body. Note that if a sentinel
// file exists (e.g. a PRE_Test ran) this method might have no effect.
void SetIsFirstRun(bool is_first_run) {
  // We want this to be functional when called from the test body because
  // enabling the FRE to run in the pre-test setup would prevent opening the
  // browser that the test fixtures rely on.
  // So are manipulating flags here instead of during `SetUpX` methods on
  // purpose.
  if (first_run::IsChromeFirstRun() == is_first_run) {
    return;
  }

  if (is_first_run) {
    // This switch is added by InProcessBrowserTest
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);
  } else {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
  }

  first_run::ResetCachedSentinelDataForTesting();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_EQ(is_first_run, first_run::IsChromeFirstRun());
  }
}

bool GetFirstRunFinishedPrefValue() {
  return g_browser_process->local_state()->GetBoolean(prefs::kFirstRunFinished);
}

base::OnceCallback<void(bool)> ExpectProceed(bool expected_proceed_value) {
  return base::BindLambdaForTesting([expected_proceed_value](bool actual) {
    EXPECT_EQ(expected_proceed_value, actual);
  });
}

}  // namespace

class FirstRunServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // We can remove flags and state suppressing the first run only after the
    // browsertest's initial browser is opened. Otherwise we would have to
    // close the FRE and reset its state before each individual test.
    SetIsFirstRun(true);

    // Also make sure we will do another attempt at creating the service now
    // that the first run state changed.
    ASSERT_FALSE(
        FirstRunServiceFactory::GetForBrowserContextIfExists(profile()));
    FirstRunServiceFactory::GetInstance()->Disassociate(profile());

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

  Profile* profile() const { return browser()->profile(); }

  FirstRunService* fre_service() const {
    return FirstRunServiceFactory::GetForBrowserContext(profile());
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
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Only Dice guards the FRE behind a feature flag.
  base::test::ScopedFeatureList scoped_feature_list_{kForYouFre};
#endif
};

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       OpenFirstRunIfNeededOpensPicker) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  bool expected_fre_finished = true;
  bool expected_proceed = false;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  expected_fre_finished = false;  // QuitEarly
#else
  expected_proceed = kForYouFreCloseShouldProceed.Get();
#endif

  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(expected_proceed).Then(run_loop.QuitClosure()));

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
  run_loop.Run();

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

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       CloseChromeWithKeyboardShortcut) {
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(false).Then(run_loop.QuitClosure()));
  profiles::testing::WaitForPickerWidgetCreated();

  ProfilePicker::GetViewForTesting()->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_Q, ui::EF_COMMAND_DOWN));
  histogram_tester.ExpectBucketCount(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kAbandonedFlow, 1);
  profiles::testing::WaitForPickerClosed();
  run_loop.Run();
}
#endif

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       OpenFirstRunIfNeededCalledTwice) {
  // When `OpenFirstRunIfNeeded` is called twice, the callback passed to it the
  // first time should be aborted (called with false) and replaced by the
  // callback passed to it the second time, which will be later called with
  // true on DICE and false on Lacros because it will quit early in the process.
  base::RunLoop first_run_loop;
  base::RunLoop second_run_loop;
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(false).Then(first_run_loop.QuitClosure()));
  profiles::testing::WaitForPickerWidgetCreated();

  bool second_proceed = true;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  second_proceed = false;
#endif
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(second_proceed).Then(second_run_loop.QuitClosure()));
  first_run_loop.Run();

  histogram_tester.ExpectBucketCount(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kAbortTask, 1);

  ProfilePicker::Hide();
  profiles::testing::WaitForPickerClosed();
  second_run_loop.Run();
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

  auto* profile_manager = g_browser_process->profile_manager();
  Profile* primary_profile =
      profile_manager->GetProfile(profile_manager->GetPrimaryUserProfilePath());
  EXPECT_TRUE(ShouldOpenFirstRun(primary_profile));

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

  auto* profile_manager = g_browser_process->profile_manager();
  Profile* primary_profile =
      profile_manager->GetProfile(profile_manager->GetPrimaryUserProfilePath());
  EXPECT_TRUE(ShouldOpenFirstRun(primary_profile));

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
                       FinishedSilentlyDeviceEphemeralUsersEnabled) {
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  base::HistogramTester histogram_tester;

  // The `DeviceEphemeralUsersEnabled` is read through DeviceSettings provided
  // on startup.
  auto init_params = chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->device_settings->device_ephemeral_users_enabled =
      crosapi::mojom::DeviceSettings::OptionalBool::kTrue;
  auto device_settings = init_params->device_settings.Clone();

  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  // TODO(crbug.com/1330310): Ideally this should be done as part of
  // `SetInitParamsForTests()`.
  g_browser_process->browser_policy_connector()
      ->device_settings_lacros()
      ->UpdateDeviceSettings(std::move(device_settings));

  auto* profile_manager = g_browser_process->profile_manager();
  Profile* primary_profile =
      profile_manager->GetProfile(profile_manager->GetPrimaryUserProfilePath());
  EXPECT_TRUE(ShouldOpenFirstRun(primary_profile));

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

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest, ShouldOpenFirstRun) {
  EXPECT_TRUE(ShouldOpenFirstRun(profile()));
  EXPECT_NE(nullptr, fre_service());

  SetIsFirstRun(false);
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));

  SetIsFirstRun(true);
  EXPECT_TRUE(ShouldOpenFirstRun(profile()));

  g_browser_process->local_state()->SetBoolean(prefs::kFirstRunFinished, true);
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest, CompletedOnIntro) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(true).Then(run_loop.QuitClosure()));

  profiles::testing::WaitForPickerWidgetCreated();
  profiles::testing::WaitForPickerLoadStop(GURL(chrome::kChromeUIIntroURL));

  content::WebContents* web_contents =
      ProfilePicker::GetWebViewForTesting()->GetWebContents();
  web_contents->GetWebUI()->ProcessWebUIMessage(
      web_contents->GetURL(), "continueWithoutAccount", base::Value::List());
  profiles::testing::WaitForPickerClosed();
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kCompleted, 1);
}

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

  base::RunLoop run_loop;
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(true).Then(run_loop.QuitClosure()));

  // Opening the FRE triggers recording of the group.
  EXPECT_EQ(kStudyTestGroupName1,
            local_state->GetString(prefs::kFirstRunStudyGroup));
  EXPECT_TRUE(variations::HasSyntheticTrial("ForYouFreSynthetic"));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("ForYouFreSynthetic",
                                                  kStudyTestGroupName1));

  profiles::testing::WaitForPickerWidgetCreated();
  ProfilePicker::Hide();
  profiles::testing::WaitForPickerClosed();
  run_loop.Run();
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

  // However the FRE should be silently marked as finished due to policies
  // forcing to skip it.
  ASSERT_TRUE(fre_service());

  base::RunLoop run_loop;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros the silent finish happens right when the service is created.
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  run_loop.Quit();  // For consistency with the dice code path.
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
  base::RunLoop run_loop;

  ASSERT_TRUE(fre_service());
  EXPECT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(GetParam().expected_proceed).Then(run_loop.QuitClosure()));

  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_FALSE(GetFirstRunFinishedPrefValue());

  ProfilePicker::Hide();
  run_loop.Run();

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
