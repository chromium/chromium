// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_service.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/profile_ui_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/device_settings_lacros.h"
#include "chrome/browser/lacros/lacros_prefs.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif

namespace {
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
    EXPECT_TRUE(first_run::IsChromeFirstRun());
  }
}
}  // namespace

class FirstRunServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());

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

  FirstRunService* fre_service() {
    Profile* profile = browser()->profile();
    return FirstRunServiceFactory::GetForBrowserContext(profile);
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
                       TryMarkFirstRunAlreadyFinished_DoesNothing) {
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  SetIsFirstRun(true);
  EXPECT_TRUE(fre_service()->ShouldOpenFirstRun());
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  fre_service()->TryMarkFirstRunAlreadyFinished(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(
      g_browser_process->local_state()->GetBoolean(prefs::kFirstRunFinished));
  EXPECT_TRUE(fre_service()->ShouldOpenFirstRun());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  histogram_tester.ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunOutcome", 0);
#endif
}

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       TryMarkFirstRunAlreadyFinished_NotFirstRun) {
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  SetIsFirstRun(false);
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  EXPECT_FALSE(
      g_browser_process->local_state()->GetBoolean(prefs::kFirstRunFinished));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       TryMarkFirstRunAlreadyFinished_SucceedsAlreadySyncing) {
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
  fre_service()->TryMarkFirstRunAlreadyFinished(run_loop.QuitClosure());
  // Future attempts are synchronously disabled.
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  run_loop.Run();

  EXPECT_TRUE(
      g_browser_process->local_state()->GetBoolean(prefs::kFirstRunFinished));
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedAlreadySyncing, 1);
}

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest,
                       TryMarkFirstRunAlreadyFinished_SyncConsentDisabled) {
  SetIsFirstRun(true);
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  base::HistogramTester histogram_tester;

  profile->GetPrefs()->SetBoolean(prefs::kEnableSyncConsent, false);

  base::RunLoop run_loop;
  fre_service()->TryMarkFirstRunAlreadyFinished(run_loop.QuitClosure());
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
  run_loop.Run();

  EXPECT_TRUE(
      g_browser_process->local_state()->GetBoolean(prefs::kFirstRunFinished));
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
}

IN_PROC_BROWSER_TEST_F(
    FirstRunServiceBrowserTest,
    TryMarkFirstRunAlreadyFinished_DeviceEphemeralUsersEnabled) {
  SetIsFirstRun(true);
  Profile* profile = browser()->profile();
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
  fre_service()->TryMarkFirstRunAlreadyFinished(run_loop.QuitClosure());
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
  run_loop.Run();

  EXPECT_TRUE(
      g_browser_process->local_state()->GetBoolean(prefs::kFirstRunFinished));
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest, ShouldOpenFirstRun) {
  EXPECT_FALSE(ShouldOpenFirstRun(browser()->profile()));
  SetIsFirstRun(true);
  EXPECT_TRUE(ShouldOpenFirstRun(browser()->profile()));

  g_browser_process->local_state()->SetBoolean(prefs::kFirstRunFinished, true);
  EXPECT_FALSE(ShouldOpenFirstRun(browser()->profile()));
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
  EXPECT_FALSE(ShouldOpenFirstRun(browser()->profile()));
  EXPECT_EQ(nullptr, fre_service());

  SetIsFirstRun(true);
  EXPECT_FALSE(ShouldOpenFirstRun(browser()->profile()));
}
#endif

IN_PROC_BROWSER_TEST_F(FirstRunServiceBrowserTest, OpenFirstRunIfNeeded) {
  SetIsFirstRun(true);

  fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                      base::DoNothing());

  profiles::testing::WaitForPickerWidgetCreated();

  // TODO(crbug.com/1375277): Check that the callback is run on closure.

  // TODO(crbug.com/1375277): Check the logic that makes the FRE run only once.
  EXPECT_TRUE(ShouldOpenFirstRun(browser()->profile()));
}
