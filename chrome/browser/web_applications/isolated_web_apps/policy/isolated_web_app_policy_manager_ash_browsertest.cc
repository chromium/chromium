// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {

constexpr char kUpdateManifestFileName[] = "update_manifest.json";
constexpr char kIwaBundleFileName[] = "iwa_bundle.swbn";
constexpr char kUpdateManifestTemplate[] = R"(
    {"versions":[
      {"version": "1.0.0", "src": "https://example.com/not-used.swbn"},
      {"version": "7.0.6", "src": "$1"}]
    })";
constexpr char kIsolatedWebAppPolicyTemplate[] = R"([
      {
        "update_manifest_url": "$1",
        "web_bundle_id": "$2"
      }
    ])";
const char kAccountId[] = "dla@example.com";
const char kDisplayName[] = "display name";

}  // namespace

class IsolatedWebAppPolicyManagerAshBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  IsolatedWebAppPolicyManagerAshBrowserTest(
      const IsolatedWebAppPolicyManagerAshBrowserTest&) = delete;
  IsolatedWebAppPolicyManagerAshBrowserTest& operator=(
      const IsolatedWebAppPolicyManagerAshBrowserTest&) = delete;

 protected:
  IsolatedWebAppPolicyManagerAshBrowserTest() = default;

  ~IsolatedWebAppPolicyManagerAshBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Turning on device local account.
    device_policy()->policy_data().set_public_key_version(1);
    policy::DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
        &device_local_account_policy_, kAccountId, kDisplayName);
  }

  void UploadAndInstallDeviceLocalAccountPolicy() {
    // Build device local account policy.
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();

    policy_test_server_mixin_.UpdatePolicy(
        policy::dm_protocol::kChromePublicAccountPolicyType, kAccountId,
        device_local_account_policy_.payload().SerializeAsString());

    session_manager_client()->set_device_local_account_policy(
        kAccountId, device_local_account_policy_.GetBlob());
  }

  void AddManagedGuestSessionToDevicePolicy() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    policy::DeviceLocalAccountTestHelper::AddPublicSession(&proto, kAccountId);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  void AddForceInstallIsolatedWebAppsToAccountPolicy() {
    const std::string swb_id = iwa_bundle_.id.id();
    em::StringPolicyProto* const isolated_web_apps_proto =
        device_local_account_policy_.payload()
            .mutable_isolatedwebappinstallforcelist();

    std::vector<std::string> replacements = {
        iwa_server_.GetURL(base::StrCat({"/", kUpdateManifestFileName})).spec(),
        swb_id};
    const std::string policy_value = base::ReplaceStringPlaceholders(
        kIsolatedWebAppPolicyTemplate, replacements, nullptr);

    isolated_web_apps_proto->set_value(policy_value);
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
    // Start login into the device-local account.
    auto* host = ash::LoginDisplayHost::default_host();
    ASSERT_TRUE(host);
    host->StartSignInScreen();
    auto* controller = ash::ExistingUserController::current_controller();
    ASSERT_TRUE(controller);

    ash::UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                  account_id_);
    controller->Login(user_context, ash::SigninSpecifics());
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
                 base::StringPiece contents) {
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

    const std::vector<std::string> replacements = {
        iwa_server_.GetURL(std::string("/") + kIwaBundleFileName).spec(),
        iwa_bundle_.id.id()};
    const std::string update_manifest_value = base::ReplaceStringPlaceholders(
        kUpdateManifestTemplate, replacements, nullptr);

    WriteFile(kUpdateManifestFileName, update_manifest_value);
    WriteFile(kIwaBundleFileName,
              std::string(iwa_bundle_.data.begin(), iwa_bundle_.data.end()));
  }

  const AccountId account_id_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kAccountId,
          policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION));
  policy::UserPolicyBuilder device_local_account_policy_;
  const web_app::TestSignedWebBundle iwa_bundle_ =
      web_app::TestSignedWebBundleBuilder::BuildDefault(
          TestSignedWebBundleBuilder::BuildOptions().SetVersion(
              base::Version("7.0.6")));

 private:
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer iwa_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPolicyManagerAshBrowserTest,
                       InstallIsolatedWebApp) {
  SetupServer();

  AddManagedGuestSessionToDevicePolicy();
  AddForceInstallIsolatedWebAppsToAccountPolicy();
  UploadAndInstallDeviceLocalAccountPolicy();
  WaitForPolicy();

  // Log in in the managed guest session.
  ASSERT_NO_FATAL_FAILURE(StartLogin());
  WaitForSessionStart();

  Profile* profile = GetProfileForTest();

  // Wait for the IWA to be installed.
  WebAppTestInstallObserver observer(profile);
  const AppId id = observer.BeginListeningAndWait();
  ASSERT_EQ(id,
            IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(iwa_bundle_.id)
                .app_id());
  const WebAppProvider* provider = WebAppProvider::GetForTest(profile);
  ASSERT_TRUE(provider->registrar_unsafe().IsInstalled(id));
}

}  // namespace web_app
