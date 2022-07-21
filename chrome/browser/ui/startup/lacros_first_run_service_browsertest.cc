// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/startup/lacros_first_run_service.h"

#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/device_settings_lacros.h"
#include "chrome/browser/lacros/lacros_prefs.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/core_account_id.h"

class LacrosFirstRunServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());

    identity_test_env()->SetRefreshTokenForPrimaryAccount();
  }

  void TearDownOnMainThread() override { identity_test_env_adaptor_.reset(); }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&LacrosFirstRunServiceBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  LacrosFirstRunService* fre_service() {
    Profile* profile = browser()->profile();
    return LacrosFirstRunServiceFactory::GetForBrowserContext(profile);
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

IN_PROC_BROWSER_TEST_F(LacrosFirstRunServiceBrowserTest,
                       TryMarkFirstRunAlreadyFinished_DoesNothing) {
  // Setup note: We are removing `switches::kNoFirstRun` only after the browser
  // is opened to simplify the setup. This allows us to call FRE-related methods
  // without having to do more elaborate things to avoid it triggering before
  // the test starts.
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);
  EXPECT_TRUE(fre_service()->ShouldOpenFirstRun());
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  fre_service()->TryMarkFirstRunAlreadyFinished(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(g_browser_process->local_state()->GetBoolean(
      lacros_prefs::kPrimaryProfileFirstRunFinished));
  EXPECT_TRUE(fre_service()->ShouldOpenFirstRun());
  histogram_tester.ExpectTotalCount(
      "Profile.LacrosPrimaryProfileFirstRunOutcome", 0);
}

IN_PROC_BROWSER_TEST_F(LacrosFirstRunServiceBrowserTest,
                       TryMarkFirstRunAlreadyFinished_SucceedsAlreadySignedIn) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);

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
  run_loop.Run();

  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      lacros_prefs::kPrimaryProfileFirstRunFinished));
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedAlreadySyncing, 1);
}

IN_PROC_BROWSER_TEST_F(LacrosFirstRunServiceBrowserTest,
                       TryMarkFirstRunAlreadyFinished_SucceedsSyncRequired) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  base::HistogramTester histogram_tester;

  testing::ScopedSyncRequiredInFirstRun sync_required_override{true};

  base::RunLoop run_loop;
  fre_service()->TryMarkFirstRunAlreadyFinished(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      lacros_prefs::kPrimaryProfileFirstRunFinished));
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
}

IN_PROC_BROWSER_TEST_F(LacrosFirstRunServiceBrowserTest,
                       TryMarkFirstRunAlreadyFinished_SyncConsentDisabled) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  base::HistogramTester histogram_tester;

  profile->GetPrefs()->SetBoolean(prefs::kEnableSyncConsent, false);

  base::RunLoop run_loop;
  fre_service()->TryMarkFirstRunAlreadyFinished(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      lacros_prefs::kPrimaryProfileFirstRunFinished));
  EXPECT_FALSE(ShouldOpenPrimaryProfileFirstRun(profile));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
}

IN_PROC_BROWSER_TEST_F(
    LacrosFirstRunServiceBrowserTest,
    TryMarkFirstRunAlreadyFinished_DeviceEphemeralUsersEnabled) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);
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
      ->device_settings_for_test()
      ->UpdateDeviceSettings(std::move(device_settings));

  base::RunLoop run_loop;
  fre_service()->TryMarkFirstRunAlreadyFinished(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      lacros_prefs::kPrimaryProfileFirstRunFinished));
  EXPECT_FALSE(ShouldOpenPrimaryProfileFirstRun(profile));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester.ExpectUniqueSample(
      "Profile.LacrosPrimaryProfileFirstRunOutcome",
      ProfileMetrics::ProfileSignedInFlowOutcome::kSkippedByPolicies, 1);
}
