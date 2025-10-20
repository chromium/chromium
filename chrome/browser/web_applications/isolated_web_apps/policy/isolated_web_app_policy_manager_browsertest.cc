// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_iwa_installer_factory.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/profile_waiter.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/features.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

const web_package::test::Ed25519KeyPair kPublicKeyPair1 =
    web_package::test::Ed25519KeyPair::CreateRandom();
const web_package::test::Ed25519KeyPair kPublicKeyPair2 =
    web_package::test::Ed25519KeyPair::CreateRandom();
const web_package::SignedWebBundleId kWebBundleId1 =
    web_package::SignedWebBundleId::CreateForPublicKey(
        kPublicKeyPair1.public_key);
const web_package::SignedWebBundleId kWebBundleId2 =
    web_package::SignedWebBundleId::CreateForPublicKey(
        kPublicKeyPair2.public_key);

const UpdateChannel kBetaChannel = UpdateChannel::Create("beta").value();
constexpr std::string kPinnedVersion = "1.0.0";
constexpr char kOrphanedBundleDirectory[] = "6zsr4hjoudsu6ihf";

using policy::DeveloperToolsPolicyHandler;

using UpdateDiscoveryTaskFuture =
    base::test::TestFuture<IsolatedWebAppUpdateDiscoveryTask::CompletionStatus>;

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kUserMail[] = "dla@example.com";
constexpr char kDisplayName[] = "display name";

void WaitForProfile() {
  ProfileWaiter waiter;
  waiter.WaitForProfileAdded();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
using IsolatedWebAppPolicyManagerTestHarness = ash::LoginManagerTest;
#else
using IsolatedWebAppPolicyManagerTestHarness =
    web_app::IsolatedWebAppBrowserTestHarness;
#endif  // BUILDFLAG(IS_CHROMEOS)

class IsolatedWebAppPolicyManagerBrowserTestBase
    : public IsolatedWebAppPolicyManagerTestHarness {
 public:
  IsolatedWebAppPolicyManagerBrowserTestBase(
      const IsolatedWebAppPolicyManagerBrowserTestBase&) = delete;
  IsolatedWebAppPolicyManagerBrowserTestBase& operator=(
      const IsolatedWebAppPolicyManagerBrowserTestBase&) = delete;

 protected:
  explicit IsolatedWebAppPolicyManagerBrowserTestBase(bool is_user_session)
      : is_user_session_(is_user_session) {
#if BUILDFLAG(IS_CHROMEOS)
    if (is_user_session_) {
      login_manager_mixin_.AppendRegularUsers(1);
    }
#else
    EXPECT_TRUE(is_user_session_);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void SetUpOnMainThread() override {
    IsolatedWebAppPolicyManagerTestHarness::SetUpOnMainThread();
    AddInitialBundles();
  }

  void TearDownOnMainThread() override {
    // Each session start, IWA cache manager checks for the updates. Wait for
    // this result to avoid crashes in tests.
    WaitForInitialUpdateDiscoveryTasksToFinish();
    IsolatedWebAppPolicyManagerTestHarness::TearDownOnMainThread();
  }

  void WaitForInitialUpdateDiscoveryTasksToFinish() {
    for (auto& update_future : initial_discovery_update_futures_) {
      EXPECT_TRUE(update_future.Wait());
    }
    initial_discovery_update_futures_.clear();
    initial_discovery_update_waiters_.clear();
  }

  const webapps::AppId kAppId1 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(kWebBundleId1)
          .app_id();
  const webapps::AppId kAppId2 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(kWebBundleId2)
          .app_id();

  void AddInitialBundles() {
    iwa_test_update_server_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(kPublicKeyPair1));
    iwa_test_update_server_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("7.0.6"))
            .BuildBundle(kPublicKeyPair1));
    iwa_test_update_server_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("9.0.0"))
            .BuildBundle(kPublicKeyPair1),
        std::vector<UpdateChannel>{kBetaChannel});

    iwa_test_update_server_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("2.0.0"))
            .BuildBundle(kPublicKeyPair2));
    iwa_test_update_server_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.2.0"))
            .BuildBundle(kPublicKeyPair2),
        std::vector<UpdateChannel>{kBetaChannel});
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedWebAppPolicyManagerTestHarness::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  void SetUpInProcessBrowserTestFixture() override {
    IsolatedWebAppPolicyManagerTestHarness::SetUpInProcessBrowserTestFixture();

    if (is_user_session_) {
      policy_provider_.SetDefaultReturns(
          /*is_initialization_complete_return=*/true,
          /*is_first_policy_load_complete_return=*/true);
      policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
          &policy_provider_);
    } else {
#if BUILDFLAG(IS_CHROMEOS)
      // Turning on device local account.
      device_policy()->policy_data().set_public_key_version(1);
      policy::DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
          &device_local_account_policy_, kUserMail, kDisplayName);
#else
      NOTREACHED();
#endif  // BUILDFLAG(IS_CHROMEOS)
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  void UploadAndInstallDeviceLocalAccountPolicy() {
    // Build device local account policy.
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();

    policy_test_server_mixin_.UpdatePolicy(
        policy::dm_protocol::kChromePublicAccountPolicyType, kUserMail,
        device_local_account_policy_.payload().SerializeAsString());

    session_manager_client()->set_device_local_account_policy(
        kUserMail, device_local_account_policy_.GetBlob());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  void AddUser() {
    if (!is_user_session_) {
#if BUILDFLAG(IS_CHROMEOS)
      AddManagedGuestSessionToDevicePolicy();
#else
      NOTREACHED();
#endif  // BUILDFLAG(IS_CHROMEOS)
    }
    // No user needs to be created for user session: the user was already
    // added in the constructor (technical constraint).
  }

  void WaitForUserAdded() {
    if (is_user_session_) {
      return;
    }

#if BUILDFLAG(IS_CHROMEOS)
    UploadAndInstallDeviceLocalAccountPolicy();
    WaitForPolicy();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void InstallOneApp() {
    if (is_user_session_) {
      SetPolicyWithOneApp();
      return;
    }
    AddDeviceLocalAccountIwaPolicy();
  }

#if BUILDFLAG(IS_CHROMEOS)
  void AddManagedGuestSessionToDevicePolicy() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    policy::DeviceLocalAccountTestHelper::AddPublicSession(&proto, kUserMail);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // This policy is active at the moment of login.
  void AddDeviceLocalAccountIwaPolicy() {
    em::StringPolicyProto* const isolated_web_apps_proto =
        device_local_account_policy_.payload()
            .mutable_isolatedwebappinstallforcelist();

    isolated_web_apps_proto->set_value(
        WriteJson(base::Value::List().Append(
                      iwa_test_update_server_.CreateForceInstallPolicyEntry(
                          kWebBundleId1)))
            .value());
  }

  void SetIwaForceInstallPolicy(base::Value::List update_manifest_entries) {
    if (is_user_session_) {
      policy::PolicyMap policies;
      policies.Set(policy::key::kIsolatedWebAppInstallForceList,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(update_manifest_entries.Clone()), nullptr);
      policy_provider_.UpdateChromePolicy(policies);
    } else {
      GetProfileForTest()->GetPrefs()->SetList(
          prefs::kIsolatedWebAppInstallForceList,
          std::move(update_manifest_entries));
    }
  }

  void SetIwaAllowlist(
      const std::vector<web_package::SignedWebBundleId>& managed_allowlist) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_THAT(test::UpdateKeyDistributionInfoWithAllowlist(
                    base::Version("1.0.0"), std::move(managed_allowlist)),
                base::test::HasValue());
  }

  void SetPolicyWithOneApp() {
    SetIwaForceInstallPolicy(base::Value::List().Append(
        iwa_test_update_server_.CreateForceInstallPolicyEntry(kWebBundleId1)));
  }

  void SetPolicyWithTwoApps() {
    SetIwaForceInstallPolicy(
        base::Value::List()
            .Append(iwa_test_update_server_.CreateForceInstallPolicyEntry(
                kWebBundleId1))
            .Append(iwa_test_update_server_.CreateForceInstallPolicyEntry(
                kWebBundleId2)));
  }

  void SetPolicyWithOneAppWithPinnedVersion(
      std::string pinned_version = kPinnedVersion) {
    SetIwaForceInstallPolicy(base::Value::List().Append(
        iwa_test_update_server_.CreateForceInstallPolicyEntry(
            kWebBundleId1, /*update_channel=*/std::nullopt,
            *IwaVersion::Create(pinned_version))));
  }

  void SetPolicyWithBetaChannelApp(
      const web_package::SignedWebBundleId& web_bundle_id) {
    SetIwaForceInstallPolicy(base::Value::List().Append(
        iwa_test_update_server_.CreateForceInstallPolicyEntry(web_bundle_id,
                                                              {kBetaChannel})));
  }

  IwaVersion GetIsolatedWebAppVersion(const webapps::AppId& app_id) {
    return provider()
        .registrar_unsafe()
        .GetAppById(app_id)
        ->isolation_data()
        ->version();
  }

  // Returns a profile which can be used for testing.
  Profile* GetProfileForTest() {
#if BUILDFLAG(IS_CHROMEOS)
    // Any profile can be used here since this test does not test multi profile.
    return ProfileManager::GetActiveUserProfile();
#else
    return profile();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void WaitForPolicy() {
#if BUILDFLAG(IS_CHROMEOS)
    // Wait for the display name becoming available as that indicates
    // device-local account policy is fully loaded, which is a prerequisite for
    // successful login.
    policy::DictionaryLocalStateValueWaiter("UserDisplayName", kDisplayName,
                                            account_id_.GetUserEmail())
        .Wait();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void StartLogin(const std::vector<webapps::AppId>&
                      wait_for_initial_update_for_apps = {}) {
#if BUILDFLAG(IS_CHROMEOS)
    if (is_user_session_) {
      LoginUser(login_manager_mixin_.users()[0].account_id);
    } else {
      // Start login into the device-local account.
      auto* host = ash::LoginDisplayHost::default_host();
      ASSERT_TRUE(host);
      host->StartSignInScreen();
      auto* controller = ash::ExistingUserController::current_controller();
      ASSERT_TRUE(controller);

      ash::UserContext user_context(user_manager::UserType::kPublicAccount,
                                    account_id_);
      controller->Login(user_context, ash::SigninSpecifics());

      if (!wait_for_initial_update_for_apps.empty()) {
        WaitForProfile();
        CreateInitialDiscoveryUpdateWaiters(wait_for_initial_update_for_apps);
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void CreateInitialDiscoveryUpdateWaiters(const webapps::AppId& app_id) {
    CreateInitialDiscoveryUpdateWaiters(std::vector<webapps::AppId>{app_id});
  }

  void CreateInitialDiscoveryUpdateWaiters(
      const std::vector<webapps::AppId>& app_ids) {
    // The initial update is checked on the session start only  inside Managed
    // Guest Session and kiosk.
    if (is_user_session_) {
      return;
    }
    for (const auto& app_id : app_ids) {
      initial_discovery_update_futures_.emplace_back();
      initial_discovery_update_waiters_.push_back(
          std::make_unique<UpdateDiscoveryTaskResultWaiter>(
              provider(), app_id,
              initial_discovery_update_futures_.back().GetCallback()));
    }
  }

  void WaitForSessionStart() {
#if BUILDFLAG(IS_CHROMEOS)
    if (session_manager::SessionManager::Get()->IsSessionStarted()) {
      return;
    }
    if (ash::WizardController::default_controller()) {
      ash::WizardController::default_controller()
          ->SkipPostLoginScreensForTesting();
    }
    ash::test::WaitForPrimaryUserSessionStart();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

#if BUILDFLAG(IS_CHROMEOS)
  void RefreshDevicePolicy() { policy_helper_.RefreshDevicePolicy(); }

  policy::DevicePolicyBuilder* device_policy() {
    return policy_helper_.device_policy();
  }

  ash::FakeSessionManagerClient* session_manager_client() {
    return ash::FakeSessionManagerClient::Get();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  WebAppProvider& provider() {
    CHECK(GetProfileForTest());
    auto* provider = WebAppProvider::GetForTest(GetProfileForTest());
    CHECK(provider);
    return *provider;
  }

  policy::UserPolicyBuilder device_local_account_policy_;
  const bool is_user_session_;

 private:
#if BUILDFLAG(IS_CHROMEOS)
  const AccountId account_id_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kUserMail,
          policy::DeviceLocalAccountType::kPublicSession));
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_};
  policy::DevicePolicyCrosTestHelper policy_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_;
  IsolatedWebAppTestUpdateServer iwa_test_update_server_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::vector<UpdateDiscoveryTaskFuture> initial_discovery_update_futures_;
  std::vector<std::unique_ptr<UpdateDiscoveryTaskResultWaiter>>
      initial_discovery_update_waiters_;
};

class IsolatedWebAppPolicyManagerBrowserTest
    : public IsolatedWebAppPolicyManagerBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  IsolatedWebAppPolicyManagerBrowserTest()
      : IsolatedWebAppPolicyManagerBrowserTestBase(GetParam()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
#if !BUILDFLAG(IS_CHROMEOS)
            features::kIsolatedWebApps,
#endif  // BUILDFLAG(IS_CHROMEOS),
            features::kIsolatedWebAppManagedAllowlist},
        /*disabled_features=*/
        {});
  }
  IsolatedWebAppPolicyManagerBrowserTest(
      const IsolatedWebAppPolicyManagerBrowserTest&) = delete;
  IsolatedWebAppPolicyManagerBrowserTest& operator=(
      const IsolatedWebAppPolicyManagerBrowserTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // Override the pre-install component directory and its alternative directory
  // so that the component update will not find the pre-installed key
  // distribution component.
  base::ScopedPathOverride preinstalled_dir_override_{
      component_updater::DIR_COMPONENT_PREINSTALLED};
  base::ScopedPathOverride preinstalled_alt_dir_override_{
      component_updater::DIR_COMPONENT_PREINSTALLED_ALT};
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerBrowserTest,
                       InstallIsolatedWebAppOnLogin) {
  AddUser();
  SetIwaAllowlist({kWebBundleId1});
  InstallOneApp();
  WaitForUserAdded();

  // Log in to the session.
  ASSERT_NO_FATAL_FAILURE(StartLogin({kAppId1}));
  WaitForSessionStart();

  Profile* profile = GetProfileForTest();

  // Wait for the IWA to be installed.
  WebAppTestInstallObserver observer(profile);
  observer.BeginListeningAndWait({kAppId1});

  ASSERT_EQ(provider().registrar_unsafe().GetInstallState(kAppId1),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  EXPECT_EQ(GetIsolatedWebAppVersion(kAppId1).GetString(), "7.0.6");
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerBrowserTest,
                       AppNotInAllowlistNotInstalled) {
  AddUser();
  // Empty the allowlist, so the app install is not allowed.
  SetIwaAllowlist(/*managed_allowlist=*/{});

  EXPECT_FALSE(
      IwaKeyDistributionInfoProvider::GetInstance().IsManagedInstallPermitted(
          kWebBundleId1.id()));

  base::RunLoop run_loop;
  IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
      // We don't use a TestFuture here as the installer may retry the
      // installation and call the callback twice before the parameters can be
      // consumed with `Take`.
      base::BindLambdaForTesting([&](web_package::SignedWebBundleId bundle_id,
                                     IwaInstallerResult install_result) {
        EXPECT_EQ(bundle_id, kWebBundleId1);
        EXPECT_EQ(install_result.type(),
                  IwaInstallerResultType::kErrorAppNotInAllowlist);
        run_loop.Quit();
      }));

  InstallOneApp();
  WaitForUserAdded();

  ASSERT_NO_FATAL_FAILURE(StartLogin({}));
  WaitForSessionStart();

  run_loop.Run();

  EXPECT_THAT(provider().registrar_unsafe().GetAppById(kAppId1),
              testing::IsNull());
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerBrowserTest, PolicyUpdate) {
  AddUser();
  SetIwaAllowlist({kWebBundleId1, kWebBundleId2});
  WaitForUserAdded();

  // Log in in the managed guest session.
  // There no IWA policy set at the moment of login.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  Profile* profile = GetProfileForTest();

  // Set the policy with 1 IWA and wait for the IWA to be installed.
  {
    SetPolicyWithOneApp();
    CreateInitialDiscoveryUpdateWaiters(kAppId1);

    WebAppTestInstallObserver observer(profile);
    observer.BeginListeningAndWait({kAppId1});

    EXPECT_EQ(provider().registrar_unsafe().GetInstallState(kAppId1),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  }

  // Set the policy with 2 IWAs and wait for the IWA to be installed.
  {
    SetPolicyWithTwoApps();
    CreateInitialDiscoveryUpdateWaiters(kAppId2);

    WebAppTestInstallObserver observer2(profile);
    observer2.BeginListeningAndWait({kAppId2});

    EXPECT_EQ(provider().registrar_unsafe().GetInstallState(kAppId2),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  }
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerBrowserTest,
                       InstallUpdateChannelVersion) {
  AddUser();
  SetIwaAllowlist({kWebBundleId1, kWebBundleId2});
  WaitForUserAdded();

  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  Profile* profile = GetProfileForTest();

  // Update channel with higher version than on the "default" channel
  {
    SetPolicyWithBetaChannelApp(kWebBundleId1);
    CreateInitialDiscoveryUpdateWaiters(kAppId1);

    WebAppTestInstallObserver install_observer(profile);
    install_observer.BeginListeningAndWait({kAppId1});

    EXPECT_EQ(GetIsolatedWebAppVersion(kAppId1).GetString(), "9.0.0");
  }

  // Update channel with lower version than on the "default" channel
  {
    SetPolicyWithBetaChannelApp(kWebBundleId2);
    CreateInitialDiscoveryUpdateWaiters(kAppId2);

    WebAppTestInstallObserver install_observer(profile);
    install_observer.BeginListeningAndWait({kAppId2});

    EXPECT_EQ(GetIsolatedWebAppVersion(kAppId2).GetString(), "1.2.0");
  }
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerBrowserTest,
                       InstallIsolatedWebAppAtPinnedVersion) {
  AddUser();
  SetIwaAllowlist({kWebBundleId1});
  WaitForUserAdded();

  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  Profile* profile = GetProfileForTest();

  // Set the policy with pinned IWA and wait for the IWA to be installed.
  SetPolicyWithOneAppWithPinnedVersion();

  WebAppTestInstallObserver observer(profile);
  observer.BeginListeningAndWait({kAppId1});

  ASSERT_EQ(provider().registrar_unsafe().GetInstallState(kAppId1),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  EXPECT_EQ(GetIsolatedWebAppVersion(kAppId1),
            *IwaVersion::Create(kPinnedVersion));
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerBrowserTest,
                       PolicyDeleteAndReinstall) {
  AddUser();
  SetIwaAllowlist({kWebBundleId1, kWebBundleId2});
  WaitForUserAdded();

  // Log in to the managed guest session. There is no IWA policy set at the
  // moment of login.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  // Set the policy with 2 IWAs and wait for the IWAs to be installed.
  {
    WebAppTestInstallObserver install_observer(GetProfileForTest());
    install_observer.BeginListening({kAppId1, kAppId2});

    SetPolicyWithTwoApps();
    CreateInitialDiscoveryUpdateWaiters({kAppId1, kAppId2});
    install_observer.Wait();

    EXPECT_EQ(provider().registrar_unsafe().GetInstallState(kAppId1),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    EXPECT_EQ(provider().registrar_unsafe().GetInstallState(kAppId2),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  }

  // Set the policy with 1 IWA and wait for the unnecessary IWA to be
  // uninstalled.
  {
    // Prepare testing environment for uninstalling.
    base::test::TestFuture<void> uninstall_browsing_data_future;
    auto* browsing_data_remover = GetProfileForTest()->GetBrowsingDataRemover();
    browsing_data_remover->SetWouldCompleteCallbackForTesting(
        base::BindLambdaForTesting([&](base::OnceClosure callback) {
          if (browsing_data_remover->GetPendingTaskCountForTesting() == 1u) {
            uninstall_browsing_data_future.SetValue();
          }
          std::move(callback).Run();
        }));

    WebAppTestUninstallObserver uninstall_observer(GetProfileForTest());
    uninstall_observer.BeginListening({kAppId2});
    SetPolicyWithOneApp();

    EXPECT_TRUE(uninstall_browsing_data_future.Wait());
    EXPECT_EQ(uninstall_observer.Wait(), kAppId2);

    EXPECT_EQ(provider().registrar_unsafe().GetInstallState(kAppId1),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    EXPECT_FALSE(provider().registrar_unsafe().IsInRegistrar(kAppId2));
  }

  // Set the policy with 2 IWAs and wait for the second IWA to be re-installed.
  {
    WebAppTestInstallObserver install_observer(GetProfileForTest());
    install_observer.BeginListening({kAppId2});

    SetPolicyWithTwoApps();
    CreateInitialDiscoveryUpdateWaiters({kAppId2});
    install_observer.Wait();

    EXPECT_EQ(provider().registrar_unsafe().GetInstallState(kAppId1),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    EXPECT_EQ(provider().registrar_unsafe().GetInstallState(kAppId2),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /***/,
    IsolatedWebAppPolicyManagerBrowserTest,
#if BUILDFLAG(IS_CHROMEOS)
    // Controls whether or not to test in a user session (true) or in a managed
    // guest session (false).
    testing::Bool()
#else
    testing::ValuesIn({true})
#endif  // BUILDFLAG(IS_CHROMEOS)
);

class IsolatedWebAppDevToolsTestWithPolicy
    : public IsolatedWebAppPolicyManagerBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, DeveloperToolsPolicyHandler::Availability>> {
 public:
  IsolatedWebAppDevToolsTestWithPolicy()
      : IsolatedWebAppPolicyManagerBrowserTestBase(std::get<bool>(GetParam())) {
  }

  void SetDevToolsAvailability() {
    GetProfileForTest()->GetPrefs()->SetInteger(
        prefs::kDevToolsAvailability,
        base::to_underlying(
            std::get<DeveloperToolsPolicyHandler::Availability>(GetParam())));
  }
  bool AreDevToolsWindowsAllowedByCurrentPolicy() const {
    return std::get<DeveloperToolsPolicyHandler::Availability>(GetParam()) ==
           DeveloperToolsPolicyHandler::Availability::kAllowed;
  }
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDevToolsTestWithPolicy,
                       DisabledForForceInstalledIwas) {
  AddUser();
  SetIwaAllowlist({kWebBundleId1});
  WaitForUserAdded();

  // Log in to the managed guest session. There is no IWA policy set at the
  // moment of login.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  {
    WebAppTestInstallObserver install_observer(GetProfileForTest());
    install_observer.BeginListening({kAppId1});

    SetPolicyWithOneApp();
    CreateInitialDiscoveryUpdateWaiters(kAppId1);
    install_observer.Wait();

    EXPECT_EQ(WebAppProvider::GetForTest(GetProfileForTest())
                  ->registrar_unsafe()
                  .GetInstallState(kAppId1),
              proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
  }

  SetDevToolsAvailability();

  auto* browser =
      web_app::LaunchWebAppBrowserAndWait(GetProfileForTest(), kAppId1);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(!!DevToolsWindowTesting::OpenDevToolsWindowSync(web_contents,
                                                            /*is_docked=*/true),
            AreDevToolsWindowsAllowedByCurrentPolicy());
}

INSTANTIATE_TEST_SUITE_P(
    /***/,
    IsolatedWebAppDevToolsTestWithPolicy,
    testing::Combine(
#if BUILDFLAG(IS_CHROMEOS)
        /*is_user_session=*/testing::Bool(),
#else
        /*is_user_session=*/testing::ValuesIn({true}),
#endif  // BUILDFLAG(IS_CHROMEOS)
        testing::Values(
            DeveloperToolsPolicyHandler::Availability::kAllowed,
            DeveloperToolsPolicyHandler::Availability::
                kDisallowedForForceInstalledExtensions,
            DeveloperToolsPolicyHandler::Availability::kDisallowed)));

class CleanupOrphanedBundlesTest
    : public IsolatedWebAppPolicyManagerBrowserTestBase,
      public ProfileManagerObserver,
      public testing::WithParamInterface<bool> {
 public:
  CleanupOrphanedBundlesTest()
      : IsolatedWebAppPolicyManagerBrowserTestBase(
            /*is_user_session=*/GetParam()) {
    IsolatedWebAppPolicyManager::RemoveDelayForBundleCleanupForTesting();
  }

  void SetUpOnMainThread() override {
    IsolatedWebAppPolicyManagerBrowserTestBase::SetUpOnMainThread();
    profile_manager_observation_.Observe(g_browser_process->profile_manager());
  }

  void TearDownOnMainThread() override {
    IsolatedWebAppPolicyManagerBrowserTestBase::TearDownOnMainThread();
    last_simulate_orphaned_bundle_profile_ = nullptr;
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    Profile* profile = Profile::FromBrowserContext(context);
    last_simulate_orphaned_bundle_profile_ = profile;
    SimulateOrphanedBundle(profile, kOrphanedBundleDirectory);
    ASSERT_TRUE(CheckBundleDirectoryExists(profile, kOrphanedBundleDirectory));
    IsolatedWebAppPolicyManagerBrowserTestBase::
        SetUpBrowserContextKeyedServices(context);
  }

  void SimulateOrphanedBundle(Profile* profile,
                              const std::string& bundle_directory) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto base_path = CHECK_DEREF(profile)
                         .GetPath()
                         .Append(kIwaDirName)
                         .AppendASCII(bundle_directory);
    ASSERT_TRUE(base::CreateDirectory(base_path));
    ASSERT_TRUE(base::WriteFile(
        base_path.Append(FILE_PATH_LITERAL("main.swbn")), "Sample content"));
  }

  bool CheckBundleDirectoryExists(Profile* profile,
                                  const std::string& bundle_directory) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::DirectoryExists(CHECK_DEREF(profile)
                                     .GetPath()
                                     .Append(kIwaDirName)
                                     .AppendASCII(bundle_directory));
  }

  void OnProfileManagerDestroying() override {
    profile_manager_observation_.Reset();
  }

 protected:
  raw_ptr<Profile> last_simulate_orphaned_bundle_profile_ = nullptr;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

IN_PROC_BROWSER_TEST_P(CleanupOrphanedBundlesTest,
                       CleanUpSuccessfulOnSessionStart) {
  AddUser();
  WaitForUserAdded();

  // Login to the session.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  Profile* const profile = GetProfileForTest();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Make sure we simulated the orphaned bundle for the profile we run the
  // cleanup command on.
  EXPECT_EQ(last_simulate_orphaned_bundle_profile_, profile);
  EXPECT_FALSE(CheckBundleDirectoryExists(profile, kOrphanedBundleDirectory));
}

INSTANTIATE_TEST_SUITE_P(
    /***/,
    CleanupOrphanedBundlesTest,
#if BUILDFLAG(IS_CHROMEOS)
    // Is a user session (true) or a managed guest session (false).
    testing::Bool()
#else
    // We only test user sessions outside of ChromeOS.
    testing::ValuesIn({true})
#endif  // BUILDFLAG(IS_CHROMEOS)
);

}  // namespace web_app
