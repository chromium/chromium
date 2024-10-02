// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_switches.h"
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
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_iwa_installer_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {

constexpr char kUpdateManifestFileName1[] = "update_manifest_1.json";
constexpr char kUpdateManifestFileName2[] = "update_manifest_2.json";
constexpr char kUpdateManifestFileName3[] = "update_manifest_3.json";
constexpr char kIwaBundleFileName1[] = "iwa_bundle_1.swbn";
constexpr char kIwaBundleFileName2[] = "iwa_bundle_2.swbn";
constexpr char kIwaBundleFileName3[] = "iwa_bundle_3.swbn";

constexpr char kUpdateManifestTemplate1[] = R"(
    {"versions":[
      {"version": "1.0.0", "src": "https://example.com/not-used.swbn"},
      {"version": "7.0.6", "src": "$1"}]
    })";
constexpr char kUpdateManifestTemplate2[] = R"(
    {"versions":[
      {"version": "2.0.0", "src": "$1"}]
    })";
constexpr char kUpdateManifestTemplate3[] = R"(
    {"versions":[
      {"version": "8.0.0", "src": "$2", "channels":["beta"]},
      {"version": "1.0.0", "src": "https://example.com/not-used.swbn"},
      {"version": "7.0.6", "src": "$1"}]
    })";

constexpr char kDefaultChannel[] = "default";
constexpr char kUserMail[] = "dla@example.com";
constexpr char kDisplayName[] = "display name";

constexpr char kOrphanedBundleDirectory[] = "6zsr4hjoudsu6ihf";

using policy::DeveloperToolsPolicyHandler;

struct IWAForceInstallPolicyEntry {
  web_package::SignedWebBundleId web_bundle_id;
  std::string update_manifest_file_name;
  std::string update_channel_name;
};
}  // namespace

class IsolatedWebAppPolicyManagerAshBrowserTestBase
    : public ash::LoginManagerTest {
 public:
  IsolatedWebAppPolicyManagerAshBrowserTestBase(
      const IsolatedWebAppPolicyManagerAshBrowserTestBase&) = delete;
  IsolatedWebAppPolicyManagerAshBrowserTestBase& operator=(
      const IsolatedWebAppPolicyManagerAshBrowserTestBase&) = delete;

 protected:
  explicit IsolatedWebAppPolicyManagerAshBrowserTestBase(bool is_user_session)
      : is_user_session_(is_user_session) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kIsolatedWebApps};
    if (is_user_session_) {
      login_manager_mixin_.AppendRegularUsers(1);
    } else {
      enabled_features.push_back(
          features::kIsolatedWebAppManagedGuestSessionInstall);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features,
                                          /*disabled_features=*/{});
  }

  ~IsolatedWebAppPolicyManagerAshBrowserTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ash::LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ash::LoginManagerTest::SetUpInProcessBrowserTestFixture();

    if (is_user_session_) {
      policy_provider_.SetDefaultReturns(
          /*is_initialization_complete_return=*/true,
          /*is_first_policy_load_complete_return=*/true);
      policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
          &policy_provider_);
    } else {
      // Turning on device local account.
      device_policy()->policy_data().set_public_key_version(1);
      policy::DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
          &device_local_account_policy_, kUserMail, kDisplayName);
    }
  }

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

  void AddUser(bool set_iwa_policy_on_login = false) {
    if (is_user_session_) {
      // No user needs to be created: for user sessions the user was already
      // added in the constructor (technical constraint).
      if (set_iwa_policy_on_login) {
        SetIWAForceInstallPolicy({IWAForceInstallPolicyEntry{
            iwa_bundle_1_.id, kUpdateManifestFileName1, kDefaultChannel}});
      }
    } else {
      AddManagedGuestSessionToDevicePolicy();
      if (set_iwa_policy_on_login) {
        AddDeviceLocalAccountIwaPolicy();
      }
      UploadAndInstallDeviceLocalAccountPolicy();
      WaitForPolicy();
    }
  }

  void AddManagedGuestSessionToDevicePolicy() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    policy::DeviceLocalAccountTestHelper::AddPublicSession(&proto, kUserMail);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  // This policy is active at the moment of login.
  void AddDeviceLocalAccountIwaPolicy() {
    PolicyGenerator policy_generator;
    policy_generator.AddForceInstalledIwa(
        iwa_bundle_1_.id,
        iwa_server_.GetURL(base::StrCat({"/", kUpdateManifestFileName1})));

    em::StringPolicyProto* const isolated_web_apps_proto =
        device_local_account_policy_.payload()
            .mutable_isolatedwebappinstallforcelist();
    isolated_web_apps_proto->set_value(
        WriteJson(policy_generator.Generate()).value());
  }

  void SetIWAForceInstallPolicy(
      const std::vector<IWAForceInstallPolicyEntry>& update_manifest_entries) {
    PolicyGenerator policy_generator;
    for (const auto& [web_bundle_id, update_manifest_file_name,
                      update_channel_name] : update_manifest_entries) {
      auto channel = UpdateChannel::Create(update_channel_name)
                         .value_or(UpdateChannel::default_channel());
      policy_generator.AddForceInstalledIwa(
          web_bundle_id,
          iwa_server_.GetURL(base::StrCat({"/", update_manifest_file_name})),
          channel);
    }
    if (is_user_session_) {
      policy::PolicyMap policies;
      policies.Set(policy::key::kIsolatedWebAppInstallForceList,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, policy_generator.Generate(),
                   nullptr);
      policy_provider_.UpdateChromePolicy(policies);
    } else {
      GetProfileForTest()->GetPrefs()->Set(
          prefs::kIsolatedWebAppInstallForceList, policy_generator.Generate());
    }
  }

  void SetPolicyWithOneApp() {
    SetIWAForceInstallPolicy({
        IWAForceInstallPolicyEntry{iwa_bundle_1_.id, kUpdateManifestFileName1,
                                   kDefaultChannel},
    });
  }

  void SetPolicyWithOneAppCustomReleaseChannel() {
    SetIWAForceInstallPolicy({
        IWAForceInstallPolicyEntry{iwa_bundle_3_.id, kUpdateManifestFileName3,
                                   "beta"},
    });
  }

  void SetPolicyWithTwoApps() {
    SetIWAForceInstallPolicy({
        IWAForceInstallPolicyEntry{iwa_bundle_1_.id, kUpdateManifestFileName1,
                                   kDefaultChannel},
        IWAForceInstallPolicyEntry{iwa_bundle_2_.id, kUpdateManifestFileName2,
                                   kDefaultChannel},
    });
  }

  // Returns a profile which can be used for testing.
  Profile* GetProfileForTest() {
    // Any profile can be used here since this test does not test multi profile.
    return ProfileManager::GetActiveUserProfile();
  }

  void WaitForPolicy() {
    // Wait for the display name becoming available as that indicates
    // device-local account policy is fully loaded, which is a prerequisite for
    // successful login.
    policy::DictionaryLocalStateValueWaiter("UserDisplayName", kDisplayName,
                                            account_id_.GetUserEmail())
        .Wait();
  }

  void StartLogin() {
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
    }
  }

  void WaitForSessionStart() {
    if (session_manager::SessionManager::Get()->IsSessionStarted()) {
      return;
    }
    if (ash::WizardController::default_controller()) {
      ash::WizardController::default_controller()
          ->SkipPostLoginScreensForTesting();
    }
    ash::test::WaitForPrimaryUserSessionStart();
  }

  void WriteFile(const base::FilePath::StringType& filename,
                 std::string_view contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(
        base::WriteFile(temp_dir_.GetPath().Append(filename), contents));
  }

  void SetupServer() {
    // Set up server that will serve update manifest and the Web Bundle
    // of the IWA.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    iwa_server_.ServeFilesFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(iwa_server_.Start());
    {
      const std::vector<std::string> replacements1 = {
          iwa_server_.GetURL(std::string("/") + kIwaBundleFileName1).spec(),
          iwa_bundle_1_.id.id()};
      const std::string update_manifest_value = base::ReplaceStringPlaceholders(
          kUpdateManifestTemplate1, replacements1, nullptr);

      WriteFile(kUpdateManifestFileName1, update_manifest_value);
      WriteFile(kIwaBundleFileName1, std::string(iwa_bundle_1_.data.begin(),
                                                 iwa_bundle_1_.data.end()));
    }

    {
      const std::vector<std::string> replacements2 = {
          iwa_server_.GetURL(std::string("/") + kIwaBundleFileName2).spec(),
          iwa_bundle_2_.id.id()};
      const std::string update_manifest_value_app2 =
          base::ReplaceStringPlaceholders(kUpdateManifestTemplate2,
                                          replacements2, nullptr);
      WriteFile(kUpdateManifestFileName2, update_manifest_value_app2);
      WriteFile(kIwaBundleFileName2, std::string(iwa_bundle_2_.data.begin(),
                                                 iwa_bundle_2_.data.end()));
    }

    {
      const std::vector<std::string> replacements3 = {
          iwa_server_.GetURL(std::string("/") + kIwaBundleFileName1).spec(),
          iwa_server_.GetURL(std::string("/") + kIwaBundleFileName3).spec(),
          iwa_bundle_3_.id.id(),
      };
      const std::string update_manifest_value_app3 =
          base::ReplaceStringPlaceholders(kUpdateManifestTemplate3,
                                          replacements3, nullptr);

      WriteFile(kUpdateManifestFileName3, update_manifest_value_app3);
      WriteFile(kIwaBundleFileName3, std::string(iwa_bundle_3_.data.begin(),
                                                 iwa_bundle_3_.data.end()));
    }
  }

  void RefreshDevicePolicy() { policy_helper_.RefreshDevicePolicy(); }

  policy::DevicePolicyBuilder* device_policy() {
    return policy_helper_.device_policy();
  }

  ash::FakeSessionManagerClient* session_manager_client() {
    return ash::FakeSessionManagerClient::Get();
  }

  const AccountId account_id_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kUserMail,
          policy::DeviceLocalAccountType::kPublicSession));
  policy::UserPolicyBuilder device_local_account_policy_;
  const web_app::TestSignedWebBundle iwa_bundle_1_ =
      web_app::TestSignedWebBundleBuilder::BuildDefault(
          TestSignedWebBundleBuilder::BuildOptions()
              .SetVersion(base::Version("7.0.6"))
              .AddKeyPair(web_package::test::Ed25519KeyPair::CreateRandom()));
  const web_app::TestSignedWebBundle iwa_bundle_2_ =
      web_app::TestSignedWebBundleBuilder::BuildDefault(
          TestSignedWebBundleBuilder::BuildOptions().SetVersion(
              base::Version("2.0.0")));
  const web_app::TestSignedWebBundle iwa_bundle_3_ =
      web_app::TestSignedWebBundleBuilder::BuildDefault(
          TestSignedWebBundleBuilder::BuildOptions()
              .SetVersion(base::Version("8.0.0"))
              .AddKeyPair(web_package::test::Ed25519KeyPair::CreateRandom()));
  const bool is_user_session_;

 private:
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_};
  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer iwa_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
};

class IsolatedWebAppPolicyManagerAshBrowserTest
    : public IsolatedWebAppPolicyManagerAshBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  IsolatedWebAppPolicyManagerAshBrowserTest()
      : IsolatedWebAppPolicyManagerAshBrowserTestBase(GetParam()) {}
  IsolatedWebAppPolicyManagerAshBrowserTest(
      const IsolatedWebAppPolicyManagerAshBrowserTest&) = delete;
  IsolatedWebAppPolicyManagerAshBrowserTest& operator=(
      const IsolatedWebAppPolicyManagerAshBrowserTest&) = delete;
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerAshBrowserTest,
                       InstallIsolatedWebAppOnLogin) {
  SetupServer();

  AddUser(/*set_iwa_policy_on_login=*/true);

  // Log in in the managed guest session.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  Profile* profile = GetProfileForTest();

  // Wait for the IWA to be installed.
  WebAppTestInstallObserver observer(profile);
  const webapps::AppId id = observer.BeginListeningAndWait();
  EXPECT_EQ(id,
            IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(iwa_bundle_1_.id)
                .app_id());
  const WebAppProvider* provider = WebAppProvider::GetForTest(profile);
  EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id));
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerAshBrowserTest,
                       PolicyUpdate) {
  SetupServer();

  AddUser();

  SetPolicyWithOneApp();

  // Log in in the managed guest session.
  // There no IWA policy set at the moment of login.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  // Set the policy with 1 IWA and wait for the IWA to be installed.
  WebAppTestInstallObserver observer(GetProfileForTest());
  SetPolicyWithOneApp();
  const webapps::AppId id = observer.BeginListeningAndWait();
  ASSERT_EQ(id,
            IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(iwa_bundle_1_.id)
                .app_id());
  const WebAppProvider* provider =
      WebAppProvider::GetForTest(GetProfileForTest());
  EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id));

  // Set the policy with 2 IWAs and wait for the IWA to be installed.
  WebAppTestInstallObserver observer2(GetProfileForTest());
  SetPolicyWithTwoApps();
  const webapps::AppId id2 = observer2.BeginListeningAndWait();
  EXPECT_EQ(id2,
            IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(iwa_bundle_2_.id)
                .app_id());
}
IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerAshBrowserTest,
                       ReleaseChannelSwitchWithHigherVersion) {
  SetupServer();

  AddUser();

  SetPolicyWithOneApp();

  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();
  {
    // Set the policy with 1 IWA and wait for the IWA to be installed.
    WebAppTestInstallObserver observer(GetProfileForTest());
    SetPolicyWithOneApp();
    const webapps::AppId id = observer.BeginListeningAndWait();
    ASSERT_EQ(
        id, IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(iwa_bundle_1_.id)
                .app_id());
    const WebAppProvider* provider =
        WebAppProvider::GetForTest(GetProfileForTest());
    EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id));
  }
  // Set the policy with the same IWA but change the release channel to channel
  // with higher version.
  {
    WebAppTestInstallObserver install_observer(GetProfileForTest());
    SetPolicyWithOneAppCustomReleaseChannel();
    const webapps::AppId id = install_observer.BeginListeningAndWait();

    const WebAppProvider* provider =
        WebAppProvider::GetForTest(GetProfileForTest());

    EXPECT_EQ(provider->registrar_unsafe()
                  .GetAppById(id)
                  ->isolation_data()
                  ->version()
                  .GetString(),
              "8.0.0");
  }
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppPolicyManagerAshBrowserTest,
                       PolicyDeleteAndReinstall) {
  SetupServer();

  AddUser();

  // Log in to the managed guest session. There is no IWA policy set at the
  // moment of login.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  const webapps::AppId id1 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(iwa_bundle_1_.id)
          .app_id();
  const webapps::AppId id2 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(iwa_bundle_2_.id)
          .app_id();

  const WebAppProvider* provider =
      WebAppProvider::GetForTest(GetProfileForTest());

  // Set the policy with 2 IWAs and wait for the IWAs to be installed.
  {
    WebAppTestInstallObserver install_observer(GetProfileForTest());
    install_observer.BeginListening({id1, id2});

    SetPolicyWithTwoApps();
    install_observer.Wait();

    EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id1));
    EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id2));
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
    uninstall_observer.BeginListening({id2});
    SetPolicyWithOneApp();

    EXPECT_TRUE(uninstall_browsing_data_future.Wait());
    EXPECT_EQ(uninstall_observer.Wait(), id2);

    EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id1));
    EXPECT_FALSE(provider->registrar_unsafe().IsInstalled(id2));
  }

  // Set the policy with 2 IWAs and wait for the second IWA to be re-installed.
  {
    WebAppTestInstallObserver install_observer(GetProfileForTest());
    install_observer.BeginListening({id2});

    SetPolicyWithTwoApps();
    install_observer.Wait();

    EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id1));
    EXPECT_TRUE(provider->registrar_unsafe().IsInstalled(id2));
  }
}

INSTANTIATE_TEST_SUITE_P(
    /***/,
    IsolatedWebAppPolicyManagerAshBrowserTest,
    // Controls whether or not to test in a user session (true) or in a managed
    // guest session (false).
    testing::Bool());

class IsolatedWebAppDevToolsTestWithPolicy
    : public IsolatedWebAppPolicyManagerAshBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, DeveloperToolsPolicyHandler::Availability>> {
 public:
  IsolatedWebAppDevToolsTestWithPolicy()
      : IsolatedWebAppPolicyManagerAshBrowserTestBase(
            std::get<bool>(GetParam())) {}

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
  SetupServer();

  AddUser();

  // Log in to the managed guest session. There is no IWA policy set at the
  // moment of login.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  const webapps::AppId id1 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(iwa_bundle_1_.id)
          .app_id();
  {
    WebAppTestInstallObserver install_observer(GetProfileForTest());
    install_observer.BeginListening({id1});

    SetPolicyWithOneApp();
    install_observer.Wait();

    EXPECT_TRUE(WebAppProvider::GetForTest(GetProfileForTest())
                    ->registrar_unsafe()
                    .IsInstalled(id1));
  }

  SetDevToolsAvailability();

  auto* browser = web_app::LaunchWebAppBrowserAndWait(GetProfileForTest(), id1);
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
        testing::Bool(),
        testing::Values(
            DeveloperToolsPolicyHandler::Availability::kAllowed,
            DeveloperToolsPolicyHandler::Availability::
                kDisallowedForForceInstalledExtensions,
            DeveloperToolsPolicyHandler::Availability::kDisallowed)));

class CleanupOrphanedBundlesTest
    : public IsolatedWebAppPolicyManagerAshBrowserTestBase,
      public ProfileManagerObserver,
      public testing::WithParamInterface<bool> {
 public:
  CleanupOrphanedBundlesTest()
      : IsolatedWebAppPolicyManagerAshBrowserTestBase(
            /*is_user_session=*/GetParam()) {}

  void SetUpOnMainThread() override {
    iwa_installer_factory_.SetUp(GetProfileForTest());
    IsolatedWebAppPolicyManagerAshBrowserTestBase::SetUpOnMainThread();
    profile_manager_observation_.Observe(g_browser_process->profile_manager());
  }

  void TearDownOnMainThread() override {
    last_simulate_orphaned_bundle_profile_ = nullptr;
  }

  void SimulateOrphanedBundle(Profile* profile,
                              const std::string& bundle_directory) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto base_path = CHECK_DEREF(profile)
                         .GetPath()
                         .Append(kIwaDirName)
                         .Append(bundle_directory);
    ASSERT_TRUE(base::CreateDirectory(base_path));
    ASSERT_TRUE(
        base::WriteFile(base_path.Append("main.swbn"), "Sample content"));
  }

  bool CheckBundleDirectoryExists(Profile* profile,
                                  const std::string& bundle_directory) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::DirectoryExists(CHECK_DEREF(profile)
                                     .GetPath()
                                     .Append(kIwaDirName)
                                     .Append(bundle_directory));
  }

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    last_simulate_orphaned_bundle_profile_ = profile;
    SimulateOrphanedBundle(profile, kOrphanedBundleDirectory);
    ASSERT_TRUE(CheckBundleDirectoryExists(profile, kOrphanedBundleDirectory));
  }

  void OnProfileManagerDestroying() override {
    profile_manager_observation_.Reset();
  }

 protected:
  TestIwaInstallerFactory iwa_installer_factory_;
  raw_ptr<Profile> last_simulate_orphaned_bundle_profile_ = nullptr;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CleanupOrphanedBundlesTest,
                       CleanUpSuccessfulOnSessionStart) {
  SetupServer();

  AddUser(/*set_iwa_policy_on_login=*/false);

  // Login to the session.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  Profile* const profile = GetProfileForTest();
  WebAppProvider::GetForTest(profile)
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();

  // Make sure we simulated the orphaned bundle for the profile we run the
  // cleanup command on.
  EXPECT_EQ(last_simulate_orphaned_bundle_profile_, profile);
  EXPECT_FALSE(CheckBundleDirectoryExists(profile, kOrphanedBundleDirectory));
}

IN_PROC_BROWSER_TEST_P(CleanupOrphanedBundlesTest,
                       CleanUpSuccessfulOnFailedInstall) {
  SetupServer();

  base::test::TestFuture<void> future;
  iwa_installer_factory_.SetInstallCompletedClosure(
      future.GetRepeatingCallback());

  AddUser(/*set_iwa_policy_on_login=*/false);

  // Login to the session.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  Profile* const profile = GetProfileForTest();
  WebAppProvider* provider = WebAppProvider::GetForTest(profile);
  WebAppCommandManager& command_manager = provider->command_manager();
  command_manager.AwaitAllCommandsCompleteForTesting();

  SimulateOrphanedBundle(profile, kOrphanedBundleDirectory);
  ASSERT_TRUE(CheckBundleDirectoryExists(profile, kOrphanedBundleDirectory));

  // Try to install an isolated web app, which should fail. This should trigger
  // a cleanup.
  iwa_installer_factory_.SetCommandBehavior(
      iwa_bundle_1_.id.id(), /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kSimulateFailure,
      /*execute_immediately=*/true);
  SetPolicyWithOneApp();
  ASSERT_TRUE(future.Wait());
  webapps::AppId id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(iwa_bundle_1_.id)
          .app_id();
  EXPECT_FALSE(provider->registrar_unsafe().IsInstalled(id));

  // Wait until the cleanup is done.
  command_manager.AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(0u, command_manager.GetCommandCountForTesting());
  EXPECT_FALSE(CheckBundleDirectoryExists(profile, kOrphanedBundleDirectory));
}

INSTANTIATE_TEST_SUITE_P(
    /***/,
    CleanupOrphanedBundlesTest,
    // Is a user session (true) or a managed guest session (false).
    testing::Bool());

}  // namespace web_app
