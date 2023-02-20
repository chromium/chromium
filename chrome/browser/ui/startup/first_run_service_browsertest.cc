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
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  if (first_run::IsChromeFirstRun() == is_first_run)
    return;

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
    // We can remove flags and state suppressing the first run, only after the
    // browsertest's initial browser is opened.
    SetIsFirstRun(true);

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
                       OpenFirstRunIfNeeded_OpensPicker) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  bool expected_fre_finished = true;
  bool expected_proceed = false;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  expected_fre_finished = false;  // QuitEarly
#else
  expected_proceed = kForYouFreCloseShouldProceed.Get();
#endif

  EXPECT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(expected_proceed).Then(run_loop.QuitClosure()));

  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_FALSE(GetFirstRunFinishedPrefValue());

  ProfilePicker::Hide();
  run_loop.Run();

  EXPECT_EQ(expected_fre_finished, GetFirstRunFinishedPrefValue());
  EXPECT_NE(expected_fre_finished, fre_service()->ShouldOpenFirstRun());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  histogram_tester.ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunOutcome", 0);
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       OpenFirstRunIfNeeded_AlreadySyncing) {
  SetIsFirstRun(true);

  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(account_id.empty());
  identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSync);
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(true).Then(run_loop.QuitClosure()));
  // Future attempts are synchronously disabled.
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  run_loop.Run();

  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedAlreadySyncing, 1);
}

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       OpenFirstRunIfNeeded_SyncConsentDisabled) {
  SetIsFirstRun(true);
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  base::HistogramTester histogram_tester;

  profile()->GetPrefs()->SetBoolean(prefs::kEnableSyncConsent, false);
  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  base::RunLoop run_loop;
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(true).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));
  run_loop.Run();

  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
}

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       OpenFirstRunIfNeeded_DeviceEphemeralUsersEnabled) {
  SetIsFirstRun(true);
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

  base::RunLoop run_loop;
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      ExpectProceed(true).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));
  run_loop.Run();

  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest, ShouldOpenFirstRun) {
  EXPECT_TRUE(ShouldOpenFirstRun(profile()));

  SetIsFirstRun(false);
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));

  SetIsFirstRun(true);
  EXPECT_TRUE(ShouldOpenFirstRun(profile()));

  g_browser_process->local_state()->SetBoolean(prefs::kFirstRunFinished, true);
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));
}

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
                       ShouldOpenFirstRun_NeverOnDice) {
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));
  EXPECT_EQ(nullptr, fre_service());

  SetIsFirstRun(true);
  EXPECT_FALSE(ShouldOpenFirstRun(profile()));
}
#endif

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
  base::RunLoop run_loop;
  fre_service()->OpenFirstRunIfNeeded(
      FirstRunService::EntryPoint::kOther,
      base::IgnoreArgs<bool>(run_loop.QuitClosure()));

  EXPECT_EQ(GetParam().should_open_fre, ProfilePicker::IsOpen());
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
  base::RunLoop run_loop;

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
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunServiceFeatureParamsBrowserTest,
                         testing::ValuesIn(kFeatureTestParams),
                         &FeatureParamToTestSuffix);
#endif
