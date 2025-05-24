// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_service.h"

#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/startup/first_run_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
static base::Value GetJSONAsValue(std::string_view json) {
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

  // TODO(crbug.com/40839518): Needed because SyncService startup hangs
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
  expected_proceed = true;

  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(proceed_future.GetCallback());

  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_FALSE(GetFirstRunFinishedPrefValue());

  histogram_tester.ExpectUniqueSample("ProfilePicker.FirstRun.ServiceCreated",
                                      true, 1);

  ProfilePicker::Hide();
  EXPECT_EQ(expected_proceed, proceed_future.Get());

  EXPECT_EQ(expected_fre_finished, GetFirstRunFinishedPrefValue());
  EXPECT_NE(expected_fre_finished, fre_service()->ShouldOpenFirstRun());
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kForYouFre, 1);
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
  // true on DICE .
  base::test::TestFuture<bool> first_proceed_future;
  base::test::TestFuture<bool> second_proceed_future;
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(first_proceed_future.GetCallback());
  profiles::testing::WaitForPickerWidgetCreated();

  bool expected_second_proceed = true;
  fre_service()->OpenFirstRunIfNeeded(second_proceed_future.GetCallback());
  EXPECT_FALSE(first_proceed_future.Get());

  histogram_tester.ExpectBucketCount(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kAbortTask, 1);

  ProfilePicker::Hide();
  profiles::testing::WaitForPickerClosed();
  EXPECT_EQ(expected_second_proceed, second_proceed_future.Get());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest, CloseProceeds) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> proceed_future;

  ASSERT_TRUE(fre_service());
  EXPECT_TRUE(fre_service()->ShouldOpenFirstRun());
  fre_service()->OpenFirstRunIfNeeded(proceed_future.GetCallback());

  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_FALSE(GetFirstRunFinishedPrefValue());

  ProfilePicker::Hide();
  EXPECT_TRUE(proceed_future.Get());

  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());

  // We log `QuitAtEnd`, whether proceed is overridden or not.
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FirstRun.ExitStatus",
      ProfilePicker::FirstRunExitStatus::kQuitAtEnd, 1);
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
    {.key = policy::key::kBrowserSignin, .value = "0"},
    {.key = policy::key::kBrowserSignin, .value = "1", .should_open_fre = true},
#if !BUILDFLAG(IS_LINUX)
    {.key = policy::key::kBrowserSignin, .value = "2"},
#endif  // BUILDFLAG(IS_LINUX)
    {.key = policy::key::kPromotionalTabsEnabled, .value = "false"},
};

std::string PolicyParamToTestSuffix(
    const ::testing::TestParamInfo<PolicyTestParam>& info) {
  std::string force_signin_profile_picker_feature;
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
  base::test::ScopedFeatureList scoped_feature_list_;

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
  fre_service()->OpenFirstRunIfNeeded(
      base::IgnoreArgs<bool>(run_loop.QuitClosure()));
  EXPECT_EQ(GetParam().should_open_fre, ProfilePicker::IsOpen());

  EXPECT_NE(GetParam().should_open_fre, GetFirstRunFinishedPrefValue());

  ProfilePicker::Hide();
  run_loop.Run();

  std::optional<std::u16string> expected_profile_name;
  // On Dice platforms, we use a default enterprise name after skipped FREs.
  //
  // If force sign in is active and through the profile picker, the profile
  // finalisation is not expected to happen, so the default name should remain.
  if (!GetParam().should_open_fre && !signin_util::IsForceSigninEnabled()) {
    expected_profile_name = l10n_util::GetStringUTF16(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_PROFILE_NAME);
  }

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
